from pathlib import Path

root = Path(__file__).resolve().parents[1]
source = (root / 'HallJoy/keyboard_subpages.cpp').read_text(encoding='utf-8-sig')
layout = source.split('static void OverlayCustom_RebuildLayout(', 1)[1].split('static OverlayCustomItem* OverlayCustom_HitTest', 1)[0]
assert layout.index('const int actionIds[]') < layout.index('bool compactTop') < layout.index('L"Visual effects"')
assert 'L"Copy URL"' not in layout
assert 'const int actionIds[] = { OVERLAY_ID_TOGGLE, OVERLAY_ID_OPEN };' in layout
assert 'L"Open overlay"' in layout
assert 'actionX + btnW > x + w' in layout
assert 'i != 1 || running' in layout
assert 'OverlayCustom_BuildUrl(st)' in layout
assert 'OverlayServer_GetLastError()' in layout
assert 'DT_CALCRECT | DT_WORDBREAK' in layout
assert 'L"Server status"' not in layout
# Exercise the exact wrapping geometry at supported logical widths and DPIs.
for scale in (1, 1.25, 1.5, 2):
    s = lambda n: round(n * scale)
    for logical_width in (260, 280, 400, 416, 600, 1000):
        width, button, gap = s(logical_width), s(132), s(10)
        x = y = 0
        rectangles = []
        for _ in range(2):
            if x + button > width:
                x = 0
                y += s(30) + gap
            rectangles.append((x, y, x + button, y + s(30)))
            x += button + gap
        assert all(0 <= a < c <= width for a, b, c, d in rectangles)
        for i, (a,b,c,d) in enumerate(rectangles):
            for e,f,g,h in rectangles[i+1:]:
                assert c <= e or g <= a or d <= f or h <= b
        if width - x < s(350):
            x = 0
            y += s(30) + gap
        address = (x, y, width, y + s(32))
        assert address[0] < address[2]
        for a,b,c,d in rectangles:
            assert c <= address[0] or d <= address[1]
assert 'x + w - actionX < addressMinWidth' in layout
assert 'RECT{ actionX, y, x + w, y + S(hWnd, 32) }' in layout
assert 'OverlayCustomKind::CopyAddress' in layout
assert 'L"Click to copy"' in source
assert 'IDC_HAND' in source
copy = source.split('case 10: // Address field', 1)[1].split('case OVERLAY_ID_DIRECTION:', 1)[0]
assert 'OverlayPage_SetClipboardText' in copy
assert 'L"Copied!" : L"Copy failed"' in copy
assert 'return;' in copy and 'OverlayCustom_RequestSave' not in copy
assert 'KillTimer(hWnd, OVERLAY_ID_COPY)' in source
print('OVERLAY_TOOLBAR_STATIC_AUDIT=PASS')
