#pragma once
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <string>

#include <objidl.h>
#include <gdiplus.h>

struct CustomSliderMetrics
{
    int pad = 0;
    int trackLeft = 0;
    int trackRight = 0;
};

void CustomPage_DrawText(HDC hdc, const std::wstring& text, RECT rc, COLORREF color, UINT fmt);
void CustomPage_DrawRoundRect(Gdiplus::Graphics& g, const RECT& rc, COLORREF fill, COLORREF border, float radius, BYTE alpha = 255);
void CustomPage_DrawButton(Gdiplus::Graphics& g, HDC hdc, const RECT& rc, const std::wstring& text, bool hot, bool pressed, bool enabled);
void CustomPage_DrawChip(Gdiplus::Graphics& g, HDC hdc, const RECT& rc, const std::wstring& text, bool enabled = true);
void CustomPage_DrawCheckbox(Gdiplus::Graphics& g, HDC hdc, HWND hWnd, const RECT& rc, const std::wstring& text, bool checked, bool enabled = true);
void CustomPage_DrawSlider(Gdiplus::Graphics& g, HWND hWnd, const RECT& rc, int minV, int maxV, int value);
int CustomPage_SliderValueFromPoint(HWND hWnd, const RECT& rc, int minV, int maxV, int x);
CustomSliderMetrics CustomPage_GetSliderMetrics(HWND hWnd, const RECT& rc);
