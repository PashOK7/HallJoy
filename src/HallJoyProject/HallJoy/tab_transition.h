#pragma once
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include "tab_transition_motion.h"

// Animated tab switching: a camera slides along a strip of page snapshots
// (all tabs side by side) from the shown position to the new tab in kDuration (0.2 s),
// passing every tab in between. Pages render their snapshots through the
// shared paint standard (CustomPage_PrintTree), so a snapshot is exactly what
// the page shows. The strip is drawn by a Direct2D layer paced to the display
// refresh. A click during a transition continues from the current position
// and speed. Resizing re-captures at the new size. With client-area
// animations disabled in Windows, or while hidden, switching is instant.
// Design: docs/current/TAB_TRANSITIONS_2026-10-03.md.
namespace halljoy::tab_transition {

struct Host {
    HWND parent = nullptr;                 // window that owns the layer (keyboard page)
    HWND tab = nullptr;                    // tab control; pages fill its page area
    int count = 0;                         // number of tabs
    HWND (*page)(int index) = nullptr;     // page window of a tab, nullptr if none
    void (*commit)(int index) = nullptr;   // show exactly this page (end of a transition)
};

void Initialize(const Host& host);
void Shutdown();

// Starts or retargets a transition to `target`. `shown` is the page visible
// now when no transition is running. Returns false when the caller must
// switch instantly (animation disabled, window hidden, no layer).
bool SwitchTo(int shown, int target);
// Snapshots of pages a transition passes are kept between transitions. Drop
// them when page content changes while hidden (profile switch, layout).
void InvalidateCache();
// Ends a running transition at once on `index` (no-op when idle).
void JumpTo(int index);
// The tab page area moved or changed size (call after pages were resized).
void OnLayout();
bool Active();
// Camera position in tab units while a transition runs (for the tab
// indicator), otherwise a negative value.
float IndicatorPosition();

} // namespace halljoy::tab_transition
