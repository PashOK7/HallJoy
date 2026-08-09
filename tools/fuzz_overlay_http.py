#!/usr/bin/env python3
"""Deterministically stress HallJoy's loopback HTTP parser with hostile frames."""

from __future__ import annotations

import argparse
import concurrent.futures
import json
import random
import socket
import time


def connect(address: tuple[str, int], timeout: float = 0.5) -> socket.socket:
    connection = socket.create_connection(address, timeout=timeout)
    connection.settimeout(timeout)
    return connection


def read_response(connection: socket.socket) -> tuple[str, dict[str, str], bytes]:
    data = bytearray()
    while b"\r\n\r\n" not in data:
        chunk = connection.recv(4096)
        if not chunk:
            raise RuntimeError("response ended before headers")
        data.extend(chunk)
        if len(data) > 65536:
            raise RuntimeError("response headers exceeded 64 KiB")
    header, body = bytes(data).split(b"\r\n\r\n", 1)
    lines = header.decode("iso-8859-1").split("\r\n")
    headers: dict[str, str] = {}
    for line in lines[1:]:
        if ":" in line:
            name, value = line.split(":", 1)
            headers[name.strip().lower()] = value.strip()
    length = int(headers.get("content-length", "0"))
    payload = bytearray(body)
    while len(payload) < length:
        chunk = connection.recv(min(4096, length - len(payload)))
        if not chunk:
            raise RuntimeError("response ended before body")
        payload.extend(chunk)
    return lines[0], headers, bytes(payload[:length])


def request(port: int, path: str, cookie: str = "") -> bytes:
    cookie_header = f"Cookie: {cookie}\r\n" if cookie else ""
    return (
        f"GET {path} HTTP/1.1\r\n"
        f"Host: 127.0.0.1:{port}\r\n"
        f"{cookie_header}Connection: close\r\n\r\n"
    ).encode("ascii")


def transact(address: tuple[str, int], payload: bytes) -> tuple[str, dict[str, str], bytes]:
    with connect(address, 1.0) as connection:
        connection.sendall(payload)
        return read_response(connection)


def wait_and_bootstrap(address: tuple[str, int], port: int, deadline_ms: int) -> str:
    deadline = time.perf_counter() + deadline_ms / 1000.0
    last_error: Exception | None = None
    while time.perf_counter() < deadline:
        try:
            status, headers, _ = transact(address, request(port, "/"))
            if " 200 " not in status:
                raise RuntimeError(status)
            cookie = headers.get("set-cookie", "").split(";", 1)[0]
            if not cookie.startswith("HallJoySession=") or len(cookie) != 47:
                raise RuntimeError("invalid session cookie")
            return cookie
        except (OSError, RuntimeError, ValueError) as error:
            last_error = error
            time.sleep(0.05)
    raise RuntimeError(f"overlay bootstrap failed within {deadline_ms} ms: {last_error}")


def assert_healthy(address: tuple[str, int], port: int, cookie: str) -> None:
    status, _, body = transact(address, request(port, "/state", cookie))
    if " 200 " not in status:
        raise RuntimeError(f"health probe returned {status}")
    parsed = json.loads(body.decode("utf-8"))
    if "tick" not in parsed or "keys" not in parsed:
        raise RuntimeError("health probe JSON is incomplete")


def make_case(index: int, port: int, cookie: str) -> tuple[bytes, bool]:
    rng = random.Random(0x48414C4C4A4F59 ^ index)
    kind = index % 8
    valid = bytearray(request(port, "/state", cookie))
    if kind == 0:
        for _ in range(1 + rng.randrange(12)):
            at = rng.randrange(len(valid))
            valid[at] ^= 1 << rng.randrange(8)
        return bytes(valid), True
    if kind == 1:
        return (
            b"GET /state HTTP/1.1\r\nHost: x\r\nX-Fuzz: "
            + bytes(rng.randrange(32, 127) for _ in range(7000 + rng.randrange(3000)))
            + b"\r\n\r\n",
            False,
        )
    if kind == 2:
        return bytes(rng.randrange(256) for _ in range(rng.randrange(1, 2049))) + b"\r\n\r\n", True
    if kind == 3:
        value = rng.choice((b"-1", b"+1", b"1x", b"999999999999999999999999999"))
        return b"GET /state HTTP/1.1\r\nHost: x\r\nContent-Length: " + value + b"\r\n\r\n", True
    if kind == 4:
        return request(port, "/state", cookie) * (2 + rng.randrange(6)), True
    if kind == 5:
        return bytes(valid), True
    if kind == 6:
        return b"GET /state HTTP/1.1\nHost: x\nConnection: close\n\n" + bytes(rng.randrange(256) for _ in range(64)), True
    return (
        b"GET /state HTTP/1.1\r\nHost: x\r\n"
        + b"Origin: https://hostile.invalid\r\n" * (1 + rng.randrange(4))
        + b"Cookie: HallJoySession=" + bytes(rng.choice(b"0123456789abcdef") for _ in range(96))
        + b"\r\nTransfer-Encoding: chunked\r\nContent-Length: 3\r\n\r\nXYZ",
        True,
    )


def exercise_one(address: tuple[str, int], port: int, cookie: str, index: int) -> str:
    payload, fragmented = make_case(index, port, cookie)
    rng = random.Random(0x465241474D454E54 ^ index)
    try:
        with connect(address) as connection:
            if fragmented:
                at = 0
                while at < len(payload):
                    step = min(len(payload) - at, 1 + rng.randrange(97))
                    connection.sendall(payload[at:at + step])
                    at += step
            else:
                connection.sendall(payload)
            connection.shutdown(socket.SHUT_WR)
            received = 0
            while received < 65536:
                chunk = connection.recv(min(4096, 65536 - received))
                if not chunk:
                    break
                received += len(chunk)
            return "response" if received else "closed"
    except socket.timeout:
        return "timeout"
    except (ConnectionResetError, ConnectionAbortedError, BrokenPipeError):
        return "reset"


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--port", type=int, default=18765)
    parser.add_argument("--iterations", type=int, default=2000)
    parser.add_argument("--workers", type=int, default=8)
    parser.add_argument("--connect-deadline-ms", type=int, default=5000)
    args = parser.parse_args()
    if args.iterations < 1 or args.workers < 1 or args.workers > 12:
        raise RuntimeError("iterations must be positive and workers must be in 1..12")

    address = ("127.0.0.1", args.port)
    cookie = wait_and_bootstrap(address, args.port, args.connect_deadline_ms)
    totals = {"response": 0, "closed": 0, "reset": 0, "timeout": 0}
    batch_size = 200
    for first in range(0, args.iterations, batch_size):
        last = min(first + batch_size, args.iterations)
        with concurrent.futures.ThreadPoolExecutor(max_workers=args.workers) as executor:
            outcomes = executor.map(
                lambda index: exercise_one(address, args.port, cookie, index),
                range(first, last),
            )
            for outcome in outcomes:
                totals[outcome] += 1
        assert_healthy(address, args.port, cookie)

    print(
        "OVERLAY_HTTP_FUZZ=PASS "
        f"iterations={args.iterations} workers={args.workers} "
        + " ".join(f"{name}={count}" for name, count in totals.items())
    )
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except (OSError, RuntimeError, ValueError, json.JSONDecodeError) as error:
        raise SystemExit(f"FAIL: {error}")
