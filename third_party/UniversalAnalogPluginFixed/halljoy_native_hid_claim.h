#pragma once

#include <cstdint>
#include <string>
#include <string_view>

namespace halljoy::native_hid
{
    struct InterfaceFingerprint final
    {
        std::uint64_t hash = 14695981039346656037ull;
        std::uint32_t utf16_units = 0;
    };

    constexpr std::uint16_t NormalizeUnit(std::uint16_t unit) noexcept
    {
        if (unit >= static_cast<std::uint16_t>('A') &&
            unit <= static_cast<std::uint16_t>('Z'))
        {
            unit = static_cast<std::uint16_t>(unit + ('a' - 'A'));
        }
        if (unit == static_cast<std::uint16_t>('/'))
            unit = static_cast<std::uint16_t>('\\');
        return unit;
    }

    constexpr void AddUtf16Unit(InterfaceFingerprint& out, std::uint16_t unit) noexcept
    {
        unit = NormalizeUnit(unit);
        constexpr std::uint64_t prime = 1099511628211ull;
        out.hash ^= static_cast<std::uint8_t>(unit & 0xffu);
        out.hash *= prime;
        out.hash ^= static_cast<std::uint8_t>(unit >> 8u);
        out.hash *= prime;
        ++out.utf16_units;
    }

    constexpr void AddCodePoint(InterfaceFingerprint& out, std::uint32_t code_point) noexcept
    {
        if (code_point <= 0xffffu)
        {
            AddUtf16Unit(out, static_cast<std::uint16_t>(code_point));
            return;
        }
        if (code_point > 0x10ffffu)
            code_point = 0xfffdu;
        code_point -= 0x10000u;
        AddUtf16Unit(out, static_cast<std::uint16_t>(0xd800u + (code_point >> 10u)));
        AddUtf16Unit(out, static_cast<std::uint16_t>(0xdc00u + (code_point & 0x3ffu)));
    }

    inline InterfaceFingerprint FingerprintWide(std::wstring_view path) noexcept
    {
        InterfaceFingerprint out{};
        for (const wchar_t value : path)
        {
            if constexpr (sizeof(wchar_t) == 2)
                AddUtf16Unit(out, static_cast<std::uint16_t>(value));
            else
                AddCodePoint(out, static_cast<std::uint32_t>(value));
        }
        return out;
    }

    inline InterfaceFingerprint FingerprintUtf8(std::string_view path) noexcept
    {
        InterfaceFingerprint out{};
        std::size_t index = 0;
        while (index < path.size())
        {
            const auto lead = static_cast<std::uint8_t>(path[index]);
            std::uint32_t code_point = 0xfffdu;
            std::size_t width = 1;
            if (lead < 0x80u)
            {
                code_point = lead;
            }
            else if ((lead & 0xe0u) == 0xc0u && index + 1 < path.size())
            {
                const auto b1 = static_cast<std::uint8_t>(path[index + 1]);
                if ((b1 & 0xc0u) == 0x80u)
                {
                    const std::uint32_t decoded = ((lead & 0x1fu) << 6u) | (b1 & 0x3fu);
                    if (decoded >= 0x80u)
                    {
                        code_point = decoded;
                        width = 2;
                    }
                }
            }
            else if ((lead & 0xf0u) == 0xe0u && index + 2 < path.size())
            {
                const auto b1 = static_cast<std::uint8_t>(path[index + 1]);
                const auto b2 = static_cast<std::uint8_t>(path[index + 2]);
                if ((b1 & 0xc0u) == 0x80u && (b2 & 0xc0u) == 0x80u)
                {
                    const std::uint32_t decoded = ((lead & 0x0fu) << 12u) |
                        ((b1 & 0x3fu) << 6u) | (b2 & 0x3fu);
                    if (decoded >= 0x800u && (decoded < 0xd800u || decoded > 0xdfffu))
                    {
                        code_point = decoded;
                        width = 3;
                    }
                }
            }
            else if ((lead & 0xf8u) == 0xf0u && index + 3 < path.size())
            {
                const auto b1 = static_cast<std::uint8_t>(path[index + 1]);
                const auto b2 = static_cast<std::uint8_t>(path[index + 2]);
                const auto b3 = static_cast<std::uint8_t>(path[index + 3]);
                if ((b1 & 0xc0u) == 0x80u && (b2 & 0xc0u) == 0x80u &&
                    (b3 & 0xc0u) == 0x80u)
                {
                    const std::uint32_t decoded = ((lead & 0x07u) << 18u) |
                        ((b1 & 0x3fu) << 12u) | ((b2 & 0x3fu) << 6u) | (b3 & 0x3fu);
                    if (decoded >= 0x10000u && decoded <= 0x10ffffu)
                    {
                        code_point = decoded;
                        width = 4;
                    }
                }
            }
            AddCodePoint(out, code_point);
            index += width;
        }
        return out;
    }

    inline void AppendHex(std::string& out, std::uint64_t value, unsigned digits)
    {
        static constexpr char hex[] = "0123456789abcdef";
        for (unsigned shift = digits; shift-- != 0;)
            out.push_back(hex[(value >> (shift * 4u)) & 0x0fu]);
    }

    inline std::string MakeInterfaceClaimToken(InterfaceFingerprint fingerprint)
    {
        std::string token = "path_";
        token.reserve(5 + 16 + 1 + 8);
        AppendHex(token, fingerprint.hash, 16);
        token.push_back('_');
        AppendHex(token, fingerprint.utf16_units, 8);
        return token;
    }

    inline std::string MakeInterfaceClaimToken(std::wstring_view path)
    {
        return path.empty() ? std::string{} : MakeInterfaceClaimToken(FingerprintWide(path));
    }

    inline std::string MakeInterfaceClaimTokenUtf8(std::string_view path)
    {
        return path.empty() ? std::string{} : MakeInterfaceClaimToken(FingerprintUtf8(path));
    }

    constexpr char FoldAscii(char value) noexcept
    {
        return value >= 'A' && value <= 'Z' ? static_cast<char>(value + ('a' - 'A')) : value;
    }

    inline bool EqualTokenAsciiCi(std::string_view left, std::string_view right) noexcept
    {
        if (left.size() != right.size())
            return false;
        for (std::size_t i = 0; i < left.size(); ++i)
        {
            if (FoldAscii(left[i]) != FoldAscii(right[i]))
                return false;
        }
        return true;
    }

    inline bool TokenListContains(std::string_view list, std::string_view wanted) noexcept
    {
        if (wanted.empty())
            return false;
        std::size_t begin = 0;
        for (;;)
        {
            const std::size_t end = list.find(';', begin);
            const auto token = list.substr(begin,
                end == std::string_view::npos ? list.size() - begin : end - begin);
            if (EqualTokenAsciiCi(token, wanted))
                return true;
            if (end == std::string_view::npos)
                return false;
            begin = end + 1;
        }
    }
}
