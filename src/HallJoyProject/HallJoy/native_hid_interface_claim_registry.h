#pragma once

#include "../../../third_party/UniversalAnalogPluginFixed/halljoy_native_hid_claim.h"

#include <algorithm>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace halljoy::native_hid
{
    template<typename Protocol>
    struct InterfaceClaim final
    {
        std::uint16_t vendor_id = 0;
        std::uint16_t product_id = 0;
        Protocol protocol{};
        std::string token;
    };

    template<typename Protocol>
    class InterfaceClaimRegistry final
    {
    public:
        bool Claim(std::uint16_t vendor_id, std::uint16_t product_id,
            std::wstring_view interface_path, Protocol protocol)
        {
            if (vendor_id == 0 || product_id == 0 || interface_path.empty())
                return false;
            const std::string token = MakeInterfaceClaimToken(interface_path);
            const auto existing = Find(token);
            if (existing != claims_.end())
                return existing->protocol == protocol;

            claims_.push_back({ vendor_id, product_id, protocol, token });
            std::sort(claims_.begin(), claims_.end(), [](const auto& left, const auto& right) {
                return left.token < right.token;
            });
            return true;
        }

        bool IsClaimed(std::wstring_view interface_path) const
        {
            const std::string token = MakeInterfaceClaimToken(interface_path);
            return !token.empty() && Find(token) != claims_.end();
        }

        bool IsClaimedBy(std::wstring_view interface_path, Protocol protocol) const
        {
            const std::string token = MakeInterfaceClaimToken(interface_path);
            const auto existing = token.empty() ? claims_.end() : Find(token);
            return existing != claims_.end() && existing->protocol == protocol;
        }

        bool ProtocolHasClaims(Protocol protocol) const
        {
            return std::any_of(claims_.begin(), claims_.end(), [=](const auto& claim) {
                return claim.protocol == protocol;
            });
        }

        void Reset() noexcept
        {
            claims_.clear();
        }

        const std::vector<InterfaceClaim<Protocol>>& Claims() const noexcept
        {
            return claims_;
        }

    private:
        auto Find(const std::string& token)
        {
            return std::find_if(claims_.begin(), claims_.end(), [&](const auto& claim) {
                return claim.token == token;
            });
        }

        auto Find(const std::string& token) const
        {
            return std::find_if(claims_.cbegin(), claims_.cend(), [&](const auto& claim) {
                return claim.token == token;
            });
        }

        std::vector<InterfaceClaim<Protocol>> claims_;
    };
}
