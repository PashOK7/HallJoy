#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

#include "native_analog_routing.h"
#include "native_hid_interface_claim_registry.h"

#include <mutex>
#include <string>

namespace
{
std::mutex g_mutex;
halljoy::native_hid::InterfaceClaimRegistry<NativeAnalogProtocol> g_claims;

void PublishLocked()
{
    // Tokens are compact fingerprints of complete normalized HID interface
    // paths. A sibling interface with the same VID/PID receives a different
    // token and remains available to UAP unless it is independently claimed.
    std::wstring tokens;
    for (const auto& claim : g_claims.Claims())
    {
        if (!tokens.empty()) tokens.push_back(L';');
        tokens.append(claim.token.begin(), claim.token.end());
    }

    if (tokens.empty())
        SetEnvironmentVariableW(L"HALLJOY_UAP_NATIVE_HID_PATHS", nullptr);
    else
        SetEnvironmentVariableW(L"HALLJOY_UAP_NATIVE_HID_PATHS", tokens.c_str());
}
}

void NativeAnalogRouting_Reset()
{
    std::lock_guard<std::mutex> lock(g_mutex);
    g_claims.Reset();
    PublishLocked();
}

bool NativeAnalogRouting_Claim(
    std::uint16_t vendorId,
    std::uint16_t productId,
    const wchar_t* interfacePath,
    NativeAnalogProtocol protocol)
{
    if (interfacePath == nullptr) return false;
    std::lock_guard<std::mutex> lock(g_mutex);
    if (!g_claims.Claim(vendorId, productId, interfacePath, protocol))
        return false;
    PublishLocked();
    return true;
}

bool NativeAnalogRouting_IsClaimed(const wchar_t* interfacePath)
{
    if (interfacePath == nullptr) return false;
    std::lock_guard<std::mutex> lock(g_mutex);
    return g_claims.IsClaimed(interfacePath);
}

bool NativeAnalogRouting_IsClaimedBy(
    const wchar_t* interfacePath,
    NativeAnalogProtocol protocol)
{
    if (interfacePath == nullptr) return false;
    std::lock_guard<std::mutex> lock(g_mutex);
    return g_claims.IsClaimedBy(interfacePath, protocol);
}

bool NativeAnalogRouting_ProtocolHasClaims(NativeAnalogProtocol protocol)
{
    std::lock_guard<std::mutex> lock(g_mutex);
    return g_claims.ProtocolHasClaims(protocol);
}
