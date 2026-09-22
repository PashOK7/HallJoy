// SPDX-License-Identifier: GPL-2.0-or-later
// K4 onboard build only: JOYSTICK_ENABLE=no, shared keyboard + vendor HID;
// append native XInput only during a leased HallJoy session.
#include "quantum.h"
#include "usb_main.h"
#include "halljoy_onboard.h"
#include <stddef.h>
#include <string.h>

extern const USB_Descriptor_Configuration_t ConfigurationDescriptor;
extern const USB_Descriptor_Device_t DeviceDescriptor;

_Static_assert(offsetof(USB_Descriptor_Configuration_t, Xinput_Interface) +
    sizeof(USB_Descriptor_Interface_t) + XINPUT_HID_DESCRIPTOR_LEN +
    2 * sizeof(USB_Descriptor_Endpoint_t) == sizeof(USB_Descriptor_Configuration_t),
    "XInput must be the final interface for prefix descriptor selection");

void get_usb_descriptor_kb(const uint16_t value, const uint16_t index,
    const uint16_t length, const void **const address, uint16_t *size) {
    (void)index; (void)length;
    static USB_Descriptor_Configuration_t config;
    static USB_Descriptor_Device_t device;
    const bool native = halljoy_onboard_native_descriptor();
    switch (value >> 8) {
        case DTYPE_Device:
            memcpy(&device, &DeviceDescriptor, sizeof(device));
            // Distinct revisions avoid Windows reusing a cached negative OS
            // descriptor result from keyboard-only mode or old firmware.
            device.ReleaseNumber = native ? 0x1213 : 0x1212;
            *address = &device; *size = sizeof(device);
            break;
        case DTYPE_Configuration:
            memcpy(&config, &ConfigurationDescriptor, sizeof(config));
            if (!native) {
                config.Config.TotalInterfaces = TOTAL_INTERFACES - 1;
                config.Config.TotalConfigurationSize = offsetof(USB_Descriptor_Configuration_t, Xinput_Interface);
            }
            *address = &config; *size = config.Config.TotalConfigurationSize;
            break;
        case DTYPE_String:
            if ((value & 255) == 3 && *address && *size >= 2 && *size <= 120) {
                // New firmware identity prevents stale Windows HID collection
                // associations from the old built-in generic joystick. Preserve
                // the full hardware serial, append a stable protocol suffix.
                static uint8_t serial[128];
                const uint8_t suffix[8] = {'H',0,'J',0,'O',0,'1',0};
                memcpy(serial,*address,*size);
                memcpy(serial+*size,suffix,sizeof(suffix));
                *size += sizeof(suffix); serial[0]=(uint8_t)*size;
                *address=serial;
            }
            if ((value & 255) == 0xEE && !native) { *address = NULL; *size = 0; }
            break;
    }
}

void get_usb_vendor_descriptor_kb(uint8_t recipient, uint8_t request,
    const uint16_t value, const uint16_t index, const uint16_t length,
    const void **const address, uint16_t *size) {
    (void)recipient; (void)request; (void)value; (void)index; (void)length;
    if (!halljoy_onboard_native_descriptor()) { *address = NULL; *size = 0; }
}
