#pragma once
#include <windows.h>
#include <algorithm>
#include <array>
#include <cmath>
#include "keyboard_layout.h"

inline constexpr const wchar_t* kKeyShapeProperty = L"HallJoyKeyShape";
inline POINT KeyShape_Get(HWND key) {
    const auto packed = reinterpret_cast<ULONG_PTR>(GetPropW(key, kKeyShapeProperty));
    return POINT{ (LONG)(packed & 65535), (LONG)(packed >> 16) };
}
inline std::array<POINT, 6> KeyShape_Points(const RECT& r, POINT notch) {
    return {{{r.left,r.top}, {r.right,r.top}, {r.right,r.bottom},
             {r.left+notch.x,r.bottom}, {r.left+notch.x,r.top+notch.y}, {r.left,r.top+notch.y}}};
}
inline int KeyShape_InnerInset(const RECT& r, POINT notch) {
    return std::max(0,std::min({3,(int)(notch.y-1)/2,(int)(r.right-r.left-notch.x-1)/2}));
}
inline void KeyShape_Set(HWND window, const KeyDef& key, int width, int height) {
    const POINT notch{key.notchW ? std::clamp((int)std::lround((double)key.notchW*width/key.w),1,width-1) : 0,
                      key.notchW ? std::clamp((int)std::lround((double)key.notchY*height/key.h),1,height-1) : 0};
    const auto old = KeyShape_Get(window);
    RECT bounds{}; GetClientRect(window, &bounds);
    if (old.x == notch.x && old.y == notch.y && bounds.right == width && bounds.bottom == height) return;
    if (!notch.x) {
        RemovePropW(window, kKeyShapeProperty);
        if (old.x) SetWindowRgn(window, nullptr, FALSE);
        return;
    }
    const auto polygon = KeyShape_Points(RECT{0,0,width,height}, notch);
    HRGN region = CreatePolygonRgn(polygon.data(), (int)polygon.size(), WINDING);
    if (!region) return;
    if (!SetPropW(window, kKeyShapeProperty, reinterpret_cast<HANDLE>((ULONG_PTR)notch.x | ((ULONG_PTR)notch.y << 16)))) {
        DeleteObject(region); return;
    }
    if (!SetWindowRgn(window, region, FALSE)) {
        DeleteObject(region);
        if (old.x) SetPropW(window, kKeyShapeProperty, reinterpret_cast<HANDLE>((ULONG_PTR)old.x | ((ULONG_PTR)old.y << 16)));
        else RemovePropW(window, kKeyShapeProperty);
    }
}
