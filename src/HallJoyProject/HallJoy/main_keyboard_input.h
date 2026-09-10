#pragma once
#include <windows.h>
#include <cwchar>

namespace halljoy::main_input {
// Explicit custom text editors/capture surfaces opt in only while editing.
inline UINT QueryExplicitInput() {
    static const UINT message = RegisterWindowMessageW(L"HallJoy.ExplicitKeyboardInput.v1");
    return message;
}
inline bool BelongsToMain(HWND target, HWND main) {
    HWND root = GetAncestor(target, GA_ROOT);
    for (int depth=0; root && depth<8; ++depth) {
        if (root == main) return true;
        wchar_t name[64]{}; GetClassNameW(root,name,64);
        // Independent editor/dialog scopes keep their own policy. All other
        // owned popups inherit the main policy, including future custom ones.
        if (wcscmp(name,L"KeyboardLayoutEditorHost") == 0 || wcscmp(name,L"#32770") == 0) return false;
        root = GetAncestor(GetWindow(root,GW_OWNER),GA_ROOT);
    }
    return false;
}
inline bool Allow(const MSG& message, HWND main) {
    if (message.message < WM_KEYFIRST || message.message > WM_KEYLAST ||
        !BelongsToMain(message.hwnd,main)) return true;
    if (GetFocus() != message.hwnd) return false;
    wchar_t name[64]{}; GetClassNameW(message.hwnd,name,64);
    if (_wcsicmp(name,L"EDIT") == 0) return true;
    const UINT query = QueryExplicitInput();
    return query && SendMessageW(message.hwnd,query,0,0) == 1;
}
}
