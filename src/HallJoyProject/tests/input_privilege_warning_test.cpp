#include "../HallJoy/input_privilege_warning.h"
#include <cassert>
#include <iostream>
using halljoy::input_privilege::Detector;
using halljoy::input_privilege::AnalogPressMailbox;
static AnalogPressMailbox source;

static void press(Detector& d, unsigned key, uint64_t t, bool higher,
                  bool raw, bool hook = false, bool block = false)
{
    source.Observe(key, 0, t);
    source.Observe(key, 950, t + 10);
    source.Observe(key, 0, t + 40); // Whole tap ends before the first UI tick.
    if (raw) d.Digital(key, false, t + 50);
    if (hook) d.Digital(key, true, t + 50);
    d.Sample(key, source.Latest(key), t + 100, higher, block);
    d.Sample(key, source.Latest(key), t + 300, higher, block);
}
int main()
{
    using halljoy::input_privilege::PostingDeniedByUipi;
    assert(PostingDeniedByUipi(false, 5));
    assert(!PostingDeniedByUipi(true, 5)); // Ignore stale error on success.
    assert(!PostingDeniedByUipi(false, 0));
    assert(!PostingDeniedByUipi(false, 1460)); // Timeout is not elevation.
    assert(!PostingDeniedByUipi(false, 1400)); // Destroyed window is not elevation.
    assert(!PostingDeniedByUipi(false, 1816)); // Queue quota is not elevation.
    Detector d;
    for (int i = 0; i < 3; ++i) press(d, 4, 1000 + i * 500, true, false);
    assert(d.warning); // Cold start: short presses qualify without training.
    d = Detector{}; // A new process starts with a fresh detector.
    press(d, 4, 6000, false, true);
    d.ResetSession();
    press(d, 4, 7000, true, false);
    press(d, 4, 7500, true, false);
    assert(!d.warning);
    press(d, 4, 8000, true, false);
    assert(d.warning);
    d.ResetSession();
    assert(d.warning); // Returning to HallJoy does not erase the explanation.
    for (int i = 0; i < 3; ++i) press(d, 4, 9000 + i * 500, false, true);
    assert(d.warning); // Healthy input must not dismiss an already shown advisory.
    d = Detector{};
    assert(!d.warning);
    for (int i = 0; i < 8; ++i) press(d, 4, 11000 + i * 500, false, false);
    assert(!d.warning); // Missing input alone does not mean insufficient rights.
    d.ResetSession();
    press(d, 4, 16000, true, false);
    press(d, 4, 16500, true, false);
    press(d, 4, 28000, true, false);
    assert(!d.warning); // Evidence expires.
    d.ResetSession();
    for (int i = 0; i < 10; ++i) d.Sample(4, source.Latest(4), 29000 + i * 500, true, false);
    assert(!d.warning); // The same mailbox event is never counted twice.
    AnalogPressMailbox held;
    for (int i = 0; i < 10; ++i) held.Observe(4, 1000, 29000 + i * 100);
    assert(held.Latest(4) == 0); // Must first observe a release.
    for (int i = 0; i < 8; ++i) {
        held.Observe(4, 0, 35000 + i * 500);
        held.Observe(4, 600, 35100 + i * 500);
        held.Observe(4, 600, 35300 + i * 500);
    }
    assert(held.Latest(4) == 0); // Shallow analog travel is not a digital press.
    held.Observe(4, 950, 39500);
    held.Observe(4, 950, 39600);
    assert(held.Latest(4) == 39500); // Holding contributes only once.
    held.Observe(0x409, 0, 39700);
    held.Observe(0x409, 950, 39701);
    assert(held.Latest(0x409) == 0); // Fn/vendor codes cannot imply missing HID.
    d.ResetSession();
    for (int i = 0; i < 4; ++i) press(d, 4, 40000 + i * 500, true, false, true, true);
    assert(!d.warning); // A working blocking hook can suppress Raw Input itself.
    for (int i = 0; i < 3; ++i) press(d, 4, 43000 + i * 500, true, true, false, true);
    assert(d.warning); // Raw works but the blocking hook misses presses.
    d.ResetSession();
    assert(d.warning); // Pause/disconnect clears evidence, not the lifetime latch.
    for (int i = 0; i < 8; ++i) press(d, 4, 46000 + i * 500, false, true, true, true);
    assert(d.warning);
    d = Detector{};
    assert(!d.warning); // Restart is the only reset of the advisory.
    std::cout << "INPUT_PRIVILEGE_WARNING_TEST=PASS\n";
}
