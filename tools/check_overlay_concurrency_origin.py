#!/usr/bin/env python3
"""Exercise HallJoy's bounded multi-client, origin and shutdown contracts."""

from __future__ import annotations

import argparse
import concurrent.futures
import json
from pathlib import Path
import select
import socket
import time


class ResponseReader:
    def __init__(self, connection: socket.socket) -> None:
        self.connection = connection
        self.buffer = bytearray()

    def read(self) -> tuple[str, dict[str, str], bytes]:
        while b"\r\n\r\n" not in self.buffer:
            chunk = self.connection.recv(4096)
            if not chunk:
                raise RuntimeError("connection closed before response headers completed")
            self.buffer.extend(chunk)
            if len(self.buffer) > 64 * 1024:
                raise RuntimeError("response headers exceeded 64 KiB")
        header_end = self.buffer.index(b"\r\n\r\n")
        header = bytes(self.buffer[:header_end]).decode("iso-8859-1")
        del self.buffer[:header_end + 4]
        lines = header.split("\r\n")
        headers: dict[str, str] = {}
        for line in lines[1:]:
            if ":" in line:
                name, value = line.split(":", 1)
                headers[name.strip().lower()] = value.strip()
        length = int(headers.get("content-length", "0"))
        while len(self.buffer) < length:
            chunk = self.connection.recv(4096)
            if not chunk:
                raise RuntimeError("connection closed before response body completed")
            self.buffer.extend(chunk)
        body = bytes(self.buffer[:length])
        del self.buffer[:length]
        return lines[0], headers, body


def connect(address: tuple[str, int], timeout: float = 1.0) -> socket.socket:
    connection = socket.create_connection(address, timeout=timeout)
    connection.settimeout(timeout)
    return connection


def wait_until_ready(address: tuple[str, int], deadline_ms: int) -> None:
    deadline = time.perf_counter() + deadline_ms / 1000.0
    last_error: OSError | None = None
    while time.perf_counter() < deadline:
        try:
            with connect(address):
                return
        except OSError as error:
            last_error = error
            time.sleep(0.05)
    raise RuntimeError(f"overlay did not accept connections within {deadline_ms} ms: {last_error}")


def request(port: int, path: str, cookie: str = "", origin: str = "") -> bytes:
    lines = [
        f"GET {path} HTTP/1.1",
        f"Host: 127.0.0.1:{port}",
        "Connection: close",
    ]
    if cookie:
        lines.append(f"Cookie: {cookie}")
    if origin:
        lines.append(f"Origin: {origin}")
    return ("\r\n".join(lines) + "\r\n\r\n").encode("ascii")


def transact(
    address: tuple[str, int], payload: bytes, timeout: float = 1.0
) -> tuple[str, dict[str, str], bytes, float]:
    started = time.perf_counter()
    with connect(address, timeout) as connection:
        connection.sendall(payload)
        status, headers, body = ResponseReader(connection).read()
    return status, headers, body, (time.perf_counter() - started) * 1000.0


def bootstrap(address: tuple[str, int], port: int) -> str:
    status, headers, _, _ = transact(address, request(port, "/"))
    if " 200 " not in status:
        raise RuntimeError(f"session bootstrap returned {status}")
    if "access-control-allow-origin" in headers:
        raise RuntimeError("origin-less bootstrap unexpectedly emitted CORS access")
    cookie = headers.get("set-cookie", "").split(";", 1)[0]
    if not cookie.startswith("HallJoySession=") or len(cookie) != 47:
        raise RuntimeError("bootstrap did not issue a 128-bit session cookie")
    cookie_attributes = headers["set-cookie"].lower()
    if "httponly" not in cookie_attributes or "samesite=strict" not in cookie_attributes:
        raise RuntimeError("session cookie is missing HttpOnly or SameSite=Strict")
    return cookie


def expect_code(status: str, code: int) -> None:
    if f" {code} " not in status:
        raise RuntimeError(f"expected HTTP {code}, received {status}")


