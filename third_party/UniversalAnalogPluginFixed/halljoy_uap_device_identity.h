#pragma once

#include <cstdint>
#include <string_view>

namespace halljoy::uap
{
    struct DeviceIdentityInput
    {
        std::uint16_t vendor_id = 0;
        std::uint16_t product_id = 0;
        std::uint16_t usage_page = 0;
        std::uint16_t usage = 0;
        std::string_view hid_path{};
        std::string_view manufacturer{};
        std::string_view device_name{};
    };

    struct DeviceIdentity
    {
        std::uint64_t id = 0;
        bool duplicate_safe = false;
    };

    namespace identity_detail
    {
        constexpr std::uint64_t kFnvOffset = 1469598103934665603ull;
        constexpr std::uint64_t kFnvPrime = 1099511628211ull;

        constexpr void HashByte(std::uint64_t& hash, std::uint8_t value) noexcept
        {
            hash ^= value;
            hash *= kFnvPrime;
        }

        constexpr void HashU16(std::uint64_t& hash, std::uint16_t value) noexcept
        {
            HashByte(hash, static_cast<std::uint8_t>(value));
            HashByte(hash, static_cast<std::uint8_t>(value >> 8));
        }

        constexpr void HashU32(std::uint64_t& hash, std::uint32_t value) noexcept
        {
            for (unsigned shift = 0; shift != 32; shift += 8)
            {
                HashByte(hash, static_cast<std::uint8_t>(value >> shift));
            }
        }

        constexpr void HashU64(std::uint64_t& hash, std::uint64_t value) noexcept
        {
            for (unsigned shift = 0; shift != 64; shift += 8)
            {
                HashByte(hash, static_cast<std::uint8_t>(value >> shift));
            }
        }

        constexpr char NormalizeAscii(char value, bool path) noexcept
        {
            if (value >= 'A' && value <= 'Z')
            {
                value = static_cast<char>(value - 'A' + 'a');
            }
            if (path && value == '/')
            {
                value = '\\';
            }
            return value;
        }

        constexpr void HashString(
            std::uint64_t& hash,
            std::uint8_t field_tag,
            std::string_view value,
            bool path = false) noexcept
        {
            HashByte(hash, field_tag);
            HashU64(hash, static_cast<std::uint64_t>(value.size()));
            for (const char ch : value)
            {
                HashByte(hash, static_cast<std::uint8_t>(NormalizeAscii(ch, path)));
            }
        }

        constexpr std::uint64_t Finalize(std::uint64_t hash) noexcept
        {
            // MurmurHash3's 64-bit finalizer removes the weak low-bit structure
            // of FNV while keeping the complete operation deterministic.
            hash ^= hash >> 33;
            hash *= 0xff51afd7ed558ccdull;
            hash ^= hash >> 33;
            hash *= 0xc4ceb9fe1a85ec53ull;
            hash ^= hash >> 33;
            return hash != 0 ? hash : 1;
        }
    }

    [[nodiscard]] constexpr std::uint64_t MakeDeviceIdentityBase(
        const DeviceIdentityInput& input) noexcept
    {
        std::uint64_t hash = identity_detail::kFnvOffset;
        identity_detail::HashString(hash, 0x01, "HallJoy/UAP/device-id/v2");
        identity_detail::HashU16(hash, input.vendor_id);
        identity_detail::HashU16(hash, input.product_id);
        identity_detail::HashU16(hash, input.usage_page);
        identity_detail::HashU16(hash, input.usage);
        if (!input.hid_path.empty())
        {
            identity_detail::HashString(hash, 0x10, input.hid_path, true);
        }
        else
        {
            identity_detail::HashString(hash, 0x20, input.manufacturer);
            identity_detail::HashString(hash, 0x21, input.device_name);
        }
        return identity_detail::Finalize(hash);
    }

    [[nodiscard]] constexpr DeviceIdentity MakeDeviceIdentity(
        const DeviceIdentityInput& input,
        std::uint32_t fallback_occurrence) noexcept
    {
        const std::uint64_t base = MakeDeviceIdentityBase(input);
        if (!input.hid_path.empty())
        {
            return DeviceIdentity{ base, true };
        }

        std::uint64_t hash = base;
        identity_detail::HashByte(hash, 0x30);
        identity_detail::HashU32(hash, fallback_occurrence);
        return DeviceIdentity{ identity_detail::Finalize(hash), false };
    }
}
