# HallJoy v1.4 reproducible build contract

## Authoritative lock

`tools/dependency-lock.json` is the machine-readable authority for remote
source revisions, binary input integrity, GitHub Action commits, runner labels,
and required toolchain families.

The official build consumes the lock directly for:

- the exact Sun commit used to generate the private UAP;
- the exact Soup commit patched for HallJoy routing;
- five reviewed HallJoy Soup overlay files and their normalized SHA-256 values;
- the path, size, and SHA-256 of `ViGEmClient.lib`.

The private Sun bootstrap never accepts an unrelated `sun` command from
`PATH`. Sun and Soup are fetched by full 40-character commit and checked out in
detached mode. The build then copies only the five locked overlay files, checks
their hashes before and after copying, and rejects every additional changed or
untracked Soup file. Moving branches and release tags are not build inputs.

GitHub Actions are referenced by full commit SHA. Human-readable major
versions remain comments only. Runner labels are fixed to `ubuntu-24.04` and
`windows-2022`; `latest` aliases are forbidden by the lock audit.

## Local and CI gates

The portable CI job and local gate both run:

```text
python tools/run_native_backend_checks.py --require-compiler
```

The official Windows job and local build both run:

```text
powershell -NoProfile -ExecutionPolicy Bypass -File .\tools\build.ps1
```

The build script also executes the portable gate, rebuilds the pinned private
UAP, verifies the ViGEm library, and performs a clean MSVC x64 Release rebuild.
The same dependency lock is copied into `build/output` and uploaded with the CI
artifact so its provenance remains inspectable beside the executable.

## Warning policy

New MSVC compiler or linker warnings fail the official build. The only current
allowlisted diagnostic is `LNK4099` for the absent `ViGEmClient.pdb` referenced
by the pinned prebuilt `ViGEmClient.lib`. MSBuild prints that same diagnostic at
the link site and again in its summary; it is one known warning code, not two
independent conditions.

The warning can be removed only by replacing the pinned binary input with a
verified build that includes matching symbols. Expanding the allowlist requires
an explicit decision and updated validation evidence.

## Updating dependencies

1. Select and review an exact upstream commit or binary.
2. Update `tools/dependency-lock.json` in the same change as any required code.
3. For a binary input, update both size and SHA-256.
4. For a Soup change, update the reviewed overlay file and its normalized
   SHA-256; do not rely on an ignored local Soup working tree.
5. Run the dependency-lock audit, portable tests, and full Windows build.
6. Record the reason, old value, new value, and results in the v1.4 worklog.
7. Run the documented clean-room build before marking the package `Verified`.

`dependency_lock_static_audit.py` rejects mutable Actions references, moving
runner aliases, build scripts that bypass the lock, a missing portable compiler
gate, or removal of warning enforcement.
