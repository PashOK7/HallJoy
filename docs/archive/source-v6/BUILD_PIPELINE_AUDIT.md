# HallJoy MAD68 Pro R v3.5.0 build-pipeline audit

## v3.4.3 failure

The failure was in neither C++ nor the MAD68 patch. Soup patching completed and
printed `Soup Madlions SafeHID fix v7 applied`. A subsequent
`git diff --check` returned exit code 0 but wrote an LF-to-CRLF warning to
stderr. Under Windows PowerShell with `$ErrorActionPreference = 'Stop'`, that
stderr text could become `NativeCommandError` before the script reliably read
Git's real exit code.

The latent defect appeared only on some machines and Git configurations. A
one-line workaround would not have addressed the broader problems: arbitrary
stderr from native tools could abort the build, `$LASTEXITCODE` could be stale,
dependencies were patched in the user's unpacked tree, failed runs left partial
state, global Git pager/color/EOL settings affected results, mutable/cached Sun
state could be reused, parallel builds shared one cache, long download paths
hurt Windows tools, stale outputs survived, unused ABI0 was built, and tool and
binary validation happened too late.

## v3.5.0 pipeline

```text
BUILD_HALLJOY.cmd
  -> tools/build_halljoy.ps1
      -> complete tool/source preflight
      -> single-build mutex
      -> isolated LocalAppData cache
      -> private UAP build
          -> exact Soup revision; reset + clean
          -> patch five expected files
          -> LF and exact-diff validation
          -> pager-free git diff --check
          -> exact Sun tag; reset + clean + submodule restore
          -> fresh Sun build
          -> build only the consumed ABI1 plugin
          -> validate DLL as AMD64 PE
      -> clean short-path HallJoyProject copy
      -> place ABI1 DLL in the runtime-resource path
      -> MSBuild HallJoy with native MAD68 support
      -> validate EXE as AMD64 PE
      -> recreate BUILD_OUTPUT
```

All native programs run through `Invoke-HJNative` using
`Start-Process -Wait -PassThru`; success is determined exclusively by
`System.Diagnostics.Process.ExitCode`. Stderr remains diagnostic text and no
longer constitutes failure by itself. Direct `&git`, `&clang`, `&msbuild`, and
`&Sun` calls and reliance on `$LASTEXITCODE` were removed.

## Reproducibility and isolation

Each Git invocation receives local noninteractive policy:

```text
core.autocrlf=false
core.eol=lf
core.safecrlf=false
core.pager=cat
pager.diff=false
color.ui=false
advice.detachedHead=false
GIT_PAGER=cat
GIT_TERMINAL_PROMPT=0
```

Build work occurs under
`%LOCALAPPDATA%\HallJoyBuildCache\MAD68ProR-v3.5.0`, not in the extracted source
archive. Soup and HallJoy use separate temporary trees; every run clears prior
outputs. The source tree receives only `BUILD_LOG.txt` and `BUILD_OUTPUT`.

Soup is pinned to commit `b02796b0b20276277c8a4b4d3759643eeab43ff7`.
Before patching, the tree is reset and cleaned. After patching, exactly five
known files may differ; each must contain no CR byte before
`git diff --check` runs. Sun is pinned to tag `0.5.0`, reset, cleaned, has its
submodules restored, and is rebuilt even when cached; an old `Sun.exe` is never
accepted as final output.

## ABI1-only output

HallJoy loads embedded resource `IDR_UAP_ABIV1`; it never consumes ABI0 from the
runtime path. v3.5.0 therefore builds only:

```text
abiv1-pluswooting-mad68native.dll -> abiv1.dll
```

This removes an unused compile/link stage without removing a HallJoy feature.

## Validation

Before MSBuild, the pipeline verifies every `ClCompile`, `ClInclude`,
`ResourceCompile`, and `Image` reference; exact ViGEmClient and Wooting library
size/hash; ABI1/UAP/Wooting flags; MAD68 pre-open exclusion and A8/A9 allowlist;
Raw Input startup ordering; and steady-state/per-key fallback protection.

After build, both DLL and EXE must have a DOS header, valid PE signature, and
AMD64 machine type. Only then are a fresh `BUILD_OUTPUT` and SHA-256 manifest
created.

Recorded static result:

```text
Protocol test: PASS - 68 descriptors, 67 HID keys
Runtime source identity against v3.4.3: PASS
Build pipeline checks: 38/38 PASS
vcxproj references: PASS
Critical library hashes: PASS
PowerShell lexical balance: PASS
Single CMD entry point: PASS
```

MAD68 runtime, UI, remapping, and ViGEm sources remained identical to v3.4.3;
only dependency preparation and build orchestration changed.

The build may still fail correctly when a required tool/workload is absent,
three GitHub attempts fail, another process holds output, a compiler returns
nonzero, an output is absent/too small/not AMD64 PE, or a pinned revision no
longer matches the expected patch structure. These conditions must produce a
specific error rather than failure on harmless stderr.

The original audit environment could not execute the complete MSVC/Windows SDK
link. Static, protocol, structural, and packaging checks passed, while final
Windows compilation remained mandatory.
