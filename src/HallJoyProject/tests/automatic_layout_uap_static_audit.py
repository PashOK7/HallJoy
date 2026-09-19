from pathlib import Path
root=Path(__file__).resolve().parents[3]
layout=(root/'src/HallJoyProject/HallJoy/keyboard_layout.cpp').read_text(encoding='utf-8')
producer=(root/'third_party/UniversalAnalogPluginFixed/main.cpp').read_text(encoding='utf-8')
identity=(root/'third_party/UniversalAnalogPluginFixed/halljoy_uap_device_identity.h').read_text(encoding='utf-8')
builder=(root/'third_party/UniversalAnalogPluginFixed/tools/build_fixed_plugin.ps1').read_text(encoding='utf-8')
assert 'return DeviceIdentity{ base, true };' in identity
assert 'out.flags |= HallJoyPluginTelemetry::DeviceFlag_DuplicateSafeId;' in producer
assert '!(d.flags & BackendAnalogDeviceFlag_DuplicateSafeId)' not in layout
assert 'd.flags=BackendAnalogDeviceFlag_Connected | BackendAnalogDeviceFlag_DuplicateSafeId;' in layout
assert 'for (const auto& identity:halljoy::layout_selection::kKeychronLayouts)' in layout
assert 'for (const auto& identity : halljoy::layout_selection::kKeychronLayouts)' in producer
assert 'keychron_layout_identities.h' in builder
assert 'automatic.selection' in layout
print('AUTOMATIC_LAYOUT_UAP_STATIC_AUDIT=PASS stable_id_flag=1 shared_matrix_catalog=1 decision_log=1')
