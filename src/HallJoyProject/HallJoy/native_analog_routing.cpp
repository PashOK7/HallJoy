#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

#include "native_analog_routing.h"

#include <algorithm>
#include <cwchar>
#include <mutex>
#include <string>
#include <vector>

namespace
{
struct Claim
{
    std::uint16_t vendorId = 0;
    std::uint16_t productId = 0;
    NativeAnalogProtocol protocol = NativeAnalogProtocol::Mad68A0;
};

std::mutex g_mutex;
std::vector<Claim> g_claims;

bool SameDevice(const Claim& claim, std::uint16_t vendorId, std::uint16_t productId)
{
    return claim.vendorId == vendorId && claim.productId == productId;
}

void PublishLocked()
{
    // Tokens match Windows HID paths directly and are exact VID/PID pairs.
    // Example: vid_373b&pid_1109;vid_372e&pid_105c
    std::wstring tokens;
    wchar_t token[32]{};
    for (const Claim& claim : g_claims)
    {
        if (!tokens.empty()) tokens.push_back(L';');
        swprintf_s(token, L"vid_%04x&pid_%04x",
            static_cast<unsigned>(claim.vendorId),
            static_cast<unsigned>(claim.productId));
        tokens += token;
    }

    if (tokens.empty())
        SetEnvironmentVariableW(L"HALLJOY_UAP_NATIVE_HID_IDS", nullptr);
    else
        SetEnvironmentVariableW(L"HALLJOY_UAP_NATIVE_HID_IDS", tokens.c_str());
}
}

void NativeAnalogRouting_Reset()
{
    std::lock_guard<std::mutex> lock(g_mutex);
    g_claims.clear();
    PublishLocked();
}

bool NativeAnalogRouting_Claim(
    std::uint16_t vendorId,
    std::uint16_t productId,
    NativeAnalogProtocol protocol)
{
    if (vendorId == 0 || productId == 0) return false;
    std::lock_guard<std::mutex> lock(g_mutex);
    const auto existing = std::find_if(g_claims.begin(), g_claims.end(),
        [=](const Claim& claim) { return SameDevice(claim, vendorId, productId); });
    if (existing != g_claims.end())
        return existing->protocol == protocol;

    g_claims.push_back({ vendorId, productId, protocol });
    std::sort(g_claims.begin(), g_claims.end(), [](const Claim& a, const Claim& b) {
        if (a.vendorId != b.vendorId) return a.vendorId < b.vendorId;
        return a.productId < b.productId;
    });
    PublishLocked();
    return true;
}

bool NativeAnalogRouting_IsClaimed(std::uint16_t vendorId, std::uint16_t productId)
{
    std::lock_guard<std::mutex> lock(g_mutex);
    return std::any_of(g_claims.begin(), g_claims.end(),
        [=](const Claim& claim) { return SameDevice(claim, vendorId, productId); });
}

bool NativeAnalogRouting_IsClaimedBy(
    std::uint16_t vendorId,
    std::uint16_t productId,
    NativeAnalogProtocol protocol)
{
    std::lock_guard<std::mutex> lock(g_mutex);
    return std::any_of(g_claims.begin(), g_claims.end(), [=](const Claim& claim) {
        return SameDevice(claim, vendorId, productId) && claim.protocol == protocol;
    });
}
