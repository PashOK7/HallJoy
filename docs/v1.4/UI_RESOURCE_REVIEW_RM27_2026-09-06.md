# RM-27 UI resources, retained state and DPI review (2026-09-06)

The retained page surfaces, Remap panel and keyboard renderer consistently
deselect objects before deleting their DC/bitmap pairs; page-specific state is
released from `WM_NCDESTROY`, while shared scroll surfaces own cache bitmap
lifetime and clamp invalid content/scroll geometry.

The audit found one real long-session resource risk: glyph cache keys include
render size/style, but both keyboard-preview and Remap maps had no insertion
bound. Each now holds at most 256 owned DC/bitmap pairs. At capacity it frees a
fully deselected entry before caching a new size/style key. This retains cache
benefit during normal painting while preventing repeated DPI/style changes from
growing the GDI footprint without bound.

The existing UI structural oracle covers retained painting, scrolling, popup
ownership, shared persistence hand-off and bounded icon hit testing; it now
also pins both cache caps/eviction paths. Physical repeated page open/close,
DPI and multi-monitor handle-count measurement remains a later manual gate,
because it requires the running UI and must not overlap gameplay.
