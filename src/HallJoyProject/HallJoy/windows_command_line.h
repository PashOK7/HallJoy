#pragma once

#include <string>
#include <string_view>

namespace halljoy::windows_command_line
{
// Quote one argument for CommandLineToArgvW using the backslash-before-quote
// rules required by CreateProcessW's single command-line string.
inline std::wstring QuoteArgument(std::wstring_view value)
{
    std::wstring quoted = L"\"";
    std::size_t backslashes = 0;
    for (const wchar_t c : value)
    {
        if (c == L'\\')
        {
            ++backslashes;
            continue;
        }
        if (c == L'\"')
        {
            quoted.append(backslashes * 2 + 1, L'\\');
            quoted += L'\"';
            backslashes = 0;
            continue;
        }
        quoted.append(backslashes, L'\\');
        backslashes = 0;
        quoted += c;
    }
    quoted.append(backslashes * 2, L'\\');
    quoted += L'\"';
    return quoted;
}
}
