#include "profile_ini.h"
#include "ini_util.h"
#include <cassert>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>

// Save paths are not exercised in this loader-only executable.
HallJoyPersistence::SaveResult IniUtil_SaveAtomic(const wchar_t*, IniUtilWriteCallback,
    IniUtilValidateCallback, void*) { std::abort(); }
void IniUtil_ReportSaveFailure(const wchar_t*, const wchar_t*, const HallJoyPersistence::SaveResult&) { std::abort(); }
int main() {
    namespace fs = std::filesystem;
    const auto root = fs::temp_directory_path() / ("halljoy-profile-loader-" + std::to_string(GetCurrentProcessId()));
    assert(fs::create_directory(root));
    const auto path = root / "fixture.ini";
    Bindings_SetAxisPlus(Axis::LX, 26);
    assert(!Profile_LoadIni(path.c_str()));
    for (const auto invalid : {"", "unrelated", "[Pad1_Axes]\nUnrelated=7\n", "[Pad1_Axes]\nLX_Plus=65543\n",
            "[Pad1_Axes]\nLX_Plus=-1\n", "[Pad1_Axes]\nLX_Plus=7junk\n",
            "[Pad1_Buttons]\nA=7,65543\n"}) {
        { std::ofstream file(path); file << invalid; }
        assert(!Profile_LoadIni(path.c_str()));
        assert(Bindings_GetAxis(Axis::LX).plusHid==26);
    }
    { std::ofstream file(path); file << "[Pad1_Buttons]\nA=";
      for (unsigned i=1; i<halljoy::keycode::kCount; ++i) file << (i==1?"":",") << i;
      file << "\n"; }
    assert(Profile_LoadIni(path.c_str()));
    for (unsigned i=1; i<halljoy::keycode::kCount; ++i)
        assert(Bindings_ButtonHasHid(GameButton::A, static_cast<uint16_t>(i)));
    fs::remove(path); fs::remove(root);
    std::cout << "PROFILE_LOADER_WINDOWS_TEST=PASS invalid_unchanged=1 full_domain=1\n";
}
