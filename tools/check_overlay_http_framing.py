#!/usr/bin/env python3
"""Exercise HallJoy's bounded loopback HTTP framing contract."""

from __future__ import annotations

import argparse
import json
import socket
import time


class ResponseReader:
    def __init__(self, connection: socket.socket) -> None:
        self.connection = connection
        self.buffer = bytearray()

    def read(self) -> tuple[str, dict[str, str], bytes]:
        while b"\r\n\r\n" not in self.buffer:
            self._receive()
            if len(self.buffer) > 64 * 1024:
                raise RuntimeError("response header exceeded 64 KiB")

        header_end = self.buffer.index(b"\r\n\r\n")
        header_bytes = bytes(self.buffer[:header_end])
        del self.buffer[:header_end + 4]
        lines = header_bytes.decode("iso-8859-1").split("\r\n")
        headers: dict[str, str] = {}
        for line in lines[1:]:
            if ":" in line:
                name, value = line.split(":", 1)
                headers[name.strip().lower()] = value.strip().lower()
        length = int(headers.get("content-length", "0"))
        while len(self.buffer) < length:
            self._receive()
        body = bytes(self.buffer[:length])
        del self.buffer[:length]
        return lines[0], headers, body

    def _receive(self) -> None:
        chunk = self.connection.recv(4096)
        if not chunk:
            raise RuntimeError("connection closed before the HTTP response completed")
        self.buffer.extend(chunk)


def request(port: int, path: str, connection: str = "close", body: bytes = b"") -> bytes:
    return (
        f"GET {path} HTTP/1.1\r\n"
        f"Host: 127.0.0.1:{port}\r\n"
        f"Connection: {connection}\r\n"
        f"Content-Length: {len(body)}\r\n\r\n"
    ).encode("ascii") + body


def connect(address: tuple[str, int]) -> socket.socket:
    connection = socket.create_connection(address, timeout=1.0)
    connection.settimeout(1.0)
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


def expect_status(address: tuple[str, int], payload: bytes, code: int) -> bytes:
    with connect(address) as connection:
        connection.sendall(payload)
        status, headers, body = ResponseReader(connection).read()
        if f" {code} " not in status:
            raise RuntimeError(f"expected HTTP {code}, received {status}")
        if code >= 400 and headers.get("connection") != "close":
            raise RuntimeError(f"HTTP {code} did not close the rejected request")
        if code >= 400 and connection.recv(1) != b"":
            raise RuntimeError(f"HTTP {code} kept the rejected connection open")
        return body


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--port", type=int, default=18765)
    parser.add_argument("--connect-deadline-ms", type=int, default=5000)
    args = parser.parse_args()
    address = ("127.0.0.1", args.port)
    wait_until_ready(address, args.connect_deadline_ms)

    # A request line and headers may span arbitrary recv() boundaries.
    fragmented = request(args.port, "/state")
    with connect(address) as connection:
        previous = 0
        for offset in (1, 7, 19, len(fragmented)):
            connection.sendall(fragmented[previous:offset])
            previous = offset
        status, _, body = ResponseReader(connection).read()
        if " 200 " not in status or "tick" not in json.loads(body.decode("utf-8")):
            raise RuntimeError("fragmented /state request did not produce valid JSON")

    # Two complete requests delivered by one send must produce two framed responses.
    with connect(address) as connection:
        connection.sendall(
            request(args.port, "/state", "keep-alive") + request(args.port, "/state")
        )
        reader = ResponseReader(connection)
        first = reader.read()
        second = reader.read()
        if " 200 " not in first[0] or " 200 " not in second[0]:
            raise RuntimeError("pipelined /state requests were not both served")

    # Content-Length bytes belong to the first frame, even when body and next request coalesce.
    first_request = request(args.port, "/state", "keep-alive", b"TEST")
    second_request = request(args.port, "/state")
    split = len(first_request) - 2
    with connect(address) as connection:
        connection.sendall(first_request[:split])
        connection.sendall(first_request[split:] + second_request)
        reader = ResponseReader(connection)
        if " 200 " not in reader.read()[0] or " 200 " not in reader.read()[0]:
            raise RuntimeError("request body was not consumed as part of its exact frame")

    oversized_header = (
        f"GET /state HTTP/1.1\r\nHost: 127.0.0.1:{args.port}\r\nX-Large: "
        + "a" * 8200
        + "\r\n\r\n"
    ).encode("ascii")
    expect_status(address, oversized_header, 431)
    expect_status(
        address,
        f"GET /state HTTP/1.1\r\nHost: x\r\nContent-Length: 4097\r\n\r\n".encode("ascii"),
        413,
    )
    expect_status(
        address,
        b"GET /state HTTP/1.1\r\nHost: x\r\nContent-Length: 0\r\nContent-Length: 0\r\n\r\n",
        400,
    )
    expect_status(
        address,
        b"GET /state HTTP/1.1\r\nHost: x\r\nTransfer-Encoding: chunked\r\n\r\n",
        400,
    )
    expect_status(
        address,
        b"GET /state HTTP/1.1\r\nHost: x\r\nContent-Length: 184467440737095516160\r\n\r\n",
        400,
    )
    expect_status(address, request(args.port, "/" + "a" * 2048), 414)
    expect_status(address, request(args.port, "/client_perf?frames=184467440737095516160"), 400)
    expect_status(address, request(args.port, "/client_perf?frames=1&frames=2"), 400)
    expect_status(address, request(args.port, "/client_perf?frames=1"), 204)
    expect_status(
        address,
        f"POST /state HTTP/1.1\r\nHost: 127.0.0.1:{args.port}\r\nConnection: close\r\n\r\n".encode("ascii"),
        405,
    )
    final_body = expect_status(address, request(args.port, "/state"), 200)
    parsed = json.loads(final_body.decode("utf-8"))
    if "tick" not in parsed or "keys" not in parsed:
        raise RuntimeError("final /state response is missing required fields")

    print("OVERLAY_HTTP_FRAMING=PASS fragmented=1 pipelined=2 bounded_rejections=8")
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except (OSError, RuntimeError, ValueError, json.JSONDecodeError) as error:
        raise SystemExit(f"FAIL: {error}")
