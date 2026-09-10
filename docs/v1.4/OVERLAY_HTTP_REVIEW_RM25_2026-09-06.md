# RM-25 overlay HTTP access, boundary and responsiveness review (2026-09-06)

## Result

The current server already implements the required loopback-only and bounded
model. No production rewrite is justified without a reproduced defect or a
measurement showing a missed limit. The server binds only `127.0.0.1`, creates
a fresh 128-bit CSPRNG session token per server generation, and uses a fixed
16-client ownership table with independent client workers.

## Endpoint and access matrix

| Endpoint | Data direction | Required access | Bounds and lifecycle |
|---|---|---|---|
| `/` and `/index.html` | Reads static overlay HTML; issues current session cookie | No cookie, but a supplied Origin must exactly be `http://127.0.0.1:<bound-port>` | HTML response follows request keep-alive policy. Cookie is `HttpOnly; SameSite=Strict`; hostile Origin receives no cookie. |
| `/state` | Reads a point-in-time JSON representation of layout/configuration/depth state | Valid one-time server-generation session cookie; exact loopback Origin when Origin is supplied | No state mutation. Builds JSON outside the configuration-state mutex after obtaining snapshots; unauthorised requests close with 401. |
| `/client_perf` | Accepts bounded numeric telemetry counters only | Same session/origin rule as `/state` | Every number is strict `from_chars` parsed and bounded to 1,000,000,000. Response is 204 and forcibly closes, preventing an idle telemetry worker. |

Unknown paths return a closing 404. GET is the only accepted method. Request
headers, body, target, header count, buffered input, keep-alive requests and
concurrent clients are all independently bounded (8 KiB, 4 KiB, 2 KiB, 64,
the server buffer bound, 256 requests/client, and 16 clients respectively).
Transfer-Encoding, repeated framing/security headers, invalid numeric forms and
ambiguous framing are rejected before endpoint dispatch.

## Adversarial and shutdown properties

- The parser consumes exactly one complete frame, retains the remainder for
  legal pipelining and rejects conflicting Content-Length or any transfer
  coding. Integer parsing is complete/overflow-safe.
- Each client has receive/send timeouts. Saturation is promptly rejected with
  closing 503 rather than waiting behind attacker-controlled input. Shutdown
  closes all client sockets before waiting for the accept worker; an incomplete
  join retains the handles/WSA ownership and blocks restart rather than freeing
  live objects.
- State and telemetry require the session cookie. CORS is never wildcarded;
  only the exact bound loopback origin is echoed, with `Vary: Origin`.
- Existing socket regressions exercise fragmented/pipelined hostile frames,
  bad framing, hostile origins, stale sessions, slow clients, saturation,
  responsiveness and shutdown clients. Those server interactions are
  output-capable HallJoy runtime tests and remain deferred while the owner is
  using the machine for gaming.

## Considered alternatives

Opening the bind address or relaxing origin/session checks was rejected because
it changes the localhost trust boundary. A single synchronous worker was
rejected because a slow client could hold state delivery. Moving JSON generation
into realtime was rejected because it would introduce allocation/string work in
the input/output path. The existing bounded-worker, snapshot-based design has
the correct ownership and latency boundary, so RM-25 requires physical runtime
evidence later rather than speculative code churn now.
