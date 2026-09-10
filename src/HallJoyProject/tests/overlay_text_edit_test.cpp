#include "overlay_text_edit.h"
#include <cassert>
#include <iostream>
#include <random>
using namespace halljoy::overlay_edit;
int main() {
    Editor e;
    e.Reset(L"8765", Kind::Port);
    e.MoveTo(2, false); assert(e.Replace(L"0") && e.Text() == L"87065");
    e.Erase(true); assert(e.Text() == L"8765" && e.Caret() == 2);
    e.MoveTo(1, false); e.MoveTo(3, true);
    assert(e.Selection() == L"76");
    assert(e.Replace(L"42") && e.Text() == L"8425");
    e.SelectAll(); assert(e.Replace(L" 65535\r\n", true) && e.Text() == L"65535");
    e.SelectAll(); assert(!e.Replace(L"123456", true) && e.Text() == L"65535" && e.Selected());
    assert(!e.Replace(L"abc80", true) && e.Text() == L"65535" && e.Selected());
    assert(!e.Replace(L" \t", true) && e.Selected());
    assert(e.Replace(L"80", true));
    assert(e.Undo() && e.Text() == L"65535");
    assert(e.Undo(true) && e.Text() == L"80");
    e.MoveTo(0, false); e.Erase(false); assert(e.Text() == L"0");
    std::uint32_t v = 99;
    assert(!Value(e.Text(), Kind::Port, v) && v == 99);
    assert(!Value(L"65536", Kind::Port, v));
    assert(Value(L"65535", Kind::Port, v) && v == 65535);
    e.Reset(L"#123456", Kind::Hex); e.SelectAll();
    assert(e.Replace(L" #aBcDeF ", true) && e.Text() == L"#ABCDEF");
    assert(Value(e.Text(), Kind::Hex, v) && v == 0xABCDEF);
    e.MoveTo(1, false); e.Move(1, true); assert(e.Replace(L"0") && e.Text() == L"#0BCDEF");
    e.MoveTo(4, false); e.Move(-1, true, true); assert(e.Selection() == L"#0BC");
    e.Move(1, false); assert(e.Caret() == 4 && !e.Selected());
    e.SelectAll(); assert(!e.Replace(L"#12#456", true));
    assert(!e.Replace(L"#00GG00", true));
    assert(!Value(L"#123", Kind::Hex, v) && !Value(L"1234567", Kind::Hex, v));
    assert(Value(L"00FF00", Kind::Hex, v) && v == 0xFF00);
    std::mt19937 random(16);
    for (int i=0; i<20000; ++i) {
        switch(random()%8) {
        case 0: e.MoveTo(random()%12, random()%2); break;
        case 1: e.Erase(random()%2, random()%2); break;
        case 2: e.SelectAll(); break;
        case 3: e.Undo(random()%2); break;
        default: e.Replace(std::wstring(1,L"0123456789ABCDEF#x"[random()%18])); break;
        }
        assert(DraftAllowed(e.Text(), Kind::Hex));
        assert(e.Begin() <= e.End() && e.End() <= e.Text().size());
        assert(e.Caret() <= e.Text().size());
    }
    std::cout << "OVERLAY_TEXT_EDIT_TEST=PASS randomized_operations=20000\n";
}
