from pathlib import Path

root = Path(__file__).resolve().parents[3]
hall = root / 'src/HallJoyProject/HallJoy'
plugin = root / 'third_party/UniversalAnalogPluginFixed'
source = (plugin/'overlay/Soup/soup/AnalogueKeyboard.cpp').read_text(encoding='utf-8')
v1 = source.split('AnalogueKeyboard::getActiveKeysWootingV1()',1)[1].split('AnalogueKeyboard::getActiveKeysWootingV2()',1)[0]
v2 = source.split('AnalogueKeyboard::getActiveKeysWootingV2()',1)[1].split('AnalogueKeyboard::getActiveKeysRazer()',1)[0]
assert 'wooting_physical::Slot' not in v1
assert 'wooting_physical::Slot' in v2 and 'hid.vendor_id, hid.product_id, matrix_pos' in v2
assert 'KEY_OEM_7 + physical' in v2 and 'static_cast<float>(value) / 1023.0f' in v2
assert 'wooting_scancode_to_soup_key(scancode)' in v2
main = (plugin/'main.cpp').read_text(encoding='utf-8')
for number,name in zip(range(7,11),('kLeftSpace','kRightSpace','kCenterFn','kRightFn')):
    assert f'case soup::KEY_OEM_{number}: return halljoy::wooting_physical::{name};' in main
assert 'IdentityFromLegacyCode(' in main and 'provider_key_values' in main
backend = (hall/'backend.cpp').read_text(encoding='utf-8-sig')
assert 'wooting_physical::IsCode(tracked->keys[i])' in backend
assert 'IsWindowsKeyBound(hid, Bindings_IsHidBound)' in (hall/'app.cpp').read_text(encoding='utf-8-sig')
assert 'wooting_physical_keys_test.cpp' in (root/'tools/run_native_backend_checks.py').read_text(encoding='utf-8')
print('WOOTING_PHYSICAL_KEYS_STATIC_AUDIT=PASS production_wire_path aliases capture blocking regression_route')
