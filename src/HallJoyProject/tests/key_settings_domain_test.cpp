#include "key_settings.h"
#include "analog_key_codes.h"

#include <cassert>
#include <vector>

int main()
{
    using namespace halljoy::keycode;
    KeySettings_ClearAll();

    KeyDeadzone fn{};
    fn.useUnique = true;
    fn.invert = true;
    fn.low = 0.21f;
    KeySettings_Set(kFn, fn);
    const KeyDeadzone observedFn = KeySettings_Get(kFn);
    assert(observedFn.useUnique && observedFn.invert);
    assert(KeySettings_GetUseUnique(kFn));

    KeyDeadzone oem{};
    oem.useUnique = true;
    oem.high = 0.77f;
    KeySettings_Set(kOem1, oem);
    assert(KeySettings_GetUseUnique(kOem1));
    assert(KeySettings_Get(0xffffu).useUnique == false);

    const auto prepared = KeySettings_Prepare({ { kFn, oem }, { 0xffffu, fn } });
    auto mutablePrepared = prepared;
    KeySettings_ApplyPrepared(mutablePrepared);
    assert(KeySettings_Get(kFn).useUnique);
    assert(!KeySettings_Get(kOem1).useUnique);

    std::vector<std::pair<uint16_t, KeyDeadzone>> values;
    KeySettings_Enumerate(values);
    assert(values.size() == 1u && values.front().first == kFn);
    return 0;
}
