#!/usr/bin/env python3
"""Keep the shared curve and final XUSB conversion finite at their boundaries."""
from pathlib import Path

root = Path(__file__).resolve().parents[1]
hall = root / "HallJoy"
math_header = (hall / "curve_math.h").read_text(encoding="utf-8")
math_source = (hall / "curve_math.cpp").read_text(encoding="utf-8")
backend = (hall / "backend_curve.cpp").read_text(encoding="utf-8")
builder = (hall / "configured_xusb_builder.cpp").read_text(encoding="utf-8")

for source, token in (
    (math_header, "std::isfinite(v)"),
    (math_source, "static Curve01 SanitizeCurve(Curve01 c)"),
    (math_source, "if (x01 <= safe.x0) return safe.y0;"),
    (backend, "std::isfinite(v)"),
    (builder, "if (!std::isfinite(value)) value = 0.0f;"),
    (builder, "return std::isfinite(value) && value >= kPressedThreshold;"),
):
    assert token in source, token
print("Curve finiteness static audit passed")
