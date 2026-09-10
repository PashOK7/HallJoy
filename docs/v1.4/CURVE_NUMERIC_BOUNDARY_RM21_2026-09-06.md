# RM-21 curve and XUSB numeric boundary (2026-09-06)

The UI/backend common rational-curve evaluator now sanitizes every normalized
coordinate and weight before evaluation. Non-finite values become zero at the
normalization boundary; X endpoints are returned exactly by the inverse helper
instead of relying on a finite binary-search approximation. The normal backend
continues to enforce its ordered X geometry before invoking the common math.

The final configured-XUSB boundary independently rejects non-finite stick,
trigger, mouse, and button values. It emits neutral for those values before
rounding, so an invalid upstream sample cannot reach `lround` or a controller
report as undefined/implementation-dependent data.

Focused portable tests cover NaN/infinity, the inverse endpoints, deliberately
non-monotonic control geometry, and a complete XUSB report with non-finite
input. This is a correctness hardening, not a changed curve shape, threshold,
or performance optimization. No HallJoy runtime, HID, controller, output
child, or ROG diagnostic was started.
