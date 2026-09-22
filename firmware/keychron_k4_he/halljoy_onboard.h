// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <stdbool.h>
#include <stdint.h>
bool halljoy_onboard_rx(uint8_t *data, uint8_t length);
void halljoy_onboard_task(uint32_t scan_us);
bool halljoy_onboard_native_descriptor(void);
bool halljoy_onboard_suppressed(uint8_t row, uint8_t col);

void halljoy_onboard_usb_reset(void);

float halljoy_analog_precise(uint8_t row,uint8_t col);
void halljoy_analog_observation(uint8_t row,uint8_t col,uint16_t* raw,uint16_t* zero,uint16_t* full,uint8_t* valid);
