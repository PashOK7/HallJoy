#include "../HallJoy/layout_editor_model.h"
#include "../HallJoy/first_run_layout.h"
#include <cassert>
#include <iostream>
#include <random>
using namespace halljoy::layout_editor;
int main() {
    using halljoy::layout_selection::Match;
    assert(std::wstring(Match(0x362D,0x0610,0xFF60,0x61,6,15))==L"Lemokey P1 HE ANSI");
    assert(std::wstring(Match(0x362D,0x0611,0xFF60,0x61,6,15))==L"Lemokey P1 HE ISO");
    assert(!Match(0x362D,0x0612,0xFF60,0x61,6,15)); // JIS is not in shipped UAP.
    assert(!Match(0x362D,0x0610,0xFF60,0x61,6,16));
    assert(!Match(0x3434,0x0610,0xFF60,0x61,6,15));
    assert(!Match(0x362D,0x0610,1,6,6,15));
    for (const auto& identity : halljoy::layout_selection::kKeychronLayouts) {
        using halljoy::layout_selection::Match;
        assert(std::wstring(Match(0x3434,identity.pid,0xFF60,0x61,identity.rows,identity.columns)) == identity.name);
        assert(!Match(0x3434,identity.pid,0xFF60,0x61,identity.rows,identity.columns+1));
        assert(!Match(0x3434,identity.pid,0xFF60,0x61,identity.rows+1,identity.columns));
        assert(!Match(0x3434,identity.pid,1,6,identity.rows,identity.columns));
    }
    assert(GridForScale(8).majorStep == 5 && GridForScale(8).minorOpacity == 1);
    assert(GridForScale(1).majorStep == 5 && GridForScale(1).minorOpacity == 0);
    for (int i = 5; i <= 6400; ++i) {
        const float scale = i / 100.0f;
        const auto grid = GridForScale(scale);
        assert(grid.majorStep % 5 == 0 && grid.majorStep * scale >= 4);
        assert(grid.minorOpacity >= 0 && grid.minorOpacity <= 1);
        if (grid.minorOpacity > 0) assert(scale > 1.5f && grid.majorStep == 5);
    }
    KeyDef key{L"A", 4, 2, 0, 42};
    const Geometry initial{100, 120, 42, 40};
    const int edgeCases[] = {Left, Right, Top, Bottom, Left | Top, Right | Top, Left | Bottom, Right | Bottom};
    for (int edges : edgeCases) {
        for (int delta = -1000; delta <= 1000; ++delta) {
            auto r = Resize(initial, edges, delta, delta, true);
            assert(r.w >= 18 && r.w <= 600 && r.h >= 18 && r.h <= 600);
            assert(r.x >= 0 && r.y >= 0);
            if (edges & Left) assert(r.x + r.w == initial.x + initial.w);
            else assert(r.x == initial.x);
            if (edges & Top) assert(r.y + r.h == initial.y + initial.h);
            else assert(r.y == initial.y);
            auto rows = Resize(initial, edges, delta, delta, false);
            assert(rows.y == initial.y);
            if (!(edges & Bottom)) assert(rows.h == initial.h);
        }
    }
    std::vector<Guide> guides{{true, 100}, {false, 115}};
    assert(SnapPosition(97, 42, guides, true, 1) == 100);
    assert(SnapPosition(60, 42, guides, true, 1) == 58);
    assert(SnapPosition(97, 42, guides, true, 64) == 97);
    assert(SnapPosition(113, 40, guides, false, 1, 0, 4000, 23, 46) == 115);
    assert(SnapPosition(113, 40, guides, false, 1, 0, 4000, 0, 46) == 113);
    assert(SnapPosition(99, 1, guides, true, 1) == 99);
    View zoom; zoom.Zoom(20, 30, 1000); assert(zoom.scale == 64);
    assert(KeyboardLayout_KeyY(key) == 92);
    key.y = 115; assert(KeyboardLayout_KeyY(key) == 115);
    assert(RowY(key, 3) == 161 && RowY(key, 1) == 69);
    Draft draft{{key}, {L"A"}, false, 8, 0};
    History history; history.Reset(draft);
    draft.labels[0] = L"AB"; history.Record(draft, 7);
    draft.labels[0] = L"ABC"; history.Record(draft, 7);
    assert(history.Step(false)->labels[0] == L"A" && !history.CanUndo());
    assert(history.Step(true)->labels[0] == L"ABC");
    history.EndGroup(); draft.keys[0].y++; history.Record(draft);
    assert(history.Step(false)->keys[0].y == 115);
    draft.keys[0].x++; history.Record(draft); assert(!history.CanRedo());
    for (int i = 0; i < 10000; ++i) { draft.keys[0].x = i; history.Record(draft); }
    int undoCount = 0; while (history.Step(false)) ++undoCount;
    assert(undoCount == 64);
    KeyDef compound{L"Enter",40,2,0,66,86,92,12,40};
    assert(KeyboardLayout_ValidShape(compound));
    for (int y=0; y<86; ++y) for (int x=0; x<66; ++x)
        assert(KeyboardLayout_Contains(compound,x,y) == (x>=12 || y<40));
    assert(!KeyboardLayout_Contains(compound,-1,0));
    assert(!KeyboardLayout_Contains(compound,66,0));
    assert(!KeyboardLayout_Contains(compound,12,86));
    Draft shapeDraft{{compound},{L"Enter"},false,6,0};
    history.Reset(shapeDraft);
    shapeDraft.keys[0].notchW = 13; history.Record(shapeDraft);
    assert(history.CanUndo() && history.Step(false)->keys[0].notchW == 12);
    assert(history.Step(true)->keys[0].notchW == 13);
    compound.notchY = 0; assert(!KeyboardLayout_ValidShape(compound));
    compound.notchY = 86; assert(!KeyboardLayout_ValidShape(compound));
    compound.notchW = 0; compound.notchY = 0; assert(KeyboardLayout_ValidShape(compound));
    View view; view.Fit(1000, 400, 800, 600);
    std::mt19937 rng(9126);
    for (int i = 0; i < 20000; ++i) {
        float px = (float)(rng() % 800), py = (float)(rng() % 600);
        const float wx = (px - view.x) / view.scale, wy = (py - view.y) / view.scale;
        view.Zoom(px, py, rng() % 2 ? 1.15f : 1.0f / 1.15f);
        assert(std::abs((px - view.x) / view.scale - wx) < 0.04f);
        assert(std::abs((py - view.y) / view.scale - wy) < 0.04f);
        assert(view.scale >= 0.05f && view.scale <= kMaxZoom);
    }
    std::cout << "LAYOUT_EDITOR_MODEL=PASS history_bound=64 zoom_anchors=20000\n";
}
