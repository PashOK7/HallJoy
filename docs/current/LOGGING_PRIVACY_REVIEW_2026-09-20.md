# Logging policy and public-report review — 2026-09-20

## Owner policy (explicit clarification)

Enable logging controls optional continuous logging when the user needs to
investigate behavior HallJoy has not identified as a failure. It is NOT a master
ban on logging. Crashes, missing supported keyboards and other recognized special
failure conditions must produce automatic reports even when the checkbox is OFF.
Do not remove automatic incident logging or ask this question again without new
contradicting requirements. Diagnostic builds may force logging.

## Findings and change

Ordinary support_log.cpp collects structural categories, numeric errors, USB
VID/PID/interface metadata and aggregate telemetry. It intentionally excludes
full HID paths, serial strings, user-editable device names and individual key
values. Its only OverlaySummary caller formats performance counters, not labels,
input text or network addresses. Detailed DebugLog writes compile out in ordinary
Release; explicit diagnostics are a different format/privacy scope.

Confirmed privacy exposure: ordinary HallJoyCrash.txt called WriteDiagnosticContext,
which writes CPU registers and raw stack words. Arbitrary process-memory fragments
could therefore enter a public crash report despite the absence of a minidump.
Guarded that call with HALLJOY_DIAGNOSTIC. Ordinary crashes retain exception code,
module basename/offset, time, thread and a production-safe checkpoint, plus
memory_context=omitted_in_production. The crash report itself remains mandatory.
Production checkpoint collection is disabled independently. Detailed diagnostic
crash reports may still include memory and are not covered by the ordinary
support-report privacy findings.

The source review found no keyboard text, serial or personal-path output in the
ordinary support formatter. This is not a claim that all diagnostics or previously
generated files are sanitized. Timestamps, uptime, models and activity/error
counters remain visible diagnostic information.

## Tests and limits

Extended the real Windows support writer test with private device/manufacturer
sentinels and100,000 queued structural events. Ordinary and forced input-path
variants passed: no sentinel leakage, reported queue overflow, file within4 MiB,
normal OFF/no-file behavior, mandatory incident/failure recording, continuous
opt-in, final flush, write failure and recovery. Static release isolation and
production crash-memory guard checks passed. Writer queue/history are bounded
at512 lines; event enqueue uses a try-lock and file I/O stays on the writer.
Burst testing is not a gameplay latency benchmark or a forced full rotation test.
Crash guard was source/build checked; no intentional live application crash was
performed and no universal safety claim is made for diagnostic memory dumps.

Ordinary Release build and its four candidate self-tests passed; existing ViGEm
missing-PDB warning only. No user logs/settings changed or GitHub publication.

Artifact: build/bin/Release/x64/HallJoy.exe
SHA256:3f2daf4cf469fcd29c2633c58a8e7936cbcbda2a5ffe8978e3664aae2bd0dc16

Evidence: .local/log-review-20260920-baseline.txt,
.local/log-review-20260920-expanded.txt, .local/log-review-20260920-release.txt.
Source backup: .local/backups/public-crash-privacy-20260920.zip.
