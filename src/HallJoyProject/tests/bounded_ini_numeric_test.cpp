#include <cstdint>
#include <stdexcept>
#include <iostream>

#include "bounded_ini.h"

static void Check(bool ok) { if (!ok) throw std::runtime_error("INI file regression failed"); }

static void FileRoundtrip()
{
    wchar_t dir[MAX_PATH]{}, path[MAX_PATH]{};
    Check(GetTempPathW(MAX_PATH, dir) != 0);
    Check(GetTempFileNameW(dir, L"HJI", 0, path) != 0);
    struct Cleanup { const wchar_t* path; ~Cleanup() { DeleteFileW(path); } } cleanup{path};
    const auto write = [&](const wchar_t* value) {
        Check(WritePrivateProfileStringW(L"Test", L"Value", value, path) != FALSE);
    };
    write(L"731");
    std::int32_t signedValue = 0;
    std::uint32_t unsignedValue = 0;
    Check(halljoy::ini::ReadSigned(path, L"Test", L"Value", -1000, 1000, 17, signedValue) && signedValue == 731);
    Check(halljoy::ini::ReadUnsigned(path, L"Test", L"Value", 1000, 17, unsignedValue) && unsignedValue == 731);
    Check(halljoy::ini::ReadSigned(path, L"Test", L"Missing", -1000, 1000, 17, signedValue) && signedValue == 17);
    Check(halljoy::ini::ReadUnsigned(path, L"Test", L"Missing", 1000, 19, unsignedValue) && unsignedValue == 19);
    for (const auto* invalid : {L"731junk", L"2147483648", L"-2147483649"}) {
        write(invalid); signedValue = 42;
        Check(!halljoy::ini::ReadSigned(path, L"Test", L"Value", INT32_MIN, INT32_MAX, 17, signedValue));
        Check(signedValue == 42);
    }
    write(L"-731");
    Check(halljoy::ini::ReadSigned(path, L"Test", L"Value", -1000, 1000, 17, signedValue) && signedValue == -731);
    Check(!halljoy::ini::ReadUnsigned(path, L"Test", L"Value", UINT32_MAX, 17, unsignedValue));
    for (std::size_t capacity : {2u, 7u, 128u, 255u, 256u, 257u, 300u, 513u}) {
        const std::wstring complete(capacity - 2, L'x');
        write(complete.c_str());
        std::wstring read;
        Check(halljoy::ini::Read(path, L"Test", L"Value", read, capacity) && read == complete);
        const std::wstring truncated(capacity, L'x');
        write(truncated.c_str()); read = L"unchanged";
        Check(!halljoy::ini::Read(path, L"Test", L"Value", read, capacity) && read == L"unchanged");
    }
    std::wstring read = L"unchanged";
    Check(!halljoy::ini::Read(path, L"Test", L"Value", read, 0));
    Check(!halljoy::ini::Read(path, L"Test", L"Value", read, 1));
    Check(read == L"unchanged");
    std::cout << "INI_REAL_FILE_ROUNDTRIP_AND_CAPACITY_BOUNDARIES=PASS\n";
}

int main() try
{
    FileRoundtrip();
    using halljoy::ini::Signed;
    using halljoy::ini::Unsigned;
    std::uint32_t u = 0;
    Check(Unsigned(L"0", 10, u) && u == 0);
    Check(Unsigned(L" 0x0A\t", 10, u) && u == 10);
    Check(!Unsigned(L"+1", 10, u));
    Check(!Unsigned(L"-1", 10, u));
    Check(!Unsigned(L"1junk", 10, u));
    Check(!Unsigned(L"4294967296", UINT32_MAX, u));

    std::int32_t s = 0;
    Check(Signed(L"-2147483648", INT32_MIN, INT32_MAX, s) && s == INT32_MIN);
    Check(Signed(L"+2147483647", INT32_MIN, INT32_MAX, s) && s == INT32_MAX);
    Check(!Signed(L"-2147483649", INT32_MIN, INT32_MAX, s));
    Check(!Signed(L"2147483648", INT32_MIN, INT32_MAX, s));
    Check(!Signed(L"-0x1", INT32_MIN, INT32_MAX, s));
    Check(!Signed(L"--1", INT32_MIN, INT32_MAX, s));
    Check(!Signed(L"18446744073709551616", 0, 0, s));
    Check(!Signed(L"-18446744073709551616", 0, 0, s));
    Check(Signed(L"-5", -10, -1, s) && s == -5);
    Check(!Signed(L"5", -10, -1, s));
    Check(Signed(L"5", 1, 10, s) && s == 5);
    Check(!Signed(L"-5", 1, 10, s));
    return 0;
}
catch (const std::exception& error) {
    std::cerr << error.what() << '\n';
    return 1;
}
