# Input Overlay Notes

HallJoy now has an `Input Overlay` tab next to `Global settings`.

Current state:
- the tab controls a local HTTP server for OBS Browser Source;
- default URL is `http://127.0.0.1:8765/`;
- `/` serves a transparent webpage that renders the active HallJoy keyboard layout;
- `/state` returns JSON with key geometry plus raw/output HE depth in milli-units.
- press depth is rendered as a solid fill, with bottom-to-top or top-to-bottom direction.
- depth display can use either post-curve output (`After curves`) or real raw key depth (`Raw press depth`) for OBS.
- analog glow intensity scales with current press depth.
- key border intensity, inner ring, and outer glow scale with analog depth.
- visual effects can be toggled individually: smoothing, glass keys, rim lighting, bloom, edge sweep, micro-scale, and label contrast.
- each visual effect has its own normalized `0-100%` strength slider in the `Input Overlay` tab.
- overlay refresh interval is adjustable from the `Input Overlay` tab.
- indicator and label colors are selectable through a compact HSV palette plus editable `#RRGGBB` values. The main square maps saturation horizontally and value vertically, so the left edge gives grayscale, the top-right gives saturated bright color, and the bottom gives black; the slim side strip selects hue.
- color picker rendering keeps separate cached DIB palettes for indicator and label color, so dragging inside the HSV square reuses prebuilt pixels instead of regenerating the gradient on every repaint.
- color picker drags are locked to the control part where the mouse press started: saturation/value square drags cannot accidentally become hue-strip drags, and hue-strip drags cannot jump into the square.
- color picker UI keeps its own hue/saturation/value cursor state, so dragging into black or grayscale does not reset the hue strip or teleport the selector to the generic black corner.
- default overlay look uses green `#00FF72`, top-to-bottom raw depth, 8 ms refresh, and tuned per-effect strengths. Existing profiles keep their selected interval, and 1 ms remains available as an explicit maximum-load option.
- the `Input Overlay` tab uses a custom styled scrollbar when its controls do not fit vertically.
- overlay settings are saved as global app settings: port, fill direction, depth display mode, visual effects, effect strengths, refresh interval, and indicator color.
- if the overlay server was running when HallJoy closed, it starts automatically on the next launch.
- overlay transport uses HTTP keep-alive for OBS polling instead of reopening a socket on every state update.
- overlay render path caches DOM nodes/styles and skips unchanged key style writes to reduce browser-side cost without changing the visual.
- OBS-side drawing is paced through `requestAnimationFrame`, so a 1 ms overlay interval no longer forces hundreds of browser repaints per second.
- OBS overlay rendering now uses a single canvas instead of many dynamic DOM nodes, avoiding mass CSS `box-shadow`/layout work when many keys are pressed at the same time.
- the canvas is retained after smoothing converges and is redrawn only for resize, layout/style changes, or a visible key-depth change; polling `/state` no longer implies a full repaint.
- key visuals are sprite-cached by size, color, effect settings, fill direction, and press depth bucket so expensive glow/blur work is reused across frames.
- key label text is cached separately as bitmap sprites, avoiding per-frame text shadow rendering.
- sprite and label caches are bounded at 512 and 256 entries and use constant-time insertion-order LRU eviction, preventing an animated or long-running page from growing them without limit.
- key label sprites now support stream-side style controls: font preset, text size, shadow strength, and independent label color. The font preset list includes neutral UI, condensed, heavy, arcade, rounded, mono, narrow, compact UI, Comic Sans, Japanese Gothic/Mincho, MS Gothic, and fantasy-style options. These values are saved in settings and included in the label sprite cache key.
- cached key sprites include a subtle top sheen highlight for a richer pressed-key look without adding per-frame cost.
- Glass keys are rendered as a full material inside the cached key sprite: dark glass depth, bevel, inner lens reflections, subtle diagonal specular, prism edge separation, and under-glass caustics from the key's own HE fill.
- Adjacent pressed keys add live light only inside a thin bevel/rim mask that matches the key edge thickness; light is computed from key rectangles, so wide keys illuminate the overlapping rim span instead of acting like a point source.
- Rim lighting is normalized so `50%` matches the tuned bevel-lighting look while `100%` remains available for a stronger stream style.
- Bloom, Edge sweep, and Label contrast now have distinct visual responsibilities: Bloom controls the outer aura/fill glow, Edge controls the bright edge line and active border, and Label contrast controls text outline/halo.
- Bloom renders only as a separate lower canvas light layer. The external glow is no longer baked into cached key sprites, which prevents sprite-edge clipping and square halos; it uses circular/elliptical multi-stop fields with zero-alpha edges.
- New default effect set is based on the tuned stream look: all effects are enabled, Smooth response is `39%`, Glass is `50%`, Bloom is `33%`, Edge sweep and Micro-scale are `50%`, Label contrast is `0%`, and Rim lighting is `50%`.
- old overlay strength settings are migrated to the normalized scale through `StrengthScaleVersion=5`; the previous `0%` Glass look now maps to `50%`, true `0%` disables the glass material visually, and the previous `359%` Rim lighting look maps to `50%`.
- smoothing is time-based rather than frame-count-based, so the feel stays consistent if OBS/browser frame pacing changes.
- perf logging is emitted as `[overlay.perf]` about every 5 seconds while the overlay is being polled; Release builds also write it to `overlay_perf.log` next to `HallJoy.exe`. It includes server request/build/send timing plus browser-reported fetch/render/layout timing.
- `tools/run_input_pipeline_profile.ps1` profiles the production Irok-to-browser chain in idle, real-page and animated-stress phases. It measures the full HallJoy and browser process trees separately, attributes persistent worker and residual short-lived CPU, verifies physical Spark transaction timing, hashes evidence, preserves user state and rejects surviving processes.
- the overlay page has a synthetic render-probe mode for local testing without OBS or physical key presses: `/?synthetic=1&active=32&hz=2.2`. It fetches the real layout/settings once, then emulates HE key depth in the browser so `tools/overlay_synthetic_probe.ps1` can stress the canvas/effects path automatically.
- `HallJoyTests` also contains a native Blend2D overlay render benchmark. It does not open Edge or OBS; it renders a 1280x720/84-key stress frame into a native bitmap so renderer experiments can be measured without browser background noise.
- SparkLink analog polling is paced row-by-row instead of bursting all matrix-row route requests back-to-back, reducing pressure on the keyboard firmware while preserving live HE depth for the overlay.
- Configuration has a temporary `Log Missed HID Presses` diagnostic toggle for SparkLink: when enabled, HallJoy logs `[backend.spark.missed_hid]` if analog depth crosses 50% but no matching physical HID key-down arrives within the diagnostic window.
- SparkLink/Sayo overlapped HID I/O reuses a per-thread event instead of creating and closing a kernel event for every 1 ms read/write transaction.
- SparkLink normal HID transactions now reuse per-thread packet buffers instead of allocating vectors for every 64/65-byte read/write.
- The `Input Overlay` settings tab is moving to a custom single-HWND renderer. The page uses the shared `CustomPageSurface` retained-scroll module: content is cached as one offscreen bitmap and scrolling copies the visible slice, which removes child-control scroll artifacts. During manual scroll tests HallJoy logs `[ui.overlay.scroll]` with paint count, approximate paint FPS, paint cost, and cache rebuild cost in Debug builds so perceived smoothness can be compared with measured repaint timing.

Next implementation steps:
- add visual style controls for stream layouts;
- consider WebSocket streaming after the JSON schema settles.
