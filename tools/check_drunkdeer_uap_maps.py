"""Compile all generated DrunkDeer identities against the actual patched Soup.

Run after staging pinned Soup, before packaging. No HID access or plugin load.
"""
import argparse
import shutil
import subprocess
import tempfile
from pathlib import Path

ROOT=Path(__file__).resolve().parents[1]
CODE=r'''
#include "HidScancode.hpp"
#include "halljoy_drunkdeer_maps.h"
using namespace halljoy::drunkdeer_identity;
constexpr bool Check(const std::array<std::uint16_t,126>& map) {
    for(auto hid:map) {
        if(!hid || hid==0x409 || hid==0x403) continue;
        if(hid>255) return false;
        const auto key=soup::hid_scancode_to_soup_key(static_cast<std::uint8_t>(hid));
        if(key==soup::KEY_NONE || key>=soup::NUM_KEYS || soup::soup_key_to_hid_scancode(key)!=hid) return false;
    }
    return true;
}
static_assert(Check(kA75Ansi) && Check(kA75Pro) && Check(kA75Iso));
static_assert(Check(kG60) && Check(kG65) && Check(kG75Ansi) && Check(kG75Jis));
static_assert(soup::KEY_INTL_HASH>soup::KEY_OEM_10);
static_assert(soup::KEY_INTL_RO!=soup::KEY_BACKSLASH && soup::KEY_INTL_YEN!=soup::KEY_INTL_RO);
static_assert(soup::NUM_KEYS<255);
int main() { return 0; }
'''

if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--compiler');args=parser.parse_args()
    cxx=args.compiler or shutil.which('clang++') or 'C:/Program Files/LLVM/bin/clang++.exe'
    with tempfile.TemporaryDirectory(prefix='HallJoyDrunkDeerMap-') as temp:
        target=Path(temp)/'roundtrip.exe'
        includes=[ROOT/'third_party/UniversalAnalogPluginFixed/overlay/Soup/soup',
                  ROOT/'.cache/uap/Soup/soup',ROOT/'third_party/UniversalAnalogPluginFixed']
        subprocess.run([cxx,'-std=c++20','-x','c++','-',*[f'-I{p}' for p in includes],'-o',str(target)],
                       input=CODE,text=True,check=True,timeout=60)
        subprocess.run([str(target)],check=True,timeout=10)
    print('DRUNKDEER_SOUP_ROUNDTRIP=PASS all_7_models')