def open_slow_clients(
    address: tuple[str, int], port: int, count: int, cookie: str = ""
) -> list[socket.socket]:
    clients: list[socket.socket] = []
    try:
        for _ in range(count):
            connection = connect(address, 2.0)
            partial = (
                f"GET /state HTTP/1.1\r\nHost: 127.0.0.1:{port}\r\n"
                + (f"Cookie: {cookie}\r\n" if cookie else "")
            )
            connection.sendall(partial.encode("ascii"))
            clients.append(connection)
            time.sleep(0.035)
        return clients
    except Exception:
        for connection in clients:
            connection.close()
        raise


def close_all(clients: list[socket.socket]) -> None:
    for connection in clients:
        connection.close()


def wait_for_state(address: tuple[str, int], port: int, cookie: str) -> None:
    deadline = time.perf_counter() + 3.0
    last_error: Exception | None = None
    while time.perf_counter() < deadline:
        try:
            status, _, body, _ = transact(address, request(port, "/state", cookie))
            expect_code(status, 200)
            if "tick" not in json.loads(body.decode("utf-8")):
                raise RuntimeError("state JSON is missing tick")
            return
        except (OSError, RuntimeError, ValueError, json.JSONDecodeError) as error:
            last_error = error
            time.sleep(0.05)
    raise RuntimeError(f"overlay did not recover after slow clients closed: {last_error}")


def run_origin_and_concurrency(address: tuple[str, int], port: int, deadline_ms: int) -> None:
    print("[overlay-concurrency] phase=session-bootstrap", flush=True)
    cookie = bootstrap(address, port)
    own_origin = f"http://127.0.0.1:{port}"

    print("[overlay-concurrency] phase=authorization", flush=True)
    status, headers, _, _ = transact(address, request(port, "/state"))
    expect_code(status, 401)
    if "access-control-allow-origin" in headers:
        raise RuntimeError("unauthenticated origin-less state response emitted CORS access")

    stale_cookie = "HallJoySession=" + "0" * 32
    status, headers, _, _ = transact(
        address, request(port, "/state", stale_cookie, own_origin)
    )
    expect_code(status, 401)
    if (headers.get("access-control-allow-origin") != own_origin or
            "set-cookie" in headers):
        raise RuntimeError("stale session rejection did not preserve the safe origin boundary")

    status, headers, body, _ = transact(address, request(port, "/state", cookie))
    expect_code(status, 200)
    if "access-control-allow-origin" in headers:
        raise RuntimeError("origin-less authenticated state response emitted CORS access")
    if "keys" not in json.loads(body.decode("utf-8")):
        raise RuntimeError("authenticated state response is missing keys")

    status, headers, _, _ = transact(address, request(port, "/state", cookie, own_origin))
    expect_code(status, 200)
    if (headers.get("access-control-allow-origin") != own_origin or
            headers.get("vary", "").lower() != "origin"):
        raise RuntimeError("same-origin response did not echo the exact origin with Vary")

    print("[overlay-concurrency] phase=hostile-origins", flush=True)
    for hostile_origin in ("https://example.invalid", "null"):
        status, headers, _, _ = transact(
            address, request(port, "/state", cookie, hostile_origin)
        )
        expect_code(status, 403)
        if "access-control-allow-origin" in headers or "set-cookie" in headers:
            raise RuntimeError("forbidden origin received CORS access or a session cookie")

    status, headers, _, _ = transact(
        address, request(port, "/", origin="https://example.invalid")
    )
    expect_code(status, 403)
    if "set-cookie" in headers:
        raise RuntimeError("hostile origin obtained a session cookie")

    print("[overlay-concurrency] phase=parallel-under-slow-clients", flush=True)
    slow = open_slow_clients(address, port, 8, cookie)
    try:
        def state_once(_: int) -> float:
            status, _, body, elapsed_ms = transact(
                address, request(port, "/state", cookie, own_origin), 2.0
            )
            expect_code(status, 200)
            json.loads(body.decode("utf-8"))
            return elapsed_ms

        with concurrent.futures.ThreadPoolExecutor(max_workers=8) as executor:
            latencies = list(executor.map(state_once, range(8)))
        if max(latencies) > deadline_ms:
            raise RuntimeError(
                f"parallel /state latency {max(latencies):.1f} ms exceeded {deadline_ms} ms"
            )
    finally:
        close_all(slow)
    print("[overlay-concurrency] phase=parallel-recovery", flush=True)
    wait_for_state(address, port, cookie)

    print("[overlay-concurrency] phase=client-limit", flush=True)
    saturated = open_slow_clients(address, port, 16, cookie)
    try:
        print("[overlay-concurrency] phase=client-limit-probe", flush=True)
        time.sleep(0.15)
        rejection_started = time.perf_counter()
        rejection = "503"
        try:
            status, headers, _, elapsed_ms = transact(
                address, request(port, "/state", cookie), 2.0
            )
            expect_code(status, 503)
            if headers.get("connection", "").lower() != "close":
                raise RuntimeError("client-limit 503 did not require connection close")
        except ConnectionResetError:
            # Winsock may reset a saturated connection because the accept worker
            # intentionally does not block to consume attacker-controlled input.
            elapsed_ms = (time.perf_counter() - rejection_started) * 1000.0
            rejection = "reset"
        if elapsed_ms > deadline_ms:
            raise RuntimeError("client-limit rejection was not prompt")
    finally:
        close_all(saturated)
    print("[overlay-concurrency] phase=client-limit-recovery", flush=True)
    wait_for_state(address, port, cookie)

    print(
        "OVERLAY_CONCURRENCY_ORIGIN=PASS "
        f"slow_clients=8 parallel_state=8 max_latency_ms={max(latencies):.1f} "
        f"client_limit=16 rejection={rejection} hostile_origins=2 session_cookie=1"
    )


