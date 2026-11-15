from pathlib import Path
root = Path(__file__).resolve().parents[3]
project = (root / 'src/HallJoyProject/HallJoy/HallJoy.vcxproj').read_text(encoding='utf-8-sig')
runner = (root / 'tools/run_profile_transaction_tests.ps1').read_text(encoding='utf-8-sig')
build = (root / 'tools/build_release.ps1').read_text(encoding='utf-8-sig')
install = (root / 'tools/publish_halljoy_build.ps1').read_text(encoding='utf-8-sig')
close = (root / 'tools/close_project_halljoy.ps1').read_text(encoding='utf-8-sig')
guard = (root / 'src/HallJoyProject/HallJoy/instance_guard.cpp').read_text(encoding='utf-8-sig')
assert 'close_project_halljoy' not in project and 'close_project_halljoy' not in runner
assert 'ReleaseCandidate' in build
assert build.index('& $msbuild') < build.index('foreach ($check') < build.index('publish_halljoy_build.ps1')
assert install.index('Candidate matches installed EXE') < install.index('-InspectOnly')
assert '[IO.File]::Replace($pending, $target, $backup)' in install
assert 'finally {' in install and '$state.WasRunning' in install
assert '[HallJoyBuild.ProcessIdentity]::ImagePath' in close and '[Parameter(Mandatory = $true)][string]$TargetPath' in close
identity = (root / 'tools/halljoy_process_identity.ps1').read_text(encoding='utf-8-sig')
assert 'OpenProcess(0x1000' in identity and 'QueryFullProcessImageName' in identity
assert 'replacement stopped' in close and '$restored.WaitForExit' in install
full = (root / 'tools/build.ps1').read_text(encoding='utf-8-sig')
isolation = (root / 'tools/build_uap_isolation.ps1').read_text(encoding='utf-8-sig')
assert full.index('if ($CheckOnly)') < full.index('Invoke-HallJoyUapIsolation {')
assert full.index('Invoke-HallJoyUapIsolation {') < full.index("Official build's physical-UAP runtime gate") < full.index('& python $uapAbiCheck $abi1')
assert 'finally {' in isolation and 'Restore-HallJoyUapTargets $states' in isolation
assert '[HallJoyBuild.ProcessIdentity]::ImagePath' in isolation and '$info.SessionId -ne $session' in isolation
assert isolation.index('$states = @(') < isolation.index('Close-HallJoyUapTarget $state.Target')
assert '#if defined(HALLJOY_ANALOG_SIMULATOR)' in guard
assert 'forbiddenBackend && fileOnly && !root.empty()' in guard and '.FileOnly.%016llx' in guard
print('BUILD_REPLACEMENT_LIFECYCLE=PASS staged exact_target conditional_restart isolated_file_tests')
