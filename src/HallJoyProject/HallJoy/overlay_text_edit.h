#pragma once
#include <algorithm>
#include <cstdint>
#include <string>
#include <vector>

namespace halljoy::overlay_edit {
enum class Kind { Port, Hex };
inline bool DraftAllowed(const std::wstring& s, Kind kind) {
    if (s.size() > (kind == Kind::Port ? 5u : 7u)) return false;
    for (size_t i = 0; i < s.size(); ++i) {
        const wchar_t c = s[i];
        if (c >= L'0' && c <= L'9') continue;
        if (kind == Kind::Hex && ((c >= L'A' && c <= L'F') || (c >= L'a' && c <= L'f') || (i == 0 && c == L'#'))) continue;
        return false;
    }
    return true;
}
inline bool Value(const std::wstring& s, Kind kind, std::uint32_t& value) {
    if (!DraftAllowed(s, kind) || s.empty()) return false;
    size_t start = kind == Kind::Hex && s[0] == L'#' ? 1 : 0;
    if (kind == Kind::Hex && s.size() - start != 6) return false;
    std::uint32_t n = 0;
    for (size_t i = start; i < s.size(); ++i) {
        const auto c = s[i];
        n = n * (kind == Kind::Hex ? 16 : 10) +
            (c <= L'9' ? c - L'0' : c <= L'F' ? c - L'A' + 10 : c - L'a' + 10);
    }
    if (kind == Kind::Port && (n == 0 || n > 65535)) return false;
    value = n;
    return true;
}
class Editor {
    struct State { std::wstring text; size_t caret = 0, anchor = 0; };
    State state;
    std::vector<State> undo, redo;
    static void Push(std::vector<State>& stack, const State& value) {
        if (stack.size() == 32) stack.erase(stack.begin());
        stack.push_back(value);
    }
public:
    Kind kind = Kind::Port;
    void Reset(std::wstring text, Kind type) { state = {std::move(text), 0, 0}; kind = type; undo.clear(); redo.clear(); }
    const std::wstring& Text() const { return state.text; }
    size_t Caret() const { return state.caret; }
    size_t Begin() const { return std::min(state.caret, state.anchor); }
    size_t End() const { return std::max(state.caret, state.anchor); }
    bool Selected() const { return Begin() != End(); }
    std::wstring Selection() const { return state.text.substr(Begin(), End() - Begin()); }
    void SelectAll() { state.anchor = 0; state.caret = state.text.size(); }
    void MoveTo(size_t pos, bool extend) { state.caret = std::min(pos, state.text.size()); if (!extend) state.anchor = state.caret; }
    void Move(int direction, bool extend, bool wholeValue = false) {
        if (!extend && Selected() && !wholeValue) { MoveTo(direction < 0 ? Begin() : End(), false); return; }
        MoveTo(wholeValue ? (direction < 0 ? 0 : state.text.size()) :
            (direction < 0 ? (state.caret ? state.caret - 1 : 0) : state.caret + 1), extend);
    }
    // Whole replacement is validated before mutation: a rejected paste never
    // deletes selection, truncates a value or silently strips invalid characters.
    bool Replace(std::wstring text, bool paste = false) {
        if (paste) {
            const auto first = text.find_first_not_of(L" \t\r\n");
            if (first == std::wstring::npos) return false;
            text = text.substr(first, text.find_last_not_of(L" \t\r\n") - first + 1);
        }
        for (auto& c : text) if (c >= L'a' && c <= L'f') c -= L'a' - L'A';
        std::wstring next = state.text;
        next.replace(Begin(), End() - Begin(), text);
        if (!DraftAllowed(next, kind)) return false;
        if (next != state.text) { Push(undo, state); redo.clear(); }
        const size_t position = Begin() + text.size();
        state.text = std::move(next); MoveTo(position, false);
        return true;
    }
    void Erase(bool backwards, bool wholeValue = false) {
        if (!Selected()) {
            const auto old = state;
            Move(backwards ? -1 : 1, true, wholeValue);
            if (!Selected()) { state = old; return; }
        }
        Replace(L"");
    }
    bool CanUndo() const { return !undo.empty(); }
    bool CanRedo() const { return !redo.empty(); }
    bool Undo(bool forward = false) {
        auto& from = forward ? redo : undo;
        auto& to = forward ? undo : redo;
        if (from.empty()) return false;
        Push(to, state); state = from.back(); from.pop_back(); return true;
    }
};
}
