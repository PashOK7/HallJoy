// This executable must fail under AddressSanitizer. It is never part of a
// production build or normal test run; the sanitizer runner uses it solely to
// prove that the loaded runtime reports an actual memory violation.
#include <cstdint>

int main()
{
    volatile auto* bytes = new std::uint8_t[1]{};
    bytes[1] = 0x5Au; // Intentional one-byte heap overflow.
    delete[] bytes;
    return 0;
}
