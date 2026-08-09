#include "windows_command_line.h"

#include <cassert>

int main()
{
    using halljoy::windows_command_line::QuoteArgument;
    assert(QuoteArgument(L"") == L"\"\"");
    assert(QuoteArgument(L"plain") == L"\"plain\"");
    assert(QuoteArgument(L"C:\\Program Files\\HallJoy\\runtime.dll") ==
        L"\"C:\\Program Files\\HallJoy\\runtime.dll\"");
    assert(QuoteArgument(L"trailing\\") == L"\"trailing\\\\\"");
    assert(QuoteArgument(L"a\\\"b") == L"\"a\\\\\\\"b\"");
    return 0;
}
