[CmdletBinding()]
param()

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

$root = Split-Path -Parent $PSScriptRoot
$pluginRoot = Join-Path $root 'third_party\UniversalAnalogPluginFixed'
$hallJoyRoot = Join-Path $root 'src\HallJoyProject'
$pluginBuild = Join-Path $pluginRoot 'tools\build_fixed_plugin.ps1'
$soupPatch = Join-Path $pluginRoot 'tools\Apply-Soup-Madlions-Fix.ps1'
$project = Join-Path $hallJoyRoot 'HallJoy\HallJoy.vcxproj'
$runtime = Join-Path $hallJoyRoot 'runtime'
$outDir = Join-Path $hallJoyRoot 'x64\MAD68ProRNative'
$targetName = 'HallJoy'
$exe = Join-Path $outDir ($targetName + '.exe')
$pdb = Join-Path $outDir ($targetName + '.pdb')
$map = Join-Path $outDir ($targetName + '.map')
$sendDir = Join-Path $root 'build\output'
$dependencyLockPath = Join-Path $root 'tools\dependency-lock.json'
if (-not (Test-Path -LiteralPath $dependencyLockPath -PathType Leaf)) {
    throw "Dependency lock is missing: $dependencyLockPath"
}
$dependencyLock = Get-Content -LiteralPath $dependencyLockPath -Raw | ConvertFrom-Json
$vigemSpec = $dependencyLock.binaryInputs.vigemClient
$vigemLib = Join-Path $root ([string]$vigemSpec.path).Replace('/', '\')
$vigemExpectedSize = [long]$vigemSpec.size
$vigemExpectedSha256 = [string]$vigemSpec.sha256

$required = @(
    $dependencyLockPath,
    $pluginBuild,
    $soupPatch,
    (Join-Path $pluginRoot 'abiv0.sun'),
    (Join-Path $pluginRoot 'abiv1-pluswooting.sun'),
    (Join-Path $pluginRoot 'abiv0-mad68native.sun'),
    (Join-Path $pluginRoot 'abiv1-pluswooting-mad68native.sun'),
    (Join-Path $pluginRoot 'main.cpp'),
    (Join-Path $pluginRoot 'halljoy_uap_cabi_guard.h'),
    (Join-Path $pluginRoot 'halljoy_uap_device_identity.h'),
    (Join-Path $pluginRoot 'halljoy_uap_pinned_owners.h'),
    (Join-Path $pluginRoot 'halljoy_uap_poll_pacing.h'),
    (Join-Path $pluginRoot 'halljoy_plugin_telemetry.h'),
    (Join-Path $pluginRoot 'halljoy_dense_snapshot.h'),
    (Join-Path $pluginRoot 'halljoy_native_hid_claim.h'),
    $project,
    (Join-Path $hallJoyRoot 'HallJoy\mad68pr_backend.cpp'),
    (Join-Path $hallJoyRoot 'HallJoy\mad68pr_protocol.cpp'),
    (Join-Path $hallJoyRoot 'HallJoy\hex80_backend.cpp'),
    (Join-Path $hallJoyRoot 'HallJoy\hex80_protocol.cpp'),
    (Join-Path $hallJoyRoot 'HallJoy\aula_win60he_backend.cpp'),
    (Join-Path $hallJoyRoot 'HallJoy\aula_win60he_backend.h'),
    (Join-Path $hallJoyRoot 'HallJoy\aula_win60he_client.cpp'),
    (Join-Path $hallJoyRoot 'HallJoy\aula_win60he_client.h'),
    (Join-Path $hallJoyRoot 'HallJoy\aula_win60he_protocol.cpp'),
    (Join-Path $hallJoyRoot 'HallJoy\aula_win60he_protocol.h'),
    (Join-Path $hallJoyRoot 'HallJoy\aula_win60he_session_policy.cpp'),
    (Join-Path $hallJoyRoot 'HallJoy\aula_win60he_session_policy.h'),
    (Join-Path $hallJoyRoot 'HallJoy\native_analog_routing.cpp'),
    (Join-Path $hallJoyRoot 'HallJoy\native_hid_interface_claim_registry.h'),
    (Join-Path $hallJoyRoot 'HallJoy\native_analog_backend.h'),
    (Join-Path $hallJoyRoot 'HallJoy\native_analog_backend_registry.h'),
    (Join-Path $hallJoyRoot 'HallJoy\native_analog_backend_registry.cpp'),
    (Join-Path $hallJoyRoot 'HallJoy\native_analog_backends.def'),
    (Join-Path $hallJoyRoot 'HallJoy\input_wake_sequence.h'),
    (Join-Path $hallJoyRoot 'HallJoy\publication_generation.h'),
    (Join-Path $hallJoyRoot 'HallJoy\latest_value_mailbox.h'),
    (Join-Path $hallJoyRoot 'tests\native_backend_architecture_static_audit.py'),
    (Join-Path $hallJoyRoot 'tests\startup_wake_transaction_static_audit.py'),
    (Join-Path $hallJoyRoot 'tests\vigem_output_isolation_static_audit.py'),
    (Join-Path $hallJoyRoot 'tests\persistence_transaction_static_audit.py'),
    (Join-Path $hallJoyRoot 'tests\storage_migration_static_audit.py'),
    (Join-Path $hallJoyRoot 'tests\mouse_ipc_static_audit.py'),
    (Join-Path $hallJoyRoot 'tests\analog_host_ipc_static_audit.py'),
    (Join-Path $hallJoyRoot 'tests\overlay_http_framing_static_audit.py'),
    (Join-Path $hallJoyRoot 'tests\overlay_concurrency_origin_static_audit.py'),
    (Join-Path $hallJoyRoot 'tests\transactional_file_store_test.cpp'),
    (Join-Path $hallJoyRoot 'tests\dependency_lock_static_audit.py'),
    (Join-Path $hallJoyRoot 'tests\dependency_installer_removal_static_audit.py'),
    (Join-Path $hallJoyRoot 'tests\dependency_guidance_policy_test.cpp'),
    (Join-Path $hallJoyRoot 'HallJoy\dependency_guidance_policy.h'),
    (Join-Path $hallJoyRoot 'tests\native_analog_backend_contract_test.cpp'),
    (Join-Path $hallJoyRoot 'tests\native_hid_interface_claim_test.cpp'),
    (Join-Path $hallJoyRoot 'tests\native_hid_interface_claim_static_audit.py'),
    (Join-Path $hallJoyRoot 'tests\aula_win60he_backend_static_audit.py'),
    (Join-Path $hallJoyRoot 'tests\aula_win60he_protocol_test.cpp'),
    (Join-Path $hallJoyRoot 'tests\aula_win60he_oracle_test.cpp'),
    (Join-Path $hallJoyRoot 'tests\aula_win60he_oracle_fixtures.h'),
    (Join-Path $hallJoyRoot 'tests\aula_win60he_end_to_end_test.cpp'),
    (Join-Path $hallJoyRoot 'tests\aula_win60he_session_policy_test.cpp'),
    (Join-Path $hallJoyRoot 'tests\private_uap_runtime_static_audit.py'),
    (Join-Path $hallJoyRoot 'tests\uap_device_identity_static_audit.py'),
    (Join-Path $hallJoyRoot 'tests\uap_device_identity_test.cpp'),
    (Join-Path $hallJoyRoot 'tests\uap_poll_pacing_static_audit.py'),
    (Join-Path $hallJoyRoot 'tests\uap_poll_pacing_test.cpp'),
    (Join-Path $hallJoyRoot 'tests\uap_snapshot_pinning_static_audit.py'),
    (Join-Path $hallJoyRoot 'tests\uap_snapshot_pinning_test.cpp'),
    (Join-Path $hallJoyRoot 'tests\windows_command_line_test.cpp'),
    (Join-Path $hallJoyRoot 'HallJoy\windows_command_line.h'),
    (Join-Path $hallJoyRoot 'HallJoy\transactional_file_store.h'),
    (Join-Path $hallJoyRoot 'HallJoy\file_name_policy.h'),
    (Join-Path $hallJoyRoot 'HallJoy\file_name_policy.cpp'),
    (Join-Path $root 'tools\new_native_backend.py'),
    (Join-Path $root 'tools\run_native_backend_checks.py'),
    (Join-Path $root 'tools\run_aula_win60he_sanitizers.py'),
    (Join-Path $root 'tools\check_overlay_responsiveness.py'),
    (Join-Path $root 'tools\check_overlay_http_framing.py'),
    (Join-Path $root 'tools\check_overlay_concurrency_origin.py'),
    (Join-Path $root 'tools\run_production_smoke.ps1'),
    (Join-Path $root 'tools\run_release_qualification.ps1'),
    (Join-Path $root 'tools\run_storage_migration_test.ps1'),
    (Join-Path $root 'tools\check_private_uap_abi.py'),
    (Join-Path $root 'tools\analyze_stability_trace.py'),
    (Join-Path $root 'tools\collect_stability_trace.ps1'),
    (Join-Path $root 'tools\COLLECT_STABILITY_TRACE.cmd'),
    (Join-Path $root 'docs\development\ADDING_NATIVE_ANALOG_PROTOCOL.md'),
    (Join-Path $root 'docs\development\NATIVE_BACKEND_CONTRACT.md'),
    (Join-Path $root 'docs\development\PROTOCOL_REVIEW_CHECKLIST.md'),
    (Join-Path $root 'docs\development\TESTING_NEW_PROTOCOL.md'),
    (Join-Path $root 'docs\development\AULA_WIN60HE_MAX_WORKSHEET.md'),
    (Join-Path $root 'docs\protocols\AULA_WIN60HE_MAX_PROTOCOL.md'),
    (Join-Path $root 'docs\stability\tests\V14-12A_AULA_WIN60HE_FIRMWARE_PROVEN_2026-08-01.txt'),
    (Join-Path $hallJoyRoot 'HallJoy\vigem_output_scheduler.h'),
    (Join-Path $hallJoyRoot 'HallJoy\app.cpp'),
    (Join-Path $hallJoyRoot 'HallJoy\addressed_analog_backend.cpp'),
    (Join-Path $hallJoyRoot 'HallJoy\debug_log.cpp'),
    $vigemLib
)
$missing = @($required | Where-Object { -not (Test-Path -LiteralPath $_) })
if ($missing.Count -ne 0) {
    $missing | ForEach-Object { Write-Host "Missing: $_" -ForegroundColor Red }
    throw 'Source package is incomplete.'
}

$projectText = Get-Content -LiteralPath $project -Raw
$analogClientHeader = Get-Content -LiteralPath (Join-Path $hallJoyRoot 'HallJoy\analog_host_client.h') -Raw
$analogSharedHeader = Get-Content -LiteralPath (Join-Path $hallJoyRoot 'HallJoy\analog_host_shared.h') -Raw
$uapIncludeMarker = '$(ProjectDir)..\..\..\third_party\UniversalAnalogPluginFixed'
if ($projectText -notmatch [regex]::Escape($uapIncludeMarker) -or
    $analogClientHeader -notmatch '#include "halljoy_plugin_telemetry.h"' -or
    $analogSharedHeader -notmatch '#include "halljoy_plugin_telemetry.h"' -or
    $analogSharedHeader -notmatch '#include "halljoy_dense_snapshot.h"' -or
    $analogClientHeader -match 'UniversalAnalogPluginFixed/' -or
    $analogSharedHeader -match 'UniversalAnalogPluginFixed/') {
    throw 'Clean-layout include preflight failed: HallJoy is not wired to third_party\UniversalAnalogPluginFixed.'
}

$standardSunText = (Get-Content -LiteralPath (Join-Path $pluginRoot 'abiv0.sun') -Raw) +
    (Get-Content -LiteralPath (Join-Path $pluginRoot 'abiv1-pluswooting.sun') -Raw)
$nativeSunText = (Get-Content -LiteralPath (Join-Path $pluginRoot 'abiv0-mad68native.sun') -Raw) +
    (Get-Content -LiteralPath (Join-Path $pluginRoot 'abiv1-pluswooting-mad68native.sun') -Raw)
if ($standardSunText -match 'UAP_EXCLUDE_HALLJOY_NATIVE') {
    throw 'Regression guard failed: standard UAP targets must not exclude MAD68 Pro R.'
}
if ($nativeSunText -notmatch 'UAP_EXCLUDE_HALLJOY_NATIVE=1') {
    throw 'Native UAP targets do not contain the HallJoy native-protocol routing gate.'
}

function Normalize-UapSun([string]$Text) {
    $lines = $Text -split "`r?`n" | Where-Object {
        $_ -notmatch '^\s*name\s+' -and
        $_ -notmatch 'UAP_EXCLUDE_HALLJOY_NATIVE'
    }
    return (($lines | ForEach-Object { $_.TrimEnd() }) -join "`n").Trim()
}
$standardAbi0 = Get-Content -LiteralPath (Join-Path $pluginRoot 'abiv0.sun') -Raw
$nativeAbi0 = Get-Content -LiteralPath (Join-Path $pluginRoot 'abiv0-mad68native.sun') -Raw
$standardAbi1 = Get-Content -LiteralPath (Join-Path $pluginRoot 'abiv1-pluswooting.sun') -Raw
$nativeAbi1 = Get-Content -LiteralPath (Join-Path $pluginRoot 'abiv1-pluswooting-mad68native.sun') -Raw
if ((Normalize-UapSun $standardAbi0) -ne (Normalize-UapSun $nativeAbi0) -or
    (Normalize-UapSun $standardAbi1) -ne (Normalize-UapSun $nativeAbi1)) {
    throw 'Regression guard failed: native UAP flags differ from baseline by more than the native routing gate/name.'
}
if ($nativeSunText -notmatch 'UAP_DISABLE_HOTPLUG=1' -or $standardSunText -notmatch 'UAP_DISABLE_HOTPLUG=1') {
    throw 'Regression guard failed: baseline HallJoy UAP hotplug policy was changed.'
}

$soupPatchText = Get-Content -LiteralPath $soupPatch -Raw
if ($soupPatchText -notmatch 'HallJoy native analogue pre-open exclusion' -or
    $soupPatchText -notmatch 'UAP_EXCLUDE_HALLJOY_NATIVE' -or
    $soupPatchText -notmatch 'halljoy_should_exclude_hid_interface' -or
    $soupPatchText -match 'HALLJOY_UAP_NATIVE_HID_IDS' -or
    $soupPatchText -notmatch '\$hidSourceText\.Insert\(\$braceStart \+ 1, \$preOpenBlock\)') {
    throw 'Regression guard failed: the dedicated UAP does not apply dynamic native routing before Soup CreateFileW.'
}

$madBackendText = Get-Content -LiteralPath (Join-Path $hallJoyRoot 'HallJoy\mad68pr_backend.cpp') -Raw
$madHeaderText = Get-Content -LiteralPath (Join-Path $hallJoyRoot 'HallJoy\mad68pr_backend.h') -Raw
$madProtocolText = Get-Content -LiteralPath (Join-Path $hallJoyRoot 'HallJoy\mad68pr_protocol.h') -Raw
$hexBackendText = Get-Content -LiteralPath (Join-Path $hallJoyRoot 'HallJoy\hex80_backend.cpp') -Raw
$hexProtocolText = Get-Content -LiteralPath (Join-Path $hallJoyRoot 'HallJoy\hex80_protocol.h') -Raw
$nativeRoutingText = Get-Content -LiteralPath (Join-Path $hallJoyRoot 'HallJoy\native_analog_routing.cpp') -Raw
$nativeContractText = Get-Content -LiteralPath (Join-Path $hallJoyRoot 'HallJoy\native_analog_backend.h') -Raw
$nativeRegistryText = Get-Content -LiteralPath (Join-Path $hallJoyRoot 'HallJoy\native_analog_backend_registry.cpp') -Raw
$nativeCatalogText = Get-Content -LiteralPath (Join-Path $hallJoyRoot 'HallJoy\native_analog_backends.def') -Raw
$backendText = Get-Content -LiteralPath (Join-Path $hallJoyRoot 'HallJoy\backend.cpp') -Raw
$curveText = Get-Content -LiteralPath (Join-Path $hallJoyRoot 'HallJoy\backend_curve.cpp') -Raw
$realtimeText = Get-Content -LiteralPath (Join-Path $hallJoyRoot 'HallJoy\realtime_loop.cpp') -Raw
$outputSchedulerText = Get-Content -LiteralPath (Join-Path $hallJoyRoot 'HallJoy\vigem_output_scheduler.h') -Raw
$appText = Get-Content -LiteralPath (Join-Path $hallJoyRoot 'HallJoy\app.cpp') -Raw
$mainText = Get-Content -LiteralPath (Join-Path $hallJoyRoot 'HallJoy\main.cpp') -Raw
$addressedText = Get-Content -LiteralPath (Join-Path $hallJoyRoot 'HallJoy\addressed_analog_backend.cpp') -Raw
$debugLogText = Get-Content -LiteralPath (Join-Path $hallJoyRoot 'HallJoy\debug_log.cpp') -Raw
$sparkText = Get-Content -LiteralPath (Join-Path $hallJoyRoot 'HallJoy\backend_sparklink.inc') -Raw
$sayoText = Get-Content -LiteralPath (Join-Path $hallJoyRoot 'HallJoy\backend_sayo.inc') -Raw
$uapMainText = Get-Content -LiteralPath (Join-Path $pluginRoot 'main.cpp') -Raw
if ($madBackendText -notmatch 'Mad68ProR_PrepareProtocolRouting' -or
    $madBackendText -notmatch 'ProbeNativeControlProtocol' -or
    $madBackendText -notmatch 'LooksLikeMad68Family' -or
    $madBackendText -notmatch 'layoutCompatible' -or
    $madBackendText -notmatch 'NativeAnalogRouting_Claim' -or
    $madBackendText -notmatch 'Mad68ProR_GetNativeBackendDescriptor' -or
    $appText -notmatch 'NativeAnalogBackends_Reset' -or
    $appText -notmatch 'NativeAnalogBackends_CatalogIsValid' -or
    $appText -notmatch 'NativeAnalogBackends_PrepareRouting' -or
    $nativeContractText -notmatch 'NativeAnalogBackendDescriptor' -or
    ([regex]::Matches($nativeRegistryText, 'native_analog_backends\.def')).Count -lt 2 -or
    $nativeRegistryText -notmatch 'NativeAnalogBackends_CatalogIsValid' -or
    $nativeRegistryText -notmatch 'std::strcmp' -or
    $nativeRegistryText -notmatch 'NativeAnalogRouting_Reset' -or
    $nativeCatalogText -notmatch 'Mad68ProR_GetNativeBackendDescriptor' -or
    $nativeCatalogText -notmatch 'Hex80_GetNativeBackendDescriptor' -or
    $nativeCatalogText -notmatch 'AddressedAnalog_GetNativeBackendDescriptor' -or
    $nativeRoutingText -notmatch 'HALLJOY_UAP_NATIVE_HID_PATHS' -or
    $nativeRoutingText -notmatch 'InterfaceClaimRegistry' -or
    $uapMainText -notmatch 'halljoy_should_exclude_hid_interface' -or
    $uapMainText -notmatch 'halljoy_uap_native_hid_excluded') {
    throw 'Native protocol-routing preflight failed: descriptor catalog or native/UAP arbitration is incomplete.'
}
if ($mainText -notmatch 'universal native continuation enabled; UAP unavailable for this run' -or
    $mainText -match 'if \(Mad68ProR_IsProtocolDevicePresent\(\)\)') {
    throw 'Universal startup preflight failed: UAP preparation fallback is still tied to one MAD68 device.'
}
if ($hexProtocolText -notmatch 'kTravelBuffer = 0x1C' -or
    $hexProtocolText -notmatch 'kTravelInfo = 0x24' -or
    $hexProtocolText -notmatch 'kCalibrationFinish = 0x19' -or
    $hexBackendText -notmatch 'ProbeCandidate' -or
    $hexBackendText -notmatch 'BuildTravelInfoPayload' -or
    $hexBackendText -notmatch 'BuildTravelBufferPayload\(0, 4\)' -or
    $hexBackendText -notmatch 'NativeAnalogRouting_Claim' -or
    $hexBackendText -notmatch 'NativeAnalogRouting_IsClaimed' -or
    $hexBackendText -match '!hex80::IsKnownProductId\(candidate.attributes.ProductID\)' -or
    $hexBackendText -notmatch 'RealtimeLoop_NotifyInputChangedAt' -or
    $hexBackendText -notmatch 'SwitchToThread' -or
    $hexBackendText -notmatch 'Hex80_GetNativeBackendDescriptor' -or
    $nativeCatalogText -notmatch 'Hex80_GetNativeBackendDescriptor') {
    throw 'Hex80 native protocol preflight failed: discovery, polling, event wake or UAP arbitration is incomplete.'
}
if ($sparkText -notmatch 'SparkQueryDeviceInfo\(routeProbe\)' -or
    $sparkText -notmatch 'NativeAnalogProtocol::SparkLink' -or
    $sparkText -notmatch 'NativeAnalogRouting_IsClaimedBy' -or
    $backendText -notmatch 'BackendNative_GetSparkDescriptor' -or
    $nativeCatalogText -notmatch 'BackendNative_GetSparkDescriptor' -or
    $backendText.IndexOf('NativeAnalogBackends_StartPhase(NativeAnalogStartPhase::BeforeUap)') -gt $backendText.IndexOf('wooting_analog_initialise()')) {
    throw 'SparkLink protocol-routing preflight failed: capability proof or pre-UAP ownership claim is incomplete.'
}
if ($sayoText -notmatch 'attr.VendorID != kSayoVendorId' -or
    $sayoText -match 'attr.ProductID != kSayo' -or
    $sayoText -notmatch 'SayoProbeDepthProtocol' -or
    $sayoText -notmatch 'selectedPid' -or
    $sayoText -notmatch 'NativeAnalogProtocol::SayoDepth' -or
    $sayoText -notmatch 'NativeAnalogRouting_IsClaimedBy' -or
    $backendText -notmatch 'BackendNative_GetSayoDescriptor' -or
    $nativeCatalogText -notmatch 'BackendNative_GetSayoDescriptor' -or
    $backendText.IndexOf('NativeAnalogBackends_StartPhase(NativeAnalogStartPhase::BeforeUap)') -gt $backendText.IndexOf('wooting_analog_initialise()')) {
    throw 'Sayo protocol-routing preflight failed: same-brand capability validation or pre-UAP ownership claim is incomplete.'
}
$analogHostText = Get-Content -LiteralPath (Join-Path $hallJoyRoot 'HallJoy\analog_host_client.cpp') -Raw
$keyboardSubpagesText = Get-Content -LiteralPath (Join-Path $hallJoyRoot 'HallJoy\keyboard_subpages.cpp') -Raw
if ($projectText -notmatch 'HALLJOY_MAD68PR_NATIVE;HALLJOY_PRODUCTION' -or
    $projectText -match 'HALLJOY_MAD68PR_NATIVE;HALLJOY_DIAGNOSTIC') {
    throw 'Final-build preflight failed: native target must be production and must not enable file diagnostics.'
}
if (([regex]::Matches($projectText, 'Synchronization\.lib')).Count -lt 4) {
    throw 'Linker preflight failed: all HallJoy configurations must link Synchronization.lib for WaitOnAddress/WakeByAddress.'
}
if ($realtimeText -notmatch 'WaitOnAddress' -or
    $realtimeText -notmatch 'WakeByAddressSingle' -or
    $realtimeText -notmatch 'WakeByAddressAll' -or
    $realtimeText -notmatch 'CREATE_WAITABLE_TIMER_HIGH_RESOLUTION' -or
    $realtimeText -notmatch 'Backend_GetNextOutputDeadlineQpc' -or
    $realtimeText -notmatch 'precise_output_deadline' -or
    $realtimeText -match 'g_inputEvent' -or
    $realtimeText -match 'SetEvent\(eventHandle\)' -or
    $realtimeText -notmatch 'Final production builds never aggregate') {
    throw 'Realtime wake preflight failed: final build must use address wakes plus a precise internal output-deadline timer.'
}
if ($madBackendText -notmatch '(?s)#if !defined\(HALLJOY_DIAGNOSTIC\).*?Final builds expose live state' -or
    $analogHostText -notmatch '(?s)#if !defined\(HALLJOY_DIAGNOSTIC\).*?Production telemetry is kept in shared memory' -or
    $debugLogText -notmatch 'Production watchdog is intentionally silent' -or
    $debugLogText -notmatch 'HALLJOY_DIAGNOSTIC\) \|\| defined\(HALLJOY_MAD68PR_NATIVE') {
    throw 'Production logging/watchdog preflight failed: hot-path files are not fully disabled or quiet A9 recovery is missing.'
}
if ($madBackendText -notmatch 'PublishAnalogueChange' -or
    $madBackendText -notmatch 'RealtimeLoop_NotifyInputChangedAt' -or
    $madBackendText -notmatch 'Mad68ProR_ConsumeChangeBatch' -or
    $madHeaderText -notmatch 'Mad68ProRChangeBatch' -or
    $backendText -notmatch 'PersistentFilteredValue' -or
    $backendText -notmatch 'BackendCurve_GetGeneration' -or
    $backendText -notmatch 'VigemOutputScheduler::Decision::DeferUntilDeadline' -or
    $backendText -notmatch 'All analogue backends share the same scheduler' -or
    $backendText -notmatch 'Backend_GetNextOutputDeadlineQpc' -or
    $backendText -notmatch 'Periodic duplicate keepalives add no' -or
    $backendText -match 'bypassing the generic 1 ms output limiter' -or
    $outputSchedulerText -notmatch 'The first changed report after an idle' -or
    $outputSchedulerText -notmatch 'newest report and become due at a fixed deadline' -or
    $curveText -notmatch 'BackendCurve_GetGeneration') {
    throw 'Unified event-driven pipeline preflight failed: wake, curve cache or exact ViGEm coalescing markers are missing.'
}
if ($backendText -notmatch 'void Backend_GetAnalogTelemetry' -or
    $backendText -notmatch 'Mad68ProR_IsDevicePresent' -or
    $backendText -notmatch 'AnalogHostClient_GetTelemetry' -or
    $backendText -notmatch 'g_sparkConnected' -or
    $backendText -notmatch 'g_sayoConnected' -or
    $backendText -notmatch 'Hex80_GetTelemetry' -or
    $keyboardSubpagesText -notmatch 'Config_BuildAnalogTelemetryLines' -or
    $keyboardSubpagesText -notmatch 'MADLIONS native A0: VID 373B / PID' -or
    $keyboardSubpagesText -notmatch 'ATK x QK Hex80:' -or
    $keyboardSubpagesText -notmatch 'Addressed Analog 09/94/02:' -or
    $keyboardSubpagesText -notmatch 'SparkLink:' -or
    $keyboardSubpagesText -notmatch 'Analog host:' -or
    $keyboardSubpagesText -notmatch 'SayoDevice:' -or
    $keyboardSubpagesText -notmatch 'Analog input: MADLIONS native A0' -or
    $keyboardSubpagesText -notmatch 'Analog input: ATK x QK Hex80' -or
    $keyboardSubpagesText -notmatch 'Analog input: Addressed 09/94/02' -or
    $keyboardSubpagesText -notmatch 'Analog input: SparkLink' -or
    $keyboardSubpagesText -notmatch 'Analog input: SayoDevice' -or
    $keyboardSubpagesText -notmatch 'Analog input: Wooting Analog SDK' -or
    $backendText -notmatch 'NativeAnalogBackends_GetTelemetry' -or
    $keyboardSubpagesText -notmatch 'Native protocol %S:' -or
    $keyboardSubpagesText -notmatch 'nativeProtocolCount') {
    throw 'UI telemetry preflight failed: Configuration/Gamepad Tester routes do not cover MAD68, Hex80, Addressed, Spark, Sayo and UAP/Wooting.'
}
if ($appText -notmatch 'RegisterRawInputDevices\(rid, 2' -or
    $keyboardSubpagesText -notmatch 'Backend_GetAnalogTelemetry' -or
    (Get-Content -LiteralPath (Join-Path $hallJoyRoot 'HallJoy\keyboard_render.cpp') -Raw) -notmatch 'DrawDigitalIndicatorAA') {
    throw 'UI regression guard failed: Raw Input keyboard preview indicators or telemetry rendering were removed.'
}
if ($madBackendText -notmatch 'HALLJOY_BUILD_ID_W' -or
    $madBackendText -notmatch 'kA8SemanticEvidenceMinFresh' -or
    $madBackendText -notmatch 'waiting for a true post-edge A0' -or
    $madBackendText -notmatch 'STEADY-STATE A0 CONFIRMED' -or
    $madBackendText -notmatch 'CaptureAnalogSnapshot' -or
    $madBackendText -notmatch 'if \(sampleSeq > sampleSeqAtEdge\) return true' -or
    $madBackendText -notmatch 'per-key scheduler starvation' -or
    $madBackendText -notmatch 'A0 transport gap' -or
    $madBackendText -notmatch 'Mad68ProR_EmergencyRestoreInputOnce' -or
    $madBackendText -notmatch 'MaybeConfirmSteadyStateFromAnalogOnlyEdge' -or
    $madBackendText -notmatch 'post-grace A0 analogue edge proof' -or
    $madProtocolText -notmatch 'kSteadyProofMinDelta = 64' -or
    $madProtocolText -notmatch 'IsPostSweepAnalogProof') {
    throw 'MAD68 native v3.6 safety/freshness markers are missing.'
}
$rawInputRegistrationIndex = $appText.IndexOf('RegisterRawInputDevices(rid, 2')
$rawInputPhaseIndex = $appText.IndexOf('NativeAnalogBackends_StartPhase(NativeAnalogStartPhase::AfterRawInput)')
$startupCommitIndex = $appText.IndexOf('g_backendReady = AppStartBackendDependents(rawInputRegistered')
if ($rawInputRegistrationIndex -lt 0 -or $rawInputPhaseIndex -lt 0 -or
    $startupCommitIndex -lt $rawInputRegistrationIndex) {
    throw 'Regression guard failed: AfterRawInput native phase can start before target-scoped Raw Input registration.'
}
$routingResetIndex = $appText.IndexOf('NativeAnalogBackends_Reset()')
$routingPrepareIndex = $appText.IndexOf('NativeAnalogBackends_PrepareRouting()')
$backendInitIndex = $appText.IndexOf('Backend_Init()')
$afterRealtimeIndex = $appText.IndexOf('NativeAnalogBackends_StartPhase(NativeAnalogStartPhase::AfterRealtime)')
if ($routingResetIndex -lt 0 -or $routingPrepareIndex -lt $routingResetIndex -or
    $backendInitIndex -lt $routingPrepareIndex -or $afterRealtimeIndex -lt 0 -or
    $startupCommitIndex -lt $backendInitIndex -or
    $addressedText -notmatch 'NativeAnalogProtocol::Addressed09402' -or
    $addressedText -notmatch 'AddressedAnalog_GetNativeBackendDescriptor' -or
    $addressedText -notmatch 'RealtimeLoop_NotifyInputChanged' -or
    $addressedText -notmatch 'kMaxKeysPerPacket' -or
    $addressedText -notmatch '#if defined\(HALLJOY_DIAGNOSTIC\)') {
    throw 'Universal routing preflight failed: catalog lifecycle or Addressed Analog safety is incomplete.'
}
if ($madProtocolText -notmatch 'kArmOpcode = 0xA8' -or
    $madProtocolText -notmatch 'kRestoreInputOpcode = 0xA9') {
    throw 'MAD68 protocol opcode allow-list constants are missing.'
}

$nativeCheckRunner = Join-Path $root 'tools\run_native_backend_checks.py'
Write-Host 'Running native backend static checks...' -ForegroundColor Cyan
& python $nativeCheckRunner --require-compiler
if ($LASTEXITCODE -ne 0) { throw "Native backend static checks failed: $LASTEXITCODE" }

$generator = Join-Path $root 'tools\new_native_backend.py'
& python $generator --help | Out-Null
if ($LASTEXITCODE -ne 0) { throw "Native backend generator self-check failed: $LASTEXITCODE" }

$vigemInfo = Get-Item -LiteralPath $vigemLib
$vigemHash = (Get-FileHash -LiteralPath $vigemLib -Algorithm SHA256).Hash
if ($vigemInfo.Length -ne $vigemExpectedSize -or $vigemHash -ne $vigemExpectedSha256) {
    throw "ViGEmClient.lib preflight failed. Size=$($vigemInfo.Length), SHA-256=$vigemHash"
}

Write-Host 'Building the embedded UAP with capability-validated native routing...' -ForegroundColor Cyan
& powershell -NoProfile -ExecutionPolicy Bypass -File $pluginBuild `
    -ExcludeMad68ProRNative -DependencyLock $dependencyLockPath
if ($LASTEXITCODE -ne 0) { throw "Plugin build failed: $LASTEXITCODE" }

$patchedHid = Join-Path $pluginRoot 'Soup\soup\hwHid.cpp'
if (-not (Test-Path -LiteralPath $patchedHid -PathType Leaf)) {
    throw "Patched Soup hwHid.cpp was not found: $patchedHid"
}
$patchedHidText = Get-Content -LiteralPath $patchedHid -Raw
$preOpenIndex = $patchedHidText.IndexOf('HallJoy native analogue pre-open exclusion')
$createFileIndex = $patchedHidText.IndexOf('hid.handle = CreateFileW')
if ($preOpenIndex -lt 0 -or $createFileIndex -lt 0 -or $preOpenIndex -gt $createFileIndex) {
    throw 'Regression guard failed: dynamic native routing is not located before Soup CreateFileW.'
}
Write-Host 'Verified: the isolated UAP skips only exact HID interface paths validated by a HallJoy native protocol before opening HID.' -ForegroundColor DarkGray

$pluginOut = Join-Path $pluginRoot 'dist\universal-analog-plugin'
$abi0 = Join-Path $pluginOut 'abiv0.dll'
$abi1 = Join-Path $pluginOut 'abiv1.dll'
if (-not (Test-Path -LiteralPath $abi0) -or -not (Test-Path -LiteralPath $abi1)) {
    throw 'Universal Analog Plugin build did not produce abiv0.dll and abiv1.dll.'
}
$uapAbiCheck = Join-Path $root 'tools\check_private_uap_abi.py'
& python $uapAbiCheck $abi1
if ($LASTEXITCODE -ne 0) { throw "Private UAP ABI runtime gate failed: $LASTEXITCODE" }
New-Item -ItemType Directory -Path $runtime -Force | Out-Null
Copy-Item -LiteralPath $abi0 -Destination (Join-Path $runtime 'universal_analog_abiv0.dll') -Force
Copy-Item -LiteralPath $abi1 -Destination (Join-Path $runtime 'universal_analog_abiv1.dll') -Force

$vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
$msbuild = $null
if (Test-Path -LiteralPath $vswhere) {
    $msbuild = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -find 'MSBuild\**\Bin\amd64\MSBuild.exe' | Select-Object -First 1
}
if (-not $msbuild) {
    $candidates = @(
        'C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\amd64\MSBuild.exe',
        'C:\Program Files\Microsoft Visual Studio\2022\Professional\MSBuild\Current\Bin\amd64\MSBuild.exe',
        'C:\Program Files\Microsoft Visual Studio\2022\Enterprise\MSBuild\Current\Bin\amd64\MSBuild.exe',
        'C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\MSBuild\Current\Bin\amd64\MSBuild.exe'
    )
    $msbuild = $candidates | Where-Object { Test-Path -LiteralPath $_ } | Select-Object -First 1
}
if (-not $msbuild) {
    throw 'MSBuild x64 was not found. Install Visual Studio 2022 with Desktop development with C++.'
}

if (Test-Path -LiteralPath $outDir) { Remove-Item -LiteralPath $outDir -Recurse -Force }
Write-Host "Building $targetName.exe with MAD68 + Hex80 + Addressed + Aula + Spark + Sayo + UAP..." -ForegroundColor Cyan
$buildOutput = @(& $msbuild $project `
    '/t:Rebuild' `
    '/p:Configuration=Release' `
    '/p:Platform=x64' `
    '/p:HallJoyMad68ProRNative=true' `
    '/p:HallJoyStabilityTrace=true' `
    '/m' 2>&1)
$buildExitCode = $LASTEXITCODE
$buildOutput | Out-Host
if ($buildExitCode -ne 0) { throw "HallJoy build failed: $buildExitCode" }
$productionWarnings = @($buildOutput | Where-Object { [string]$_ -match ': warning (?:C|LNK)\d+:' })
$allowedProductionWarning = 'warning LNK4099:.*ViGEmClient\.pdb'
$unexpectedWarnings = @($productionWarnings | Where-Object { [string]$_ -notmatch $allowedProductionWarning })
if ($unexpectedWarnings.Count -ne 0) {
    $unexpectedWarnings | ForEach-Object { Write-Host $_ -ForegroundColor Red }
    throw 'Unexpected production compiler/linker warnings were emitted.'
}
$productionWarningCodes = @($productionWarnings | ForEach-Object {
    if ([string]$_ -match 'warning ((?:C|LNK)\d+):') { $Matches[1] }
} | Sort-Object -Unique)
$warningSummary = if ($productionWarningCodes.Count) { $productionWarningCodes -join ', ' } else { 'none' }
Write-Host "Production warning baseline: allowed codes=$warningSummary; 0 unexpected." -ForegroundColor DarkGray
if (-not (Test-Path -LiteralPath $exe)) { throw "Executable not produced: $exe" }

$preservedRuntimeNames = @(
    'settings.ini',
    'bindings.ini',
    'GlobalProfiles',
    'Layouts',
    'CurvePresets',
    'HallJoy.portable'
)
if (Test-Path -LiteralPath $sendDir) {
    $resolvedSendDir = [IO.Path]::GetFullPath($sendDir).TrimEnd('\')
    $expectedSendDir = [IO.Path]::GetFullPath((Join-Path $root 'build\output')).TrimEnd('\')
    if (-not $resolvedSendDir.Equals($expectedSendDir, [StringComparison]::OrdinalIgnoreCase)) {
        throw "Unsafe package cleanup target: $resolvedSendDir"
    }
    Get-ChildItem -LiteralPath $resolvedSendDir -Force | Where-Object {
        $preservedRuntimeNames -notcontains $_.Name
    } | ForEach-Object {
        Remove-Item -LiteralPath $_.FullName -Recurse -Force
    }
} else {
    New-Item -ItemType Directory -Path $sendDir -Force | Out-Null
}
Copy-Item -LiteralPath $exe -Destination $sendDir -Force
Copy-Item -LiteralPath (Join-Path $root 'tools\analyze_stability_trace.py') -Destination $sendDir -Force
Copy-Item -LiteralPath (Join-Path $root 'tools\collect_stability_trace.ps1') -Destination $sendDir -Force
Copy-Item -LiteralPath (Join-Path $root 'tools\COLLECT_STABILITY_TRACE.cmd') -Destination $sendDir -Force
Copy-Item -LiteralPath $dependencyLockPath -Destination $sendDir -Force

$symbols = Join-Path $outDir 'LOCAL_SYMBOLS_DO_NOT_SEND'
New-Item -ItemType Directory -Path $symbols -Force | Out-Null
if (Test-Path -LiteralPath $pdb) { Copy-Item -LiteralPath $pdb -Destination $symbols -Force }
if (Test-Path -LiteralPath $map) { Copy-Item -LiteralPath $map -Destination $symbols -Force }

@'
HallJoy v1.4 integration build

Aula WIN60HE: firmware-proven support for exact 1CA2:1902 / FFA0:0001 /
App V1.1.6 (Feb 4 2026); physical Aula hardware is not yet validated.

Запускайте HallJoy.exe обычным способом.

В финальной сборке:
- изменяемые настройки и профили по умолчанию хранятся в `%LOCALAPPDATA%\HallJoy`;
- прежние настройки рядом с EXE при первом запуске копируются с резервной копией в
  `MigrationBackups`, а исходные файлы не удаляются;
- для явного переносного режима создайте рядом с HallJoy.exe обычный файл `HallJoy.portable`;
- для управления используются только реальные аналоговые значения;
- зелёные цифровые индикаторы превью сохранены только как функция интерфейса;
- живая информация в Configuration и Gamepad Tester работает для MAD68 Pro R,
  ATK x QK Hex80, Addressed 09/94/02, Aula WIN60HE, SparkLink, SayoDevice и UAP/Wooting-совместимых клавиатур;
- любой VID 373B Hex80-совместимый PID направляется в native 0x96 только после двух
  валидных GET-ответов и не конфликтует с UAP, Addressed или native A0;
- Addressed 09/94/02 возвращён: точный FF60:0061/64-byte fingerprint и валидный
  checksummed ответ резервируют совместимую QBZ/IPI-платформу независимо от VID/PID;
- другие PID Sayo принимаются только после валидного ответа штатного depth-протокола;
- MADLIONS MAD68-family направляется в native A0 только после строгого fingerprint/ACK,
  а остальные MADLIONS остаются в UAP, поэтому два протокола не конфликтуют;
- Aula WIN60HE поддерживается только для точного 1CA2:1902, FFA0:0001 и
  App V1.1.6 / Feb 4 2026 после полного read-only proof; физическая Aula пока не проверена;
- realtime-поток пробуждается через WaitOnAddress/WakeByAddress;
- временный ограниченный HallJoyStabilityTrace.log создаётся рядом с EXE для проверки текущего этапа;
- trace содержит только редкие lifecycle/state-события, не записывает нажатые клавиши,
  аналоговые значения, вводимый текст, имена пользователя или per-poll данные;
- непрерывные per-key, latency, host и diagnostic-файлы не создаются;
- тихое аварийное восстановление A9 после завершения HallJoy сохранено.

Проверка S02V1:
1. Запустите HallJoy и активно нажимайте аналоговые клавиши не менее 30 секунд.
2. По очереди выберите SparkLink Safe, Fast Yield и Max Burst.
3. Установите минимум два разных значения Spark row limit, включая Unlimited/0.
4. При удерживаемой аналоговой клавише отключите SparkLink, затем подключите обратно.
5. Убедитесь, что ViGEm снова реагирует, и штатно закройте HallJoy.
6. Запустите COLLECT_STABILITY_TRACE.cmd.
7. Передайте HallJoyStabilityTraceBundle.zip для машинной проверки.

Stock-прошивка MAD68, формат A0 и частота телеметрии не изменяются.
Не запускайте прошивальщики и конфигураторы клавиатуры, пока HallJoy владеет
vendor-интерфейсом MAD68.
'@ | Set-Content -LiteralPath (Join-Path $sendDir 'README_FOR_TESTER_RU.txt') -Encoding UTF8

Write-Host ''
Write-Host 'Build completed:' -ForegroundColor Green
Write-Host "  $(Join-Path $sendDir ($targetName + '.exe'))" -ForegroundColor Green
Write-Host 'Verification build: bounded event-only HallJoyStabilityTrace.log is enabled for this stabilization stage.' -ForegroundColor Yellow
Write-Host 'After the test, run COLLECT_STABILITY_TRACE.cmd from build\output.' -ForegroundColor Yellow
