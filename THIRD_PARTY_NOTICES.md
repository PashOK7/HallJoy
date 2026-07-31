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

The Universal Analog Plugin build script obtains the Soup/Sun sources used by the
plugin. Their upstream license files must remain with generated source/build
artifacts and release compliance material. HallJoy patches only the documented HID
routing boundary and plugin integration points.

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
