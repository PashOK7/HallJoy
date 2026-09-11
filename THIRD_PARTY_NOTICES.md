# Third-party notices

HallJoy includes or builds against the following third-party components. Their
licenses apply to those components independently of HallJoy's own license.

## Universal Analog Plugin

- Bundled under `third_party/UniversalAnalogPluginFixed/`.
- Copyright (c) 2023-2025 Calamity, Inc.
- License: MIT.
- License text: `third_party/UniversalAnalogPluginFixed/LICENCE`.
- HallJoy carries local isolation, telemetry and exact native-device exclusion patches.

## Soup and Sun

- Soup source: `calamity-inc/Soup`, pinned by `tools/dependency-lock.json` to
  `b02796b0b20276277c8a4b4d3759643eeab43ff7`.
- Soup Copyright (c) 2021-2026 Calamity, Inc.; license: MIT.
- Sun build tool: `calamity-inc/Sun`, pinned by `tools/dependency-lock.json` to
  `83c195bd61314bdbfdccc161653dbb652e3b6678`.
- Sun Copyright (c) 2022-2025 Calamity, Inc.; license: MIT.
- HallJoy modifies five locked Soup HID/plugin integration files. Every overlay
  hash is recorded in the dependency lock; neither repository tracks a moving
  branch during an official build.

The MIT license for Universal Analog Plugin, Soup and Sun grants permission to
use, copy, modify, merge, publish, distribute, sublicense and/or sell copies,
provided that the applicable copyright notice and permission notice are kept in
all copies or substantial portions. The full license text is:

> Permission is hereby granted, free of charge, to any person obtaining a copy
> of this software and associated documentation files (the "Software"), to deal
> in the Software without restriction, including without limitation the rights
> to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
> copies of the Software, and to permit persons to whom the Software is
> furnished to do so, subject to the following conditions:
>
> The above copyright notice and this permission notice shall be included in all
> copies or substantial portions of the Software.
>
> THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
> IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
> FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
> AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
> LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
> OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
> SOFTWARE.

The official build copies this notice beside `HallJoy.exe`. GitHub Releases
publishes it as accompanying license documentation; it is not a runtime
dependency. Users can download and run HallJoy.exe without placing this document
beside it. Redistributors must preserve the applicable notices and source access.

HallJoy source for release 1.5.1:
https://github.com/PashOK7/HallJoy/tree/v1.5.1
HallJoy's AGPL-3.0 license is supplied as the separate LICENSE release asset.

## Wooting Analog SDK/common library

- Headers and prebuilt common libraries are used by the isolated Universal Analog
  Plugin compatibility layer.
- Bundled library files are under `third_party/UniversalAnalogPluginFixed/`.
- License: Mozilla Public License 2.0 (MPL-2.0).
- Upstream source: https://github.com/WootingKb/wooting-analog-sdk
- License text: https://www.mozilla.org/MPL/2.0/
- Covered source remains available under MPL-2.0; HallJoy's license does not
  restrict recipients' rights to those components under MPL-2.0.
- Do not replace these binaries without updating the build preflight hashes/source
  attribution and reviewing the corresponding upstream license.

## ViGEmClient

- Headers and audited x64 import library are under
  `src/HallJoyProject/third_party/ViGEmClient/`.
- Copyright (c) 2017-2023 Nefarius Software Solutions e.U. and Contributors.
- License: MIT; the full permission and warranty notice reproduced above also
  applies to ViGEmClient with its copyright notice listed here.

## ViGEmBus

- The official ViGEmBus 1.22.0 installer is embedded in `HallJoy.exe` so a
  missing driver can be installed without a runtime download.
- Source asset:
  `ViGEmBus_1.22.0_x64_x86_arm64.exe`, 6,278,576 bytes, SHA-256
  `89220A7865076B342892F98865F3499FB7C4CFD673159E89D352C360FD014C6A`.
- Copyright (c) 2016-2020, Nefarius Software Solutions e.U.
- License: BSD 3-Clause License. The preserved source copy is
  `src/HallJoyProject/third_party/ViGEmBus/LICENSE`.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

1. Redistributions of source code must retain the above copyright notice, this
   list of conditions and the following disclaimer.
2. Redistributions in binary form must reproduce the above copyright notice,
   this list of conditions and the following disclaimer in the documentation
   and/or other materials provided with the distribution.
3. Neither the name of the copyright holder nor the names of its contributors
   may be used to endorse or promote products derived from this software without
   specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

## Device protocol documentation

Protocol notes, captures, mappings and reverse-engineering reports describe device
interoperability. They are not firmware licenses and do not grant permission to
redistribute vendor firmware images. Review `docs/firmware/` and `docs/research/`
before publishing those materials in a public repository or release asset.
