# RM-23 private UAP host trust boundary (2026-09-06)

The parent still extracts the private ABI1 plugin atomically and compares it
byte-for-byte with its embedded resource before launch. The child now repeats
that identity check against its own executable resource immediately before
loading the passed path. It opens the candidate as a non-reparse ordinary file
with no write/delete sharing, retains that verification lease while
`LoadLibraryW` opens the image, then releases it only after load succeeds or
fails. This closes the ordinary check-to-load replacement window.

The parent-to-child command remains an explicit absolute path plus a bounded
inherited-handle list, owner process handle, PID, nonce, and provider-plane
generation. The child validates those values before the plugin load. Unknown
host command forms continue to fail closed.

This is an integrity boundary against accidental/cross-process replacement at
the same user privilege level; it is not a claim of elevation, a sandbox, or a
defense against an attacker who can alter the executable/resource itself.
Static trust-boundary audits and source-only syntax checks passed. No HallJoy
runtime, HID, controller, output child, or ROG diagnostic was started.
