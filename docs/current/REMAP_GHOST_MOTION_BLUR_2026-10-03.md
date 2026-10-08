# Remap drag ghost: motion blur (2026-10-03)

Owner request: add motion blur to the icons dragged with the cursor on the
Remap tab, like the tab transitions.

## Implementation (`remap_panel.cpp`)

- **Where.** The ghost is a layered window fed by `UpdateLayeredWindow` from a
  premultiplied 32bpp DIB. The blur lives in `Ghost_UpdateLayered`, the single
  output path, so it applies to:
  - dragging;
  - the hint ghost;
  - fly-back and shrink post-animations.
- **Velocity.** Taken from the ghost's smoothed float position (`gx`, `gy`)
  between updates, over the measured frame time (QPC, clamped to
  1/240..1/30 s), then lightly smoothed (0.6 blend).
- **Streak.** It covers the distance travelled in half a frame (180-degree
  shutter, the same as tab transitions). It is capped at 1.8x the padding;
  under 1 px the ghost is drawn sharp.
- **Compose.** `Ghost_ComposeBlur` averages the sharp icon at up to 32
  offsets, spaced at most 1.25 px apart along the motion vector and centred on
  the rest position.
  - It averages into a padded surface (padding 3/4 of the icon size) in
    16-bit accumulators.
  - Premultiplied BGRA averages correctly: the streak fades out with no dark
    fringes.
  - At rest the output equals the sharp icon.
- **Window.** The layered window is the padded surface, offset by the
  padding. It is input-transparent as before.
- **Reset.** `Ghost_Hide` and surface recreation reset the velocity history,
  so a ghost never appears with a streak.
- **Cost.** One small CPU pass per frame (icon-sized), no GPU.
- **Fallback.** If the padded DIB cannot be created, the ghost stays sharp
  exactly as before.

## Validation

- `build_release.ps1` and the native suite: PASS. EXE SHA-256
  `f68c0058649b5dab84329068e957a25ffdcebd106af59b88badd4c1dda4f5651`.
- No visual run by the agent; the owner judges the look.

## Frame pacing at the monitor's refresh rate (2026-10-03)

Owner report: at 200 Hz the dragged icon still showed separate frames; three
monitors with different refresh rates.

- **Cause.** The drag and post-animations ran on
  `SetTimer(DRAG_ANIM_TIMER_ID)` with the UI refresh setting (1 ms), the
  binding hint on its own 16 ms timer. Windows
  clamps timers to 10 ms and fires them on the system tick (~15.6 ms), so
  the ghost moved at ~64-100 Hz whatever the monitor. Frame times came from
  `GetTickCount`, which advances in the same ~16 ms steps, so the motion
  was uneven too.
- **Pacer (`display_pacer.h/.cpp`).**
  - A worker thread maps the target `HMONITOR` to its DXGI output and waits
    on `IDXGIOutput::WaitForVBlank`.
  - After each refresh it posts one frame message; at most one is queued, so
    a busy UI thread skips frames instead of piling them up.
  - It falls back to `DwmFlush` (then an 8 ms sleep) when there is no output
    or the driver returns instantly. It never spins.
  - `Stop` never blocks. The thread owns its shared state and exits after its
    current wait; one late frame message is ignored by the panel.
- **Remap panel.**
  - `Anim_Start` / `Anim_Stop` replace every `SetTimer` / `KillTimer` of the
    drag, post-animations and the binding hint (its 16 ms timer is gone).
    Stopping the hint keeps the frames when a drag or post-animation has
    taken over.
  - Each frame re-targets the pacer to the monitor under the ghost centre,
    so a drag onto another monitor switches to that monitor's rate.
  - `dt` is the spacing of the refreshes that produced the frames
    (`Pacer::FrameTime`), so it stays even when a frame is handled late.
  - Post-animation and hint clocks (`shrinkStartMs`, `postPhaseStartTick`,
    `hintStarted`) use `AnimNowMs()` (QPC) instead of `GetTickCount`.
- **Validation.**
  - `build_release.ps1` and the native suite: PASS.
  - EXE SHA-256
    `8f902585ff06907a0062958e916662b1e28cae6eab77c1df7e10a2187a26a985`.
  - No visual run by the agent; the owner judges the look.
- Follow-up (2026-10-05): `remap_hint_static_audit.py` still required the removed
  `KillTimer(panel, BIND_HINT_TIMER_ID)`; it now requires `Anim_Stop(st)` and
  `Anim_Start(panel, st)` in the hint code. Native suite PASS again.