def run_shutdown_probe(
    address: tuple[str, int], port: int, count: int, ready_file: Path, deadline_ms: int
) -> None:
    clients = open_slow_clients(address, port, count)
    try:
        time.sleep(0.2)
        ready_file.write_text(f"ready clients={count}\n", encoding="utf-8")
        deadline = time.perf_counter() + deadline_ms / 1000.0
        active = set(clients)
        heartbeat = 0
        next_heartbeat = time.perf_counter() + 2.0
        while active and time.perf_counter() < deadline:
            now = time.perf_counter()
            if now >= next_heartbeat:
                heartbeat += 1
                payload = f"X-HallJoy-Probe: {heartbeat}\r\n".encode("ascii")
                for connection in list(active):
                    try:
                        connection.sendall(payload)
                    except (ConnectionResetError, ConnectionAbortedError, BrokenPipeError):
                        active.discard(connection)
                next_heartbeat = now + 2.0
            readable, _, _ = select.select(
                list(active), [], [], min(0.25, max(0.0, deadline - now))
            )
            for connection in readable:
                try:
                    if connection.recv(1) == b"":
                        active.discard(connection)
                except (ConnectionResetError, ConnectionAbortedError):
                    active.discard(connection)
        closed = count - len(active)
        if closed != count:
            raise RuntimeError(f"server shutdown closed only {closed}/{count} active clients")
    finally:
        close_all(clients)
    print(f"OVERLAY_ACTIVE_CLIENT_SHUTDOWN=PASS clients={count}")


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--port", type=int, default=18765)
    parser.add_argument("--connect-deadline-ms", type=int, default=5000)
    parser.add_argument("--deadline-ms", type=int, default=1000)
    parser.add_argument("--shutdown-probe-clients", type=int, default=0)
    parser.add_argument("--ready-file", type=Path)
    args = parser.parse_args()
    address = ("127.0.0.1", args.port)
    wait_until_ready(address, args.connect_deadline_ms)

    if args.shutdown_probe_clients:
        if not args.ready_file:
            raise RuntimeError("--ready-file is required for a shutdown probe")
        run_shutdown_probe(
            address,
            args.port,
            args.shutdown_probe_clients,
            args.ready_file,
            max(10000, args.deadline_ms),
        )
    else:
        run_origin_and_concurrency(address, args.port, args.deadline_ms)
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except (OSError, RuntimeError, ValueError, json.JSONDecodeError) as error:
        raise SystemExit(f"FAIL: {error}")
