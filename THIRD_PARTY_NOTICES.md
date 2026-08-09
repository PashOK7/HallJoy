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

The official build copies this notice beside `HallJoy.exe`. Release archives
must retain it; sending a test executable alone is not the release package.

## Wooting Analog SDK/common library

- Headers and prebuilt common libraries are used by the isolated Universal Analog
  Plugin compatibility layer.
- Bundled library files are under `third_party/UniversalAnalogPluginFixed/`.
- Do not replace these binaries without updating the build preflight hashes/source
  attribution and reviewing the corresponding upstream license.

## ViGEmClient

- Headers and audited x64 import library are under
  `src/HallJoyProject/third_party/ViGEmClient/`.
- Copyright (c) 2017-2023 Nefarius Software Solutions e.U. and Contributors.
- License: MIT; the license text is preserved in the distributed headers.

## Device protocol documentation

Protocol notes, captures, mappings and reverse-engineering reports describe device
interoperability. They are not firmware licenses and do not grant permission to
redistribute vendor firmware images. Review `docs/firmware/` and `docs/research/`
before publishing those materials in a public repository or release asset.
