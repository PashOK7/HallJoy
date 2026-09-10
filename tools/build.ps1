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
$runtime = Join-Path $hallJoyRoot '..\..\build\runtime'
$outDir = Join-Path $hallJoyRoot '..\..\build\bin\MAD68ProRNative\Release\x64'
$targetName = 'HallJoy'
$exe = Join-Path $outDir ($targetName + '.exe')
$pdb = Join-Path $outDir ($targetName + '.pdb')
$map = Join-Path $outDir ($targetName + '.map')
$releaseDir = Join-Path $root 'build\release'
$dependencyLockPath = Join-Path $root 'tools\dependency-lock.json'
$thirdPartyNoticesPath = Join-Path $root 'THIRD_PARTY_NOTICES.md'
if (-not (Test-Path -LiteralPath $dependencyLockPath -PathType Leaf)) {
    throw "Dependency lock is missing: $dependencyLockPath"
}
$dependencyLock = Get-Content -LiteralPath $dependencyLockPath -Raw | ConvertFrom-Json
$vigemSpec = $dependencyLock.binaryInputs.vigemClient
$vigemLib = Join-Path $root ([string]$vigemSpec.path).Replace('/', '\')
$vigemExpectedSize = [long]$vigemSpec.size
$vigemExpectedSha256 = [string]$vigemSpec.sha256
$vigemInstallerSpec = $dependencyLock.runtimeDependencies.vigemBus
$vigemInstaller = Join-Path $root ([string]$vigemInstallerSpec.installerPath).Replace('/', '\')
$vigemInstallerLicense = Join-Path (Split-Path -Parent $vigemInstaller) 'LICENSE'
$vigemInstallerExpectedSize = [long]$vigemInstallerSpec.installerSize
$vigemInstallerExpectedSha256 = [string]$vigemInstallerSpec.installerSha256

$required = @(
    $dependencyLockPath,
    $thirdPartyNoticesPath,
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
    (Join-Path $pluginRoot 'halljoy_analog_provider_v2_contract.h'),
    (Join-Path $pluginRoot 'halljoy_uap_provider_v2.h'),
    (Join-Path $pluginRoot 'halljoy_uap_provider_v2_projection.h'),
    (Join-Path $pluginRoot 'halljoy_native_hid_claim.h'),
    $project,
    (Join-Path $hallJoyRoot 'HallJoy\ui_paint_audit.h'),
    (Join-Path $hallJoyRoot 'HallJoy\mad68pr_backend.cpp'),
    (Join-Path $hallJoyRoot 'HallJoy\mad68pr_protocol.cpp'),
    (Join-Path $hallJoyRoot 'HallJoy\hex80_backend.cpp'),
    (Join-Path $hallJoyRoot 'HallJoy\hex80_protocol.cpp'),
    (Join-Path $hallJoyRoot 'HallJoy\aula_win60he_backend.cpp'),
    (Join-Path $hallJoyRoot 'HallJoy\aula_win60he_backend.h'),
    (Join-Path $hallJoyRoot 'HallJoy\aula_win60he_client.cpp'),
    (Join-Path $hallJoyRoot 'HallJoy\aula_win60he_client.h'),
    (Join-Path $hallJoyRoot 'HallJoy\aula_win60he_diagnostic_metrics.cpp'),
    (Join-Path $hallJoyRoot 'HallJoy\aula_win60he_diagnostic_metrics.h'),
    (Join-Path $hallJoyRoot 'HallJoy\aula_win60he_protocol.cpp'),
    (Join-Path $hallJoyRoot 'HallJoy\aula_win60he_protocol.h'),
    (Join-Path $hallJoyRoot 'HallJoy\aula_win60he_session_policy.cpp'),
    (Join-Path $hallJoyRoot 'HallJoy\aula_win60he_session_policy.h'),
    (Join-Path $hallJoyRoot 'HallJoy\aula_w669_backend.cpp'),
    (Join-Path $hallJoyRoot 'HallJoy\aula_w669_backend.h'),
    (Join-Path $hallJoyRoot 'HallJoy\aula_w669_protocol.cpp'),
    (Join-Path $hallJoyRoot 'HallJoy\aula_w669_protocol.h'),
    (Join-Path $hallJoyRoot 'HallJoy\native_analog_routing.cpp'),
    (Join-Path $hallJoyRoot 'HallJoy\native_hid_interface_claim_registry.h'),
    (Join-Path $hallJoyRoot 'HallJoy\analog_provider_v2.h'),
    (Join-Path $hallJoyRoot 'HallJoy\analog_provider_v2.cpp'),
    (Join-Path $hallJoyRoot 'HallJoy\provider_v2_data_plane_layout.h'),
    (Join-Path $hallJoyRoot 'HallJoy\provider_v2_data_plane_layout.cpp'),
    (Join-Path $hallJoyRoot 'HallJoy\provider_v2_data_plane_windows.h'),
    (Join-Path $hallJoyRoot 'HallJoy\provider_v2_data_plane_windows.cpp'),
    (Join-Path $hallJoyRoot 'HallJoy\uap_parent_snapshot.h'),
    (Join-Path $hallJoyRoot 'HallJoy\uap_parent_snapshot.cpp'),
    (Join-Path $hallJoyRoot 'HallJoy\provider_v2_controller_shadow.h'),
    (Join-Path $hallJoyRoot 'HallJoy\provider_v2_controller_shadow.cpp'),
    (Join-Path $hallJoyRoot 'HallJoy\provider_v2_qualification_model.h'),
    (Join-Path $hallJoyRoot 'HallJoy\provider_v2_qualification_model.cpp'),
    (Join-Path $hallJoyRoot 'HallJoy\provider_v2_qualification_report.h'),
    (Join-Path $hallJoyRoot 'HallJoy\provider_v2_qualification_report.cpp'),
    (Join-Path $hallJoyRoot 'HallJoy\native_analog_backend.h'),
    (Join-Path $hallJoyRoot 'HallJoy\native_analog_backend_registry.h'),
    (Join-Path $hallJoyRoot 'HallJoy\native_analog_backend_registry.cpp'),
    (Join-Path $hallJoyRoot 'HallJoy\native_analog_backends.def'),
    (Join-Path $hallJoyRoot 'HallJoy\input_wake_sequence.h'),
    (Join-Path $hallJoyRoot 'HallJoy\publication_generation.h'),
    (Join-Path $hallJoyRoot 'HallJoy\latest_value_mailbox.h'),
    (Join-Path $hallJoyRoot 'HallJoy\sparklink_hotplug_age.h'),
    (Join-Path $hallJoyRoot 'tests\sparklink_hotplug_age_test.cpp'),
    (Join-Path $hallJoyRoot 'tests\sparklink_hotplug_age_static_audit.py'),
    (Join-Path $hallJoyRoot 'HallJoy\monotonic_time.h'),
    (Join-Path $hallJoyRoot 'HallJoy\saturating_int.h'),
    (Join-Path $hallJoyRoot 'tests\runtime_arithmetic_test.cpp'),
    (Join-Path $hallJoyRoot 'tests\runtime_arithmetic_static_audit.py'),
    (Join-Path $hallJoyRoot 'tests\protocol_parser_fuzz_smoke_test.cpp'),
    (Join-Path $hallJoyRoot 'tests\prerelease_hardening_static_audit.py'),
    (Join-Path $hallJoyRoot 'tests\pre_release_ui_static_audit.py'),
    (Join-Path $hallJoyRoot 'tests\resource_ownership_static_audit.py'),
    (Join-Path $hallJoyRoot 'tests\native_backend_architecture_static_audit.py'),
    (Join-Path $hallJoyRoot 'tests\startup_wake_transaction_static_audit.py'),
    (Join-Path $hallJoyRoot 'tests\vigem_output_isolation_static_audit.py'),
    (Join-Path $hallJoyRoot 'tests\vigem_output_process_static_audit.py'),
    (Join-Path $hallJoyRoot 'tests\vigem_output_real_child_static_audit.py'),
    (Join-Path $hallJoyRoot 'tests\vigem_output_self_host_static_audit.py'),
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
    (Join-Path $hallJoyRoot 'tests\analog_provider_v2_test.cpp'),
    (Join-Path $hallJoyRoot 'tests\analog_provider_v2_static_audit.py'),
    (Join-Path $hallJoyRoot 'tests\provider_v2_data_plane_layout_test.cpp'),
    (Join-Path $hallJoyRoot 'tests\provider_v2_data_plane_windows_test.cpp'),
    (Join-Path $hallJoyRoot 'tests\provider_v2_data_plane_layout_static_audit.py'),
    (Join-Path $hallJoyRoot 'tests\provider_v2_data_plane_windows_static_audit.py'),
    (Join-Path $hallJoyRoot 'tests\uap_provider_v2_static_audit.py'),
    (Join-Path $hallJoyRoot 'tests\uap_provider_v2_projection_test.cpp'),
    (Join-Path $hallJoyRoot 'tests\uap_parent_snapshot_test.cpp'),
    (Join-Path $hallJoyRoot 'tests\provider_v2_controller_shadow_test.cpp'),
    (Join-Path $hallJoyRoot 'tests\provider_v2_controller_shadow_static_audit.py'),
    (Join-Path $hallJoyRoot 'tests\provider_v2_qualification_model_test.cpp'),
    (Join-Path $hallJoyRoot 'tests\provider_v2_qualification_static_audit.py'),
    (Join-Path $hallJoyRoot 'HallJoy\configured_xusb_builder.h'),
    (Join-Path $hallJoyRoot 'HallJoy\configured_xusb_builder.cpp'),
    (Join-Path $hallJoyRoot 'HallJoy\virtual_controller_frame.h'),
    (Join-Path $hallJoyRoot 'HallJoy\xusb_output_adapter.h'),
    (Join-Path $hallJoyRoot 'HallJoy\xusb_output_adapter.cpp'),
    (Join-Path $hallJoyRoot 'tests\configured_xusb_builder_test.cpp'),
    (Join-Path $hallJoyRoot 'tests\configured_xusb_builder_static_audit.py'),
    (Join-Path $hallJoyRoot 'tests\native_analog_backend_contract_test.cpp'),
    (Join-Path $hallJoyRoot 'tests\native_hid_interface_claim_test.cpp'),
    (Join-Path $hallJoyRoot 'tests\native_hid_interface_claim_static_audit.py'),
	(Join-Path $hallJoyRoot 'tests\keychron_k4he_static_audit.py'),
	(Join-Path $hallJoyRoot 'tests\uap_hid_receive_event_static_audit.py'),
    (Join-Path $hallJoyRoot 'tests\aula_win60he_backend_static_audit.py'),
    (Join-Path $hallJoyRoot 'tests\aula_win60he_diagnostic_metrics_test.cpp'),
    (Join-Path $hallJoyRoot 'tests\aula_win60he_protocol_test.cpp'),
    (Join-Path $hallJoyRoot 'tests\aula_win60he_oracle_test.cpp'),
    (Join-Path $hallJoyRoot 'tests\aula_win60he_oracle_fixtures.h'),
    (Join-Path $hallJoyRoot 'tests\aula_win60he_end_to_end_test.cpp'),
    (Join-Path $hallJoyRoot 'tests\aula_win60he_session_policy_test.cpp'),
    (Join-Path $hallJoyRoot 'tests\aula_w669_protocol_test.cpp'),
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
    (Join-Path $root 'tools\build_provider_v2_qualification.ps1'),
    (Join-Path $root 'tools\run_provider_v2_qualification_smoke.ps1'),
    (Join-Path $root 'tools\run_aula_win60he_sanitizers.py'),
    (Join-Path $root 'tools\run_protocol_fuzz_sanitizers.py'),
    (Join-Path $root 'tools\check_overlay_responsiveness.py'),
    (Join-Path $root 'tools\check_overlay_http_framing.py'),
    (Join-Path $root 'tools\check_overlay_concurrency_origin.py'),
    (Join-Path $root 'tools\fuzz_overlay_http.py'),
    (Join-Path $root 'tools\run_production_smoke.ps1'),
    (Join-Path $root 'tools\run_release_qualification.ps1'),
    (Join-Path $root 'tools\run_ui_scroll_stress.ps1'),
    (Join-Path $root 'tools\run_long_soak.ps1'),
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
    (Join-Path $hallJoyRoot 'HallJoy\vigem_output_shared.h'),
    (Join-Path $hallJoyRoot 'HallJoy\vigem_output_channel.h'),
    (Join-Path $hallJoyRoot 'HallJoy\vigem_output_channel.cpp'),
    (Join-Path $hallJoyRoot 'HallJoy\vigem_output_process_protocol.h'),
    (Join-Path $hallJoyRoot 'HallJoy\vigem_output_process_host.h'),
    (Join-Path $hallJoyRoot 'HallJoy\vigem_output_process_host.cpp'),
    (Join-Path $hallJoyRoot 'HallJoy\vigem_output_self_host_test.h'),
    (Join-Path $hallJoyRoot 'HallJoy\vigem_output_self_host_test.cpp'),
    (Join-Path $hallJoyRoot 'HallJoy\vigem_output_runtime.h'),
    (Join-Path $hallJoyRoot 'HallJoy\vigem_output_runtime.cpp'),
    (Join-Path $hallJoyRoot 'HallJoy\vigem_output_process_client.h'),
    (Join-Path $hallJoyRoot 'HallJoy\vigem_output_process_client.cpp'),
    (Join-Path $hallJoyRoot 'HallJoy\process_generation_supervisor.h'),
    (Join-Path $hallJoyRoot 'HallJoy\process_generation_supervisor.cpp'),
    (Join-Path $hallJoyRoot 'HallJoy\protected_native_handle.h'),
    (Join-Path $hallJoyRoot 'HallJoy\vigem_child_transport.h'),
    (Join-Path $hallJoyRoot 'HallJoy\vigem_child_transport.cpp'),
    (Join-Path $hallJoyRoot 'HallJoy\app.cpp'),
    (Join-Path $hallJoyRoot 'HallJoy\addressed_analog_backend.cpp'),
    (Join-Path $hallJoyRoot 'HallJoy\debug_log.cpp'),
    (Join-Path $root 'tools\run_vigem_output_self_host_test.ps1'),
    (Join-Path $root 'tools\run_vigem_output_real_child_test.ps1'),
    $vigemLib,
    $vigemInstaller,
    $vigemInstallerLicense
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
$vigemRuntimeText = Get-Content -LiteralPath (Join-Path $hallJoyRoot 'HallJoy\vigem_output_runtime.cpp') -Raw
$vigemProcessClientText = Get-Content -LiteralPath (Join-Path $hallJoyRoot 'HallJoy\vigem_output_process_client.cpp') -Raw
$vigemSupervisorText = Get-Content -LiteralPath (Join-Path $hallJoyRoot 'HallJoy\process_generation_supervisor.cpp') -Raw
$vigemTransportText = Get-Content -LiteralPath (Join-Path $hallJoyRoot 'HallJoy\vigem_child_transport.cpp') -Raw
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
$debugLogHeaderText = Get-Content -LiteralPath (Join-Path $hallJoyRoot 'HallJoy\debug_log.h') -Raw
$stabilityTraceHeaderText = Get-Content -LiteralPath (Join-Path $hallJoyRoot 'HallJoy\stability_trace.h') -Raw
if ($projectText -notmatch 'HALLJOY_MAD68PR_NATIVE;HALLJOY_PRODUCTION' -or
    $projectText -match 'HALLJOY_MAD68PR_NATIVE;HALLJOY_DIAGNOSTIC') {
    throw 'Final-build preflight failed: native target must be production and must not enable file diagnostics.'
}
$linkConfigurationCount = ([regex]::Matches($projectText, '<AdditionalDependencies>')).Count
$synchronizationLibraryCount = ([regex]::Matches($projectText, 'Synchronization\.lib')).Count
if ($linkConfigurationCount -eq 0 -or $synchronizationLibraryCount -ne $linkConfigurationCount) {
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
    $debugLogHeaderText -notmatch '#define DebugLog_Write\(\.\.\.\) do \{ if constexpr \(false\)' -or
    $stabilityTraceHeaderText -notmatch '#define StabilityTrace_Write\(\.\.\.\) do \{ if constexpr \(false\)' -or
    $debugLogText -notmatch '(?s)#elif defined\(HALLJOY_PRODUCTION\).*?SetUnhandledExceptionFilter' -or
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
    $backendText -notmatch 'scheduler remains in realtime, but the complete snapshot' -or
    $backendText -notmatch 'Backend_GetNextOutputDeadlineQpc' -or
    $backendText -notmatch 'g_vigemOutputRuntime.TryPublish' -or
    $backendText -notmatch 'const std::uint32_t validMask = \(1u << outputCount\) - 1u' -or
    $backendText -match 'VigemOutputThreadProc|VigemOutputThreadBody|g_vigemOutputMailbox|Vigem_ReconnectThrottled' -or
    $backendText -match 'vigem_target_x360_update|vigem_connect\(|vigem_alloc\(' -or
    $vigemRuntimeText -notmatch 'OutputProcessSession session' -or
    $vigemRuntimeText -notmatch 'accessAdmission' -or
    $vigemRuntimeText -notmatch 'WaitForSessionLeases' -or
    $vigemProcessClientText -notmatch 'PrepareOutputTermination' -or
    $vigemSupervisorText -notmatch 'prepareTermination' -or
    ([regex]::Matches($vigemTransportText, 'vigem_target_x360_update')).Count -ne 1 -or
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
    $keyboardSubpagesText -notmatch 'BuildAnalogDiagnosticsLines' -or
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
$rawInputPublishedIndex = $appText.IndexOf('g_rawInputRegistered.store(rawInputRegistered')
$startupFunctionIndex = $appText.IndexOf('static bool AppStartBackendDependents(bool rawInputRegistered')
$startupFunctionEnd = $appText.IndexOf('static bool EngineRuntimeCloseAdmission', $startupFunctionIndex)
$engineFreshStartIndex = $appText.IndexOf('static bool EngineRuntimeStartFreshGeneration')
$enginePassesRawInputIndex = $appText.IndexOf('AppStartBackendDependents(g_rawInputRegistered.load', $engineFreshStartIndex)
if ($rawInputRegistrationIndex -lt 0 -or $rawInputPublishedIndex -lt $rawInputRegistrationIndex -or
    $startupFunctionIndex -lt 0 -or $startupFunctionEnd -le $startupFunctionIndex -or
    $engineFreshStartIndex -lt 0 -or $enginePassesRawInputIndex -lt $engineFreshStartIndex) {
    throw 'Regression guard failed: engine-owned startup no longer carries registered Raw Input into backend admission.'
}
$startupFunctionText = $appText.Substring($startupFunctionIndex, $startupFunctionEnd - $startupFunctionIndex)
$rawInputGuardIndex = $startupFunctionText.IndexOf('if (!rawInputRegistered)')
$rawInputPhaseIndex = $startupFunctionText.IndexOf('NativeAnalogBackends_StartPhase(NativeAnalogStartPhase::AfterRawInput)')
if ($rawInputGuardIndex -lt 0 -or $rawInputPhaseIndex -lt $rawInputGuardIndex) {
    throw 'Regression guard failed: AfterRawInput native phase is not gated by confirmed Raw Input registration.'
}
$routingResetIndex = $appText.IndexOf('NativeAnalogBackends_Reset()')
$routingPrepareIndex = $appText.IndexOf('NativeAnalogBackends_PrepareRouting()')
$backendInitIndex = $appText.IndexOf('Backend_Init()')
$afterRealtimeIndex = $appText.IndexOf('NativeAnalogBackends_StartPhase(NativeAnalogStartPhase::AfterRealtime)')
if ($routingResetIndex -lt 0 -or $routingPrepareIndex -lt $routingResetIndex -or
    $backendInitIndex -lt $routingPrepareIndex -or $afterRealtimeIndex -lt 0 -or
    $backendInitIndex -lt $startupFunctionIndex -or
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

$uiAudit = Join-Path $hallJoyRoot 'tests\pre_release_ui_static_audit.py'
Write-Host 'Running pre-release UI static audit...' -ForegroundColor Cyan
& python $uiAudit
if ($LASTEXITCODE -ne 0) { throw "Pre-release UI static audit failed: $LASTEXITCODE" }

$generator = Join-Path $root 'tools\new_native_backend.py'
& python $generator --help | Out-Null
if ($LASTEXITCODE -ne 0) { throw "Native backend generator self-check failed: $LASTEXITCODE" }

$vigemInfo = Get-Item -LiteralPath $vigemLib
$vigemHash = (Get-FileHash -LiteralPath $vigemLib -Algorithm SHA256).Hash
if ($vigemInfo.Length -ne $vigemExpectedSize -or $vigemHash -ne $vigemExpectedSha256) {
    throw "ViGEmClient.lib preflight failed. Size=$($vigemInfo.Length), SHA-256=$vigemHash"
}
$vigemInstallerInfo = Get-Item -LiteralPath $vigemInstaller
$vigemInstallerHash = (Get-FileHash -LiteralPath $vigemInstaller -Algorithm SHA256).Hash
if ($vigemInstallerInfo.Length -ne $vigemInstallerExpectedSize -or
    $vigemInstallerHash -ne $vigemInstallerExpectedSha256) {
    throw "Embedded ViGEmBus installer preflight failed. Size=$($vigemInstallerInfo.Length), SHA-256=$vigemInstallerHash"
}
$vigemInstallerSignature = Get-AuthenticodeSignature -LiteralPath $vigemInstaller
if ($vigemInstallerSignature.Status -ne 'Valid' -or
    -not $vigemInstallerSignature.SignerCertificate -or
    $vigemInstallerSignature.SignerCertificate.Subject -notmatch 'CN=Nefarius Software Solutions e\.U\.') {
    throw "Embedded ViGEmBus installer signature is not the pinned publisher signature. Status=$($vigemInstallerSignature.Status)"
}
Write-Host "Embedded ViGEmBus installer: exact hash and Nefarius signature verified." -ForegroundColor DarkGray

Write-Host 'Building the embedded UAP with capability-validated native routing...' -ForegroundColor Cyan
& powershell -NoProfile -ExecutionPolicy Bypass -File $pluginBuild `
    -ExcludeMad68ProRNative -DependencyLock $dependencyLockPath
if ($LASTEXITCODE -ne 0) { throw "Plugin build failed: $LASTEXITCODE" }

$patchedHid = Join-Path $root '.cache\uap\Soup\soup\hwHid.cpp'
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

$pluginOut = Join-Path $root 'build\bin\UAP\native\universal-analog-plugin'
$abi0 = Join-Path $pluginOut 'abiv0.dll'
$abi1 = Join-Path $pluginOut 'abiv1.dll'
if (-not (Test-Path -LiteralPath $abi0) -or -not (Test-Path -LiteralPath $abi1)) {
    throw 'Universal Analog Plugin build did not produce abiv0.dll and abiv1.dll.'
}
$uapAbiCheck = Join-Path $root 'tools\check_private_uap_abi.py'
try {
    $activeHallJoy = @(
        Get-CimInstance Win32_Process -Filter "Name='HallJoy.exe'" -ErrorAction Stop
    )
}
catch {
    throw 'Cannot prove an isolated physical-UAP session because HallJoy process enumeration failed.'
}
if ($activeHallJoy.Count -ne 0) {
    $owners = ($activeHallJoy | ForEach-Object {
        $path = if ($_.ExecutablePath) { [string]$_.ExecutablePath } else { '<unknown>' }
        "PID=$($_.ProcessId) path=$path"
    }) -join '; '
    throw "Official build's physical-UAP runtime gate requires every HallJoy instance to be closed: $owners"
}
& python $uapAbiCheck $abi1
if ($LASTEXITCODE -ne 0) { throw "Private UAP ABI runtime gate failed: $LASTEXITCODE" }
New-Item -ItemType Directory -Path $runtime -Force | Out-Null
Copy-Item -LiteralPath $abi0 -Destination (Join-Path $runtime 'universal_analog_abiv0.dll') -Force
Copy-Item -LiteralPath $abi1 -Destination (Join-Path $runtime 'universal_analog_abiv1.dll') -Force

# A static audit cannot detect a loader which silently substitutes defaults.
# The simulator embeds the same runtime resources: materialize them FIRST on a
# clean checkout, then test file roundtrips with backend initialization forbidden.
Write-Host 'Running production-linked settings/profile roundtrip tests...' -ForegroundColor Cyan
& (Join-Path $root 'tools\run_profile_transaction_tests.ps1')

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

# Rebuild owns compiler intermediates; never recursively erase adjacent runtime data.
Write-Host "Building $targetName.exe with MAD68 + Hex80 + Addressed + Aula + Spark + Sayo + UAP..." -ForegroundColor Cyan
$buildOutput = @(& $msbuild $project `
    '/t:Rebuild' `
    '/p:Configuration=Release' `
    '/p:Platform=x64' `
    '/p:HallJoyMad68ProRNative=true' `
    '/p:HallJoyStabilityTrace=false' `
    '/m' 2>&1)
$buildExitCode = $LASTEXITCODE
$buildOutput | Out-Host
if ($buildExitCode -ne 0) { throw "HallJoy build failed: $buildExitCode" }
$productionWarnings = @($buildOutput | Where-Object { [string]$_ -match ': warning (?:C|LNK)\d+:' })
$allowedProductionWarning = 'ViGEmClient\.lib\(ViGEmClient\.obj\)\s*: warning LNK4099:'
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

$vigemSelfTest = Start-Process -FilePath $exe `
    -ArgumentList '--halljoy-verify-embedded-vigem-installer' `
    -WindowStyle Hidden -Wait -PassThru
if ($vigemSelfTest.ExitCode -ne 0) {
    throw "Linked embedded ViGEmBus resource verification failed: $($vigemSelfTest.ExitCode)"
}
Write-Host 'Linked embedded ViGEmBus resource/extraction/signature self-test: PASS.' -ForegroundColor DarkGray

# All continuous telemetry must remain isolated to explicit tester builds.
# Check the linked image, not only preprocessor settings, so an accidental
# configuration leak fails the official release before packaging.
$productionWideStrings = [Text.Encoding]::Unicode.GetString([IO.File]::ReadAllBytes($exe))
foreach ($forbiddenDiagnosticMarker in @(
    'HallJoyStabilityTrace.log',
    'HallJoyStabilityTrace.previous.log',
    'HallJoyDiagnostic.log',
    'matrix.health',
    'matrix.activity',
    'matrix.session_summary',
    'matrix.coverage',
    'ten_key_gate=1',
    'HallJoyProviderV2Qualification.txt'
)) {
    if ($productionWideStrings.Contains($forbiddenDiagnosticMarker)) {
        throw "Production image contains isolated Aula diagnostic telemetry: $forbiddenDiagnosticMarker"
    }
}
if (-not $productionWideStrings.Contains('HallJoyCrash.txt')) {
    throw 'Production image is missing its crash-only report marker.'
}
foreach ($requiredVigemMarker in @(
    'ViGEmBus_1.22.0_x64_x86_arm64.exe',
    'Install ViGEmBus 1.22.0 (recommended)',
    'No installer is downloaded at runtime.'
)) {
    if (-not $productionWideStrings.Contains($requiredVigemMarker)) {
        throw "Production image is missing embedded ViGEm installer marker: $requiredVigemMarker"
    }
}
# The SHA-256 is stored as 32 binary bytes in production, not as a diagnostic
# string. The immediately preceding self-test hashes the exact linked RCDATA
# resource and verifies Authenticode, so a textual hash marker is neither
# expected nor used as an integrity oracle.
$productionWideStrings = $null
Write-Host 'Production continuous telemetry markers: absent; crash-only report retained.' -ForegroundColor DarkGray

# One distribution directory. Preserve local runtime files if someone launches
# HallJoy here; packaging must never erase user profiles or unknown files.
$preservedRuntimeNames = @(
    'settings.ini', 'bindings.ini', 'settings.ini.pre-bundle.bak',
    'GlobalProfiles', 'Layouts', 'CurvePresets', 'HallJoy.portable'
)
New-Item -ItemType Directory -Path $releaseDir -Force | Out-Null
Copy-Item -LiteralPath $exe -Destination $releaseDir -Force
Copy-Item -LiteralPath $dependencyLockPath -Destination $releaseDir -Force
Copy-Item -LiteralPath $thirdPartyNoticesPath -Destination $releaseDir -Force
$releaseHash = Get-FileHash -LiteralPath (Join-Path $releaseDir 'HallJoy.exe') -Algorithm SHA256
"$($releaseHash.Hash)  HallJoy.exe" | Set-Content -LiteralPath (Join-Path $releaseDir 'SHA256SUMS.txt') -Encoding ASCII
# PDB/MAP stay beside their matching compiler output; no second symbol copy.
Write-Host ''
Write-Host 'Build completed:' -ForegroundColor Green
Write-Host "  $(Join-Path $releaseDir ($targetName + '.exe'))" -ForegroundColor Green
Write-Host "  SHA256 $($releaseHash.Hash)" -ForegroundColor Green
Write-Host 'Final production profile: continuous telemetry is disabled; crash-only reporting is retained.' -ForegroundColor Green
exit 0
