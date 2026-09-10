#pragma once

#include "base.hpp"

#include <vector>

#include "hwHid.hpp"
#include "Key.hpp"

NAMESPACE_SOUP
{
	struct AnalogueKeyboard
	{
		std::string name;
		hwHid hid;
		bool has_ctx_key;
		bool disconnected = false;
		// Immutable after device admission; zero means identity not verified.
		uint8_t drunkdeer_model = 0;
		union
		{
			struct
			{
				uint8_t consecutive_empty_reports;
			} razer;
			struct
			{
				uint8_t am_version;
				uint8_t state;
				const uint8_t* layout;
				uint8_t buffer[NUM_KEYS];
			} keychron;
			struct
			{
				// NuPhy A0 values are 16-bit raw travel. Keeping that domain until
				// publication avoids silently reducing an 800/1600-step stream to
				// the legacy 8-bit cache.
				uint16_t buffer[NUM_KEYS];
			} nuphy;
			struct
			{
				uint8_t state;
				uint8_t consecutive_failed_reports;
				uint8_t failed_reports[64];
				uint8_t layout_size;
				const Key* layout;
				uint8_t buffer[NUM_KEYS];
			} madlions;
		};
		static_assert(sizeof(decltype(nuphy)) >= sizeof(decltype(razer)));
		static_assert(sizeof(decltype(nuphy)) >= sizeof(decltype(keychron)));
		static_assert(sizeof(decltype(nuphy)) >= sizeof(decltype(madlions)));

		AnalogueKeyboard() = default;
		AnalogueKeyboard(std::string&& name, hwHid&& hid, bool has_ctx_key);

		[[nodiscard]] static std::vector<AnalogueKeyboard> getAll(bool include_no_permission = false);

		class ActiveKey
		{
		public:
			Key sk;
			float fvalue;

			[[nodiscard]] Key getSoupKey() const noexcept
			{
				return sk;
			}

			[[nodiscard]] uint8_t getHidScancode() const noexcept;

			[[nodiscard]] uint16_t getPs2Scancode() const noexcept
			{
				return soup_key_to_ps2_scancode(sk);
			}

			[[nodiscard]] float getFValue() const noexcept
			{
				return fvalue;
			}

#if SOUP_WINDOWS
			// E.g., if the key to the right of T (KEY_Y) is pressed, will return 'Y'.
			[[nodiscard]] int getVk() const noexcept
			{
				return soup_key_to_virtual_key(getSoupKey());
			}

			// E.g. if the key to the right of T (KEY_Y) is pressed, may return 'Y' or 'Z' depending on user's layout.
			[[nodiscard]] int getVkTranslated() const noexcept
			{
				return soup_key_to_translated_virtual_key(getSoupKey());
			}
#endif
		};

		[[nodiscard]] bool havePermission() const noexcept
		{
			return hid.havePermission();
		}

		[[nodiscard]] bool isPoll() const noexcept;

		// This will block unless `isPoll()` or `hid.hasReport()` is true.
		[[nodiscard]] std::vector<ActiveKey> getActiveKeys();
	protected:
		[[nodiscard]] std::vector<ActiveKey> getActiveKeysWootingV1();
		[[nodiscard]] std::vector<ActiveKey> getActiveKeysWootingV2();
		[[nodiscard]] std::vector<ActiveKey> getActiveKeysRazer();
		[[nodiscard]] std::vector<ActiveKey> getActiveKeysDrunkdeer();
		[[nodiscard]] std::vector<ActiveKey> getActiveKeysKeychron();
		[[nodiscard]] std::vector<ActiveKey> getActiveKeysNuphy();
		[[nodiscard]] std::vector<ActiveKey> getActiveKeysMadlions();
	};
}
