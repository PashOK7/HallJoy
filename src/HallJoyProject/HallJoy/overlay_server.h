#pragma once
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <cstdint>
#include <string>
#include "worker_lifecycle.h"

enum class OverlayFillDirection : int
{
    BottomUp = 0,
    TopDown = 1,
};

enum OverlayEffectFlags : uint32_t
{
    OverlayEffect_Smoothing = 1u << 0,
    OverlayEffect_Glass = 1u << 1,
    OverlayEffect_Bloom = 1u << 2,
    OverlayEffect_EdgeSweep = 1u << 3,
    OverlayEffect_MicroScale = 1u << 4,
    OverlayEffect_LabelContrast = 1u << 5,
    OverlayEffect_GlassRimLight = 1u << 6,
};

bool OverlayServer_Start(uint16_t port = 8765);
halljoy::lifecycle::StopResult OverlayServer_Stop();
bool OverlayServer_IsRunning();
uint16_t OverlayServer_GetPort();
uint16_t OverlayServer_GetConfiguredPort();
void OverlayServer_SetConfiguredPort(uint16_t port);
std::wstring OverlayServer_GetUrl();
std::wstring OverlayServer_GetLastError();
OverlayFillDirection OverlayServer_GetFillDirection();
void OverlayServer_SetFillDirection(OverlayFillDirection direction);
uint32_t OverlayServer_GetEffectFlags();
void OverlayServer_SetEffectFlags(uint32_t flags);
bool OverlayServer_GetEffectEnabled(uint32_t flag);
void OverlayServer_SetEffectEnabled(uint32_t flag, bool enabled);
uint32_t OverlayServer_GetAccentColor();
void OverlayServer_SetAccentColor(uint32_t rgb);
int OverlayServer_GetEffectStrengthPercent(uint32_t flag);
void OverlayServer_SetEffectStrengthPercent(uint32_t flag, int percent);
int OverlayServer_GetRefreshIntervalMs();
void OverlayServer_SetRefreshIntervalMs(int ms);
bool OverlayServer_GetAutoStart();
void OverlayServer_SetAutoStart(bool enabled);
bool OverlayServer_GetUseRawDepth();
void OverlayServer_SetUseRawDepth(bool enabled);
int OverlayServer_GetLabelFontIndex();
void OverlayServer_SetLabelFontIndex(int index);
int OverlayServer_GetLabelSizePx();
void OverlayServer_SetLabelSizePx(int px);
int OverlayServer_GetLabelShadowPercent();
void OverlayServer_SetLabelShadowPercent(int percent);
uint32_t OverlayServer_GetLabelColor();
void OverlayServer_SetLabelColor(uint32_t rgb);
