# Custom UI Architecture Direction

HallJoy should move away from scrollable pages made from many Win32 child controls.

The target UI model is a custom retained/immediate hybrid surface:
- each scrollable page is one HWND and one backbuffer;
- UI elements are data records, not child windows;
- layout produces rectangles in content coordinates;
- scrolling changes only the viewport offset;
- painting clips to the viewport and draws visible elements from the retained model;
- hit testing maps mouse coordinates through the viewport into content coordinates;
- hover, pressed, focused, drag, and text-edit states are owned by HallJoy;
- scrollbars are drawn by HallJoy and never rely on native scroll controls;
- controls should redraw with stable dark theme pixels only, without white native erase frames;
- expensive content can be cached as page/layer bitmaps, invalidated only when state changes.

The current Win32 child-control approach is not acceptable for the final high-refresh UI goal because:
- each edit/button/slider/static is its own window with its own erase/redraw lifecycle;
- moving child windows during scroll causes white frames, disappearing controls, or forced synchronous repaint;
- forcing repaint avoids artifacts but caps perceived scroll smoothness;
- the page cannot be treated as a single retained texture while it is made of separate native windows.

Migration policy:
- do not rewrite every page at once;
- migrate one visible problem page at a time;
- keep the old implementation only as a temporary fallback while the custom page reaches feature parity;
- start with `Input Overlay` because it is scroll-heavy, self-contained, and currently shows the clearest artifacts;
- after `Input Overlay`, apply the same renderer to `Mouse settings`, then `Configuration`, then `Remap`.

First custom page requirements for `Input Overlay`:
- custom labels, buttons, checkboxes, sliders, chips, color preview, hue strip, URL/status text, and scrollbar;
- custom numeric/text fields for port and HEX with caret, selection, clipboard paste, backspace/delete, and focus ring;
- wheel and drag scrolling must feel smooth on a 240 Hz display;
- no white flashes, disappearing controls, stale text, ghost trails, clipped controls, or delayed scrollbar thumb;
- visuals must match HallJoy's dark theme and look at least as polished as the existing custom controls;
- settings must still save immediately;
- keyboard navigation can be minimal at first but must not break mouse workflows.

Validation checklist for each migrated page:
- fast wheel scroll up/down;
- scrollbar drag top to bottom and back;
- resize while scrolled;
- tab switch away/back;
- interact with sliders and text fields while scrolled;
- verify no visual artifacts on high refresh display;
- verify settings persist after restart.

Shared implementation:
- new custom pages should use `HallJoy/custom_page_surface.h`;
- `CustomPageSurface` owns scroll offset, content height, retained bitmap cache, styled scrollbar geometry/painting, and scroll paint counters;
- new custom controls should use `HallJoy/custom_page_controls.h` for shared text, button, chip, checkbox, slider, and rounded-rect drawing;
- page code should keep controls as data records, render content into the surface cache, and use `CustomPageSurface_SetScrollY` for wheel/drag scrolling;
- avoid adding Win32 child controls to new scroll-heavy pages unless there is a measured reason and the control does not participate in smooth scrolling;
- `Input Overlay` is the reference implementation for this architecture before migrating `Mouse settings`, `Configuration`, and `Remap`.

Migration status:
- `Input Overlay`: uses `CustomPageSurface` for retained page cache, scrollbar, scroll state, and scroll perf counters.
- `Mouse settings`: migrated to the same single-HWND retained-render path as `Input Overlay`; its controls are now data records and scrolling copies the cached page slice instead of moving child HWNDs. Shared drawing primitives are being extracted into `custom_page_controls` so the next pages do not duplicate button/slider/text code.
- `Configuration`: now uses the shared retained bitmap path for the custom-mode page, matching `Input Overlay` and `Mouse settings`: the full content is rendered into `CustomPageSurface::contentCache`, scrolling copies the visible slice, and state changes mark the cache dirty. `KeySettingsPanel` draws its override/invert toggles, info text, mode selector, and preset selector without visible child HWNDs. The lower settings block also draws Snap Stick, Last Key Priority, Block Bound Keys, missed-HID debug, addressed-protocol status, slider/chip, and status on the same surface. Remaining work: replace the temporary/simple custom dropdown behavior with full rename/delete/inline-create parity, then remove the hidden compatibility HWNDs entirely.

Configuration scroll rule:
- never move or repaint separate child controls while `customControls` is enabled;
- wheel and scrollbar movement must only change `scrollY` and BitBlt from the retained page cache;
- settings changes, graph morphs, profile edits, and hotplug visibility changes must call `Config_MarkSurfaceDirty`;
- if text, combo boxes, or graph labels lag behind the graph during scroll, it means a control escaped the retained path or the cache is being skipped.
- Non-scroll pages should still use the same visual primitives and should add `CustomPageSurface` as soon as they need scrolling.
