#!/usr/bin/env python3
"""One paint standard for tab pages and their controls (custom_page_surface.h).

1. CustomPageSurface_Present clears the client before drawing. Called on the
   window DC it flashes the background and scrollbar on every repaint (the
   first Profiles tab did this). It may only run inside CustomPagePaintScope
   (directly or through CustomPageSurface_Paint).
2. Every window procedure that paints itself on a tab page handles
   WM_PRINTCLIENT with the same drawing code, so the tab transition can
   render any page, visible or not, exactly as it appears on screen.
   Top-level popups (toasts, open drop-down lists, the layout editor) are not
   part of a page and are listed explicitly.
3. No private double-buffer helpers: the shared scope owns buffering.
"""
import re
from pathlib import Path

HALL = Path(__file__).resolve().parents[1] / "HallJoy"
PAGE_FILES = ["keyboard_subpages.cpp", "remap_panel.cpp", "profiles_page.cpp",
              "premium_combo_core.cpp", "keyboard_keysettings_panel_style.cpp"]
# Markers that identify a top-level window's paint handler near its WM_PAINT.
TOP_LEVEL_MARKERS = ["SetLayeredWindowAttributes", "PaintPopup(", "Layout_DrawCanvas("]
failures = []

for path in sorted(HALL.glob("*.cpp")):
    if path.name == "custom_page_surface.cpp":
        continue
    text = path.read_text(encoding="utf-8", errors="replace")
    lines = text.splitlines()
    for i, line in enumerate(lines):
        if "CustomPageSurface_Present(" in line:
            window = "\n".join(lines[max(0, i - 80):i])
            if "CustomPagePaintScope" not in window:
                failures.append(f"{path.name}:{i + 1}: Present outside CustomPagePaintScope; use CustomPageSurface_Paint")
    if re.search(r"static\s+void\s+\w*DoubleBuffer\w*Paint\s*\(", text) or "BufferedPaint(" in text:
        failures.append(f"{path.name}: private double-buffer helper; use CustomPagePaintScope")

for name in PAGE_FILES:
    lines = (HALL / name).read_text(encoding="utf-8", errors="replace").splitlines()
    for i, line in enumerate(lines):
        if not re.match(r"\s*case WM_PAINT\s*:", line):
            continue
        near = "\n".join(lines[max(0, i - 4):i + 14])
        if "case WM_PRINTCLIENT" in near:
            continue
        context = "\n".join(lines[max(0, i - 40):i + 12])
        if any(marker in context for marker in TOP_LEVEL_MARKERS):
            continue
        failures.append(f"{name}:{i + 1}: WM_PAINT without WM_PRINTCLIENT on a tab page")

profiles = (HALL / "profiles_page.cpp").read_text(encoding="utf-8")
if "CustomPageSurface_Paint(" not in profiles:
    failures.append("profiles_page.cpp must paint through CustomPageSurface_Paint")

for failure in failures:
    print("FAIL:", failure)
if failures:
    raise SystemExit(1)
print("CUSTOM_PAGE_PAINT_STATIC_AUDIT=PASS one paint standard, printable pages")
