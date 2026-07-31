#!/usr/bin/env python3
"""Verify that overlay telemetry cannot hold the single HTTP worker idle."""

from __future__ import annotations

import argparse
import json
import socket
import time


def read_response(connection: socket.socket) -> tuple[str, dict[str, str], bytes]:
    data = bytearray()
    while b"\r\n\r\n" not in data:
        chunk = connection.recv(4096)
        if not chunk:
            raise RuntimeError("connection closed before the HTTP header completed")
        data.extend(chunk)
        if len(data) > 64 * 1024:
            raise RuntimeError("HTTP header exceeded 64 KiB")

    header_bytes, body = bytes(data).split(b"\r\n\r\n", 1)
    lines = header_bytes.decode("iso-8859-1").split("\r\n")
    headers: dict[str, str] = {}
    for line in lines[1:]:
        if ":" in line:
            name, value = line.split(":", 1)
            headers[name.strip().lower()] = value.strip().lower()

    length = int(headers.get("content-length", "0"))
    payload = bytearray(body)
    while len(payload) < length:
        chunk = connection.recv(min(4096, length - len(payload)))
        if not chunk:
            raise RuntimeError("connection closed before the HTTP body completed")
        payload.extend(chunk)
    return lines[0], headers, bytes(payload[:length])


def send_get(connection: socket.socket, port: int, path: str, connection_header: str) -> None:
    request = (
        f"GET {path} HTTP/1.1\r\n"
        f"Host: 127.0.0.1:{port}\r\n"
        f"Connection: {connection_header}\r\n"
        "Cache-Control: no-store\r\n\r\n"
    )
    connection.sendall(request.encode("ascii"))


def connect_when_ready(address: tuple[str, int], deadline_ms: int) -> socket.socket:
    deadline = time.perf_counter() + deadline_ms / 1000.0
    last_error: OSError | None = None
    while time.perf_counter() < deadline:
        try:
            return socket.create_connection(address, timeout=1.0)
        except OSError as error:
            last_error = error
            time.sleep(0.05)
    raise RuntimeError(f"overlay did not accept connections within {deadline_ms} ms: {last_error}")


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--port", type=int, default=18765)
    parser.add_argument("--deadline-ms", type=int, default=1000)
    parser.add_argument("--connect-deadline-ms", type=int, default=5000)
    args = parser.parse_args()

    address = ("127.0.0.1", args.port)
    with connect_when_ready(address, args.connect_deadline_ms) as telemetry:
        telemetry.settimeout(1.0)
        send_get(telemetry, args.port, "/client_perf?frames=1", "keep-alive")
        status, headers, _ = read_response(telemetry)
        if " 204 " not in status:
            raise SystemExit(f"FAIL: client telemetry returned {status}")
        if headers.get("connection") != "close":
            raise SystemExit("FAIL: client telemetry did not advertise Connection: close")
        if telemetry.recv(1) != b"":
            raise SystemExit("FAIL: client telemetry connection remained open")

    started = time.perf_counter()
    with socket.create_connection(address, timeout=1.0) as state:
        state.settimeout(1.0)
        send_get(state, args.port, "/state", "close")
        status, _, body = read_response(state)
    elapsed_ms = (time.perf_counter() - started) * 1000.0

    if " 200 " not in status:
        raise SystemExit(f"FAIL: state request returned {status}")
    parsed = json.loads(body.decode("utf-8"))
    if "tick" not in parsed or "keys" not in parsed:
        raise SystemExit("FAIL: state response is missing required fields")
    if elapsed_ms > args.deadline_ms:
        raise SystemExit(
            f"FAIL: state request took {elapsed_ms:.1f} ms after client telemetry "
            f"(deadline {args.deadline_ms} ms)"
        )

    print(f"OVERLAY_HTTP_RESPONSIVENESS=PASS elapsed_ms={elapsed_ms:.1f}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
