#pragma once
#include "keyboard_layout.h"
#include <algorithm>
#include <cmath>
#include <vector>

namespace halljoy::layout_editor {
inline constexpr float kMaxZoom = 64.0f;
enum ResizeEdge { Left = 1, Right = 2, Top = 4, Bottom = 8 };
struct Geometry { int x, y, w, h; };
inline Geometry Resize(Geometry start, int edges, int dx, int dy, bool exactY) {
    if (!exactY) edges &= ~Top;
    Geometry result = start;
    if (edges & Left) {
        result.x = std::clamp(start.x + dx, std::max(0, start.x + start.w - 600),
            std::min(4000, start.x + start.w - 18));
        result.w = start.x + start.w - result.x;
    } else if (edges & Right) result.w = std::clamp(start.w + dx, 18, 600);
    if (edges & Top) {
        result.y = std::clamp(start.y + dy, std::max(0, start.y + start.h - 600),
            std::min(4000, start.y + start.h - 18));
        result.h = start.y + start.h - result.y;
    } else if (edges & Bottom) result.h = std::clamp(start.h + dy, 18, 600);
    return result;
}
struct Guide { bool vertical; int coordinate; };
inline int SnapPosition(int position, int size, const std::vector<Guide>& guides,
    bool vertical, float scale, int minPosition = 0, int maxPosition = 4000,
    int rowOffset = 0, int rowPitch = 0) {
    float distance = std::min(8.0f, 6.0f / scale);
    int result = position;
    for (const auto& guide : guides) {
        if (guide.vertical != vertical) continue;
        const int offsets[] = {0, size, size / 2};
        for (int i = 0; i < 3; ++i) {
            if (i == 2 && size % 2) continue;
            const int offset = offsets[i];
            const int candidate = guide.coordinate - offset;
            if (candidate < minPosition || candidate > maxPosition ||
                (rowPitch && (candidate - rowOffset) % rowPitch)) continue;
            const float delta = (float)std::abs(candidate - position);
            if (delta <= distance) { distance = delta; result = candidate; }
        }
    }
    return result;
}
struct PixelGrid {
    int majorStep;
    float minorOpacity;
};
inline PixelGrid GridForScale(float scale) {
    // Layout pixels, not screen pixels. Suppress subpixel-density lines on zoom-out.
    int majorStep = 5;
    while (majorStep * scale < 4.0f && majorStep < 3125) majorStep *= 5;
    return {majorStep, std::clamp((scale - 1.5f) / 1.5f, 0.0f, 1.0f)};
}
inline int RowY(const KeyDef& key, int row) {
    return std::clamp(KeyboardLayout_KeyY(key) + (row - key.row) * KEYBOARD_ROW_PITCH_Y, 0, 4000);
}
struct View {
    float scale = 1, x = 0, y = 0;
    bool ready = false;
    void Fit(float width, float height, float areaW, float areaH) {
        scale = std::clamp(std::min((areaW - 32) / std::max(1.0f, width),
            (areaH - 32) / std::max(1.0f, height)), 0.05f, kMaxZoom);
        x = (areaW - width * scale) / 2; y = (areaH - height * scale) / 2;
        ready = true;
    }
    void Zoom(float px, float py, float factor) {
        const float next = std::clamp(scale * factor, 0.05f, kMaxZoom);
        x = px - (px - x) * next / scale;
        y = py - (py - y) * next / scale;
        scale = next;
    }
};
struct Draft {
    std::vector<KeyDef> keys;
    std::vector<std::wstring> labels;
    bool spacing = false;
    int gap = 8, selected = -1;
    bool Same(const Draft& other) const {
        if (labels != other.labels || keys.size() != other.keys.size() ||
            spacing != other.spacing || gap != other.gap) return false;
        for (size_t i = 0; i < keys.size(); ++i) {
            const auto& a = keys[i]; const auto& b = other.keys[i];
            if (a.hid != b.hid || a.x != b.x || KeyboardLayout_KeyY(a) != KeyboardLayout_KeyY(b) ||
                a.row != b.row || a.w != b.w || a.h != b.h ||
                a.notchW != b.notchW || a.notchY != b.notchY) return false;
        }
        return true;
    }
};
class History {
    std::vector<Draft> entries;
    size_t cursor = 0;
    uintptr_t group = 0;
public:
    void Reset(const Draft& draft) { entries = {draft}; cursor = 0; group = 0; }
    void EndGroup() { group = 0; }
    void Select(int selected) { if (!entries.empty()) entries[cursor].selected = selected; EndGroup(); }
    void Record(const Draft& draft, uintptr_t editGroup = 0) {
        if (entries.empty()) { Reset(draft); return; }
        if (entries[cursor].Same(draft)) return;
        entries.resize(cursor + 1);
        if (editGroup && editGroup == group && cursor > 0) entries[cursor] = draft;
        else { entries.push_back(draft); ++cursor; }
        if (entries.size() > 65) { entries.erase(entries.begin()); --cursor; }
        group = editGroup;
    }
    bool CanUndo() const { return cursor > 0; }
    bool CanRedo() const { return cursor + 1 < entries.size(); }
    const Draft* Step(bool redo) {
        EndGroup();
        if (redo ? !CanRedo() : !CanUndo()) return nullptr;
        if (redo) ++cursor; else --cursor;
        return &entries[cursor];
    }
};
}
