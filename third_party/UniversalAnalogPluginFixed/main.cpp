#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <soup/AnalogueKeyboard.hpp>
#include <soup/HidScancode.hpp>
#include <soup/os.hpp>
#include <soup/RecursiveMutex.hpp>
#include <soup/Thread.hpp>
#include <soup/UniquePtr.hpp>

#include <algorithm>
#include <array>
#include <atomic>
#include <bitset>
#include <chrono>
#include <cmath>
#include <condition_variable>
#include <cstdint>
#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <cctype>
#include <mutex>
#include <thread>
#include <unordered_map>
#if SOUP_WINDOWS
#include <windows.h>
#endif

#include "halljoy_plugin_telemetry.h"
#include "halljoy_dense_snapshot.h"
#include "halljoy_uap_cabi_guard.h"
#include "halljoy_uap_device_identity.h"
#include "halljoy_uap_poll_pacing.h"

#define LOGGING false

// Some Analog SDK apps rely on read_full_buffer sending a 0 value for released keys instead of stopping to report the key.
#define REPORT_RELEASED_KEYS true

#ifndef UAP_DISABLE_HOTPLUG
#define UAP_DISABLE_HOTPLUG 0
#endif

#ifndef UAP_POLL_TARGET_US
#define UAP_POLL_TARGET_US 1000
#endif

// HallJoy Madlions diagnostic: poll poll-only keyboards synchronously from
// read_full_buffer/read_analog instead of creating a Soup worker thread.
#ifndef UAP_SYNCHRONOUS_POLL
#define UAP_SYNCHRONOUS_POLL 0
#endif

#if LOGGING
#include <iostream>
#endif

#if REPORT_RELEASED_KEYS
#include <unordered_set>
#endif


// Optional HallJoy diagnostic bridge. The host supplies a pointer to its shared
// checkpoint field. These exports are ignored by ordinary SDK/plugin users.
#if SOUP_WINDOWS
static std::atomic<volatile LONG*> halljoy_checkpoint_target{ nullptr };
static std::atomic<volatile LONG*> halljoy_transport_error_target{ nullptr };
static std::atomic_bool running{ false };
static std::atomic_bool halljoy_restart_blocked{ false };
static std::atomic<std::uint32_t> halljoy_plugin_fault{ 0 };

SOUP_CEXPORT void halljoy_set_diagnostic_checkpoint(volatile LONG* target) noexcept
{
	halljoy_checkpoint_target.store(target, std::memory_order_release);
}

SOUP_CEXPORT void halljoy_set_diagnostic_transport_error(volatile LONG* target) noexcept
{
	halljoy_transport_error_target.store(target, std::memory_order_release);
}

extern "C" void halljoy_plugin_checkpoint(int checkpoint)
{
	if (auto* target = halljoy_checkpoint_target.load(std::memory_order_acquire))
	{
		InterlockedExchange(target, static_cast<LONG>(checkpoint));
	}
}

extern "C" void halljoy_plugin_transport_error(uint32_t error)
{
	if (auto* target = halljoy_transport_error_target.load(std::memory_order_acquire))
	{
		InterlockedExchange(target, static_cast<LONG>(error));
	}
}
#else
static std::atomic_bool running{ false };
static std::atomic_bool halljoy_restart_blocked{ false };
static std::atomic<std::uint32_t> halljoy_plugin_fault{ 0 };
extern "C" void halljoy_plugin_checkpoint(int) {}
extern "C" void halljoy_plugin_transport_error(uint32_t) {}
#endif

static void halljoy_mark_plugin_fault(std::uint32_t error) noexcept
{
	halljoy_plugin_fault.store(error, std::memory_order_release);
	halljoy_restart_blocked.store(true, std::memory_order_release);
	running.store(false, std::memory_order_release);
	halljoy_plugin_transport_error(error);
}


static uint64_t halljoy_telemetry_now_us()
{
#if SOUP_WINDOWS
	static LARGE_INTEGER frequency = []() {
		LARGE_INTEGER value{};
		QueryPerformanceFrequency(&value);
		return value;
	}();
	LARGE_INTEGER counter{};
	QueryPerformanceCounter(&counter);
	if (frequency.QuadPart <= 0)
	{
		return static_cast<uint64_t>(GetTickCount64()) * 1000ull;
	}
	const uint64_t ticks = static_cast<uint64_t>(counter.QuadPart);
	const uint64_t ticks_per_second = static_cast<uint64_t>(frequency.QuadPart);
	return (ticks / ticks_per_second) * 1000000ull
		+ ((ticks % ticks_per_second) * 1000000ull) / ticks_per_second;
#else
	using namespace std::chrono;
	return static_cast<uint64_t>(duration_cast<microseconds>(steady_clock::now().time_since_epoch()).count());
#endif
}

static bool halljoy_contains_ascii_ci(const std::string& text, const char* needle)
{
	if (needle == nullptr || *needle == '\0')
	{
		return true;
	}
	const std::string wanted(needle);
	if (wanted.size() > text.size())
	{
		return false;
	}
	for (size_t i = 0; i + wanted.size() <= text.size(); ++i)
	{
		bool same = true;
		for (size_t j = 0; j < wanted.size(); ++j)
		{
			const unsigned char a = static_cast<unsigned char>(text[i + j]);
			const unsigned char b = static_cast<unsigned char>(wanted[j]);
			if (std::tolower(a) != std::tolower(b))
			{
				same = false;
				break;
			}
		}
		if (same)
		{
			return true;
		}
	}
	return false;
}

static bool halljoy_uap_native_hid_excluded(std::uint16_t vendor_id, std::uint16_t product_id)
{
#if defined(UAP_EXCLUDE_HALLJOY_NATIVE)
    const char* configured = std::getenv("HALLJOY_UAP_NATIVE_HID_IDS");
    if (configured == nullptr || *configured == '\0')
    {
        return false;
    }
    char token[32]{};
    std::snprintf(token, sizeof(token), "vid_%04x&pid_%04x",
        static_cast<unsigned>(vendor_id), static_cast<unsigned>(product_id));
    return halljoy_contains_ascii_ci(configured, token);
#else
    (void)vendor_id;
    (void)product_id;
    return false;
#endif
}

static void halljoy_copy_ascii(char* destination, size_t destination_size, const std::string& source)
{
	if (destination == nullptr || destination_size == 0)
	{
		return;
	}
	const size_t count = (source.size() < destination_size - 1) ? source.size() : destination_size - 1;
	if (count != 0)
	{
		memcpy(destination, source.data(), count);
	}
	destination[count] = '\0';
}

// Types

enum HallJoyPluginCheckpoint : int
{
	HjPluginReadEntry = 200,
	HjPluginBeforeDeviceLock = 210,
	HjPluginBeforeKeyboardUpdate = 220,
	HjPluginAfterKeyboardUpdate = 230,
	HjPluginReadReturn = 240,
};

using DeviceID = uint64_t;

enum class DeviceEventType : int
{
	Connected = 1,
	Disconnected = 2,
};

enum class DeviceType : int
{
	Keyboard = 1,
	Keypad = 2,
	Other = 3,
};

using DeviceInfo = void; // opaque

struct DeviceInfo_FFI
{
	uint16_t vendor_id;
	uint16_t product_id;
	const char* manufacturer_name;
	const char* device_name;
	DeviceID device_id;
	DeviceType device_type;
};

// Rust interop stuff

#if ABI_VERSION_TARGET == 0
	#if SOUP_WINDOWS
		#pragma comment(lib, "wooting_analog_common.lib")
		#pragma comment(lib, "Userenv.lib")
		#pragma comment(lib, "ntdll.lib")
		#pragma comment(lib, "Bcrypt.lib")
		#pragma comment(lib, "Ws2_32.lib")
		#pragma comment(lib, "Advapi32.lib")
	#else
		#pragma comment(lib, "./wooting_analog_common.a")
	#endif

extern "C"
{
	DeviceInfo* new_device_info(uint16_t vendor_id, uint16_t product_id, const char* manufacturer_name, const char* device_name, DeviceID device_id, DeviceType device_type);
	void drop_device_info(DeviceInfo* device);
}
#else
inline DeviceInfo* new_device_info(uint16_t vendor_id, uint16_t product_id, const char* manufacturer_name, const char* device_name, DeviceID device_id, DeviceType device_type)
{
	return reinterpret_cast<DeviceInfo*>(new DeviceInfo_FFI{ vendor_id, product_id, manufacturer_name, device_name, device_id, device_type });
}

inline void drop_device_info(DeviceInfo* device)
{
	delete reinterpret_cast<DeviceInfo_FFI*>(device);
}
#endif

// Boilerplate

SOUP_CEXPORT const uint32_t ANALOG_SDK_PLUGIN_ABI_VERSION = ABI_VERSION_TARGET;

#if ABI_VERSION_TARGET >= 1
#define _name name
#define _device_info device_info
#define _initialise initialise
#define _read_full_buffer read_full_buffer
#endif

SOUP_CEXPORT const char* _name() noexcept
{
	return "Universal Analog Plugin (HallJoy SafeHID v11 stable-identity deadline-paced telemetry)";
}

SOUP_CEXPORT bool is_initialised() noexcept
{
	return running.load(std::memory_order_acquire)
		&& !halljoy_restart_blocked.load(std::memory_order_acquire);
}

// Devices

[[nodiscard]] static uint16_t mapToWootingKey(soup::Key key);

// HallJoy V9: every device worker publishes independently. The isolated host
// waits on this generation instead of imposing a fixed 8 ms polling cadence.
// The condition variable is process-local: HallJoy loads this DLL inside the
// isolated analog-host process and calls the optional export directly.
static std::atomic<uint64_t> halljoy_snapshot_generation{ 0 };
static std::mutex halljoy_snapshot_wait_mtx{};
static std::condition_variable halljoy_snapshot_wait_cv{};

static void halljoy_signal_snapshot_update()
{
	halljoy_snapshot_generation.fetch_add(1, std::memory_order_release);
	halljoy_snapshot_wait_cv.notify_one();
}

SOUP_CEXPORT uint64_t halljoy_wait_for_snapshot_update(uint64_t last_generation, uint32_t timeout_ms) noexcept
{
	return halljoy::uap::CAbiInvoke<uint64_t>(last_generation, [&]() {
		std::unique_lock<std::mutex> lock(halljoy_snapshot_wait_mtx);
		const auto changed = [last_generation]() {
			return halljoy_snapshot_generation.load(std::memory_order_acquire) != last_generation;
		};
		if (!changed())
		{
			halljoy_snapshot_wait_cv.wait_for(lock, std::chrono::milliseconds(timeout_ms), changed);
		}
		return halljoy_snapshot_generation.load(std::memory_order_acquire);
	}, []() noexcept { halljoy_mark_plugin_fault(0xE0470001u); });
}

struct Device
{
	DeviceID id;
	DeviceInfo* info;
	soup::AnalogueKeyboard kbd;
	soup::Thread thrd;
	std::string manufacturer_name;
	bool duplicate_safe_id = false;
	bool synchronous_poll = false;
	bool poll_transport = false;
	uint32_t telemetry_rows = 0;
	uint32_t telemetry_columns = 0;
	uint32_t telemetry_layout_key_slots = 0;
	uint32_t telemetry_nominal_levels = 0;
	static constexpr std::size_t KEY_COUNT = HallJoyDenseSnapshot::kKeyCount;

	// V11: every worker publishes one coherent dense HID table. This removes the
	// legacy 16-active-key truncation and makes reads independent of key ordering.
	soup::RecursiveMutex snapshot_mtx{};
	std::array<float, KEY_COUNT> key_values{};
	std::uint32_t active_key_count = 0;
	std::uint64_t snapshot_generation = 0;
	std::uint64_t snapshot_timestamp_us = 0;
	uint64_t telemetry_last_update_us = 0;
	uint64_t telemetry_update_count = 0;
	uint32_t telemetry_avg_interval_us = 0;
	uint32_t telemetry_max_interval_us = 0;
	uint64_t worker_started_us = 0;
	std::bitset<65536> telemetry_seen_levels{};
	uint32_t telemetry_observed_levels = 1; // zero/released level
	// Per-key observations are quantised to 12 bits. This preserves all known
	// parser resolutions in the bundled stack (up to 1024 levels) without
	// pretending that a float value exposes the keyboard's ADC internals.
	static constexpr uint32_t TELEMETRY_LEVEL_BUCKETS = 4096;
	std::array<std::bitset<TELEMETRY_LEVEL_BUCKETS>, 256> telemetry_key_seen_levels{};
	std::array<uint16_t, 256> telemetry_key_observed_levels{};

	Device(soup::AnalogueKeyboard&& _kbd, halljoy::uap::DeviceIdentity assigned_identity,
		std::string&& assigned_manufacturer_name)
		: id(assigned_identity.id), kbd(std::move(_kbd)),
		  manufacturer_name(std::move(assigned_manufacturer_name)),
		  duplicate_safe_id(assigned_identity.duplicate_safe),
		  synchronous_poll(UAP_SYNCHRONOUS_POLL != 0 && kbd.isPoll()), poll_transport(kbd.isPoll())
	{
		telemetry_seen_levels.set(0);
		const std::string combined_name = manufacturer_name + " " + kbd.name;
		if (kbd.hid.usage_page == 0xFF54)
		{
			telemetry_nominal_levels = 256;
		}
		else if (kbd.hid.usage_page == 0xFF53)
		{
			telemetry_nominal_levels = 1024;
		}
		else if (kbd.hid.vendor_id == 0x1532 || halljoy_contains_ascii_ci(combined_name, "Razer"))
		{
			telemetry_nominal_levels = 256;
		}
		else if (kbd.hid.vendor_id == 0x352d || halljoy_contains_ascii_ci(combined_name, "DrunkDeer"))
		{
			// Soup decodes every supported DrunkDeer through the same 6x21
			// transport matrix, even when the physical keyboard has fewer keys.
			telemetry_rows = 6;
			telemetry_columns = 21;
			telemetry_layout_key_slots = 6u * 21u;
			telemetry_nominal_levels = 41;
		}
		else if (kbd.hid.vendor_id == 0x3434 || kbd.hid.vendor_id == 0x362D ||
			halljoy_contains_ascii_ci(combined_name, "Keychron") || halljoy_contains_ascii_ci(combined_name, "Lemokey"))
		{
			telemetry_nominal_levels = 236;
			if (kbd.hid.product_id == 0x0B10 || kbd.hid.product_id == 0x0B11 || kbd.hid.product_id == 0x0B12 ||
				kbd.hid.product_id == 0x0610 || kbd.hid.product_id == 0x0611)
			{
				telemetry_rows = 6;
				telemetry_columns = 15;
			}
			else if (kbd.hid.product_id == 0x0B30 || kbd.hid.product_id == 0x0E20 ||
				kbd.hid.product_id == 0x0E21 || kbd.hid.product_id == 0x0E22)
			{
				telemetry_rows = 6;
				telemetry_columns = 16;
			}
			else if (kbd.hid.product_id == 0x0B50)
			{
				telemetry_rows = 6;
				telemetry_columns = 19;
			}
			telemetry_layout_key_slots = telemetry_rows * telemetry_columns;
		}
		else if (kbd.hid.vendor_id == 0x373b || halljoy_contains_ascii_ci(combined_name, "Madlions") || halljoy_contains_ascii_ci(kbd.name, "MAD"))
		{
			// The pinned SafeHID parser clamps the wire travel to 0..350 and
			// publishes travel / 350 for a freshly queried key. The bundled
			// layouts are five logical rows: 14 slots for MAD60 and 15 for MAD68.
			telemetry_nominal_levels = 351;
			if (kbd.hid.product_id == 0x1055 || kbd.hid.product_id == 0x1056 ||
				kbd.hid.product_id == 0x105D || kbd.hid.product_id == 0x1053 || kbd.hid.product_id == 0x1054)
			{
				telemetry_rows = 5;
				telemetry_columns = 14;
			}
			else if (kbd.hid.product_id == 0x1058 || kbd.hid.product_id == 0x1059 ||
				kbd.hid.product_id == 0x105A || kbd.hid.product_id == 0x105C || kbd.hid.product_id == 0x10A7)
			{
				telemetry_rows = 5;
				telemetry_columns = 15;
			}
			telemetry_layout_key_slots = telemetry_rows * telemetry_columns;
		}
		else if (kbd.hid.vendor_id == 0x19f5 || halljoy_contains_ascii_ci(combined_name, "NuPhy"))
		{
			// Soup stores the NuPhy stream in an 8-bit per-key cache before
			// publishing it to the plugin ABI.
			telemetry_nominal_levels = 256;
		}
		info = new_device_info(kbd.hid.vendor_id, kbd.hid.product_id, manufacturer_name.c_str(), kbd.name.c_str(), this->id, DeviceType::Keyboard);
	}

	~Device()
	{
		drop_device_info(info);
	}

	void publish_dense(const std::array<float, KEY_COUNT>& dense)
	{
		const uint64_t now_us = halljoy_telemetry_now_us();
		std::uint32_t active_count = 0;
		{
			halljoy::uap::LockGuard<soup::RecursiveMutex> snapshot_lock(snapshot_mtx);
		for (std::size_t code = 0; code < KEY_COUNT; ++code)
		{
			const float value = std::clamp(dense[code], 0.0f, 1.0f);
			key_values[code] = value;
			if (value > 0.0f)
			{
				++active_count;
				const uint32_t level = static_cast<uint32_t>(std::lround(value * 65535.0f));
				if (!telemetry_seen_levels.test(level))
				{
					telemetry_seen_levels.set(level);
					++telemetry_observed_levels;
				}

				auto& key_seen = telemetry_key_seen_levels[code];
				auto& key_count = telemetry_key_observed_levels[code];
				if (key_count == 0)
				{
					key_seen.set(0);
					key_count = 1;
				}
				const uint32_t key_level = static_cast<uint32_t>(std::lround(
					value * static_cast<float>(TELEMETRY_LEVEL_BUCKETS - 1)));
				if (!key_seen.test(key_level))
				{
					key_seen.set(key_level);
					++key_count;
				}
			}
		}
		active_key_count = active_count;
		++snapshot_generation;
		snapshot_timestamp_us = now_us;
		if (telemetry_last_update_us != 0 && now_us > telemetry_last_update_us)
		{
			const uint32_t interval_us = static_cast<uint32_t>(std::min<uint64_t>(now_us - telemetry_last_update_us, 60000000ull));
			if (telemetry_avg_interval_us == 0)
			{
				telemetry_avg_interval_us = interval_us;
			}
			else if (interval_us > telemetry_avg_interval_us)
			{
				telemetry_avg_interval_us += (interval_us - telemetry_avg_interval_us + 7u) / 8u;
			}
			else
			{
				telemetry_avg_interval_us -= (telemetry_avg_interval_us - interval_us + 7u) / 8u;
			}
			telemetry_max_interval_us = (std::max)(telemetry_max_interval_us, interval_us);
		}
		telemetry_last_update_us = now_us;
			++telemetry_update_count;
		}
		halljoy_signal_snapshot_update();
	}

	void clear_snapshot()
	{
		std::array<float, KEY_COUNT> empty{};
		publish_dense(empty);
	}

	bool poll_worker_stale(uint64_t now_us, uint64_t timeout_us)
	{
		if (!poll_transport || synchronous_poll || worker_started_us == 0)
		{
			return false;
		}
		halljoy::uap::LockGuard<soup::RecursiveMutex> snapshot_lock(snapshot_mtx);
		const uint64_t reference_us = telemetry_last_update_us != 0 ? telemetry_last_update_us : worker_started_us;
		const bool stale = now_us > reference_us && now_us - reference_us > timeout_us;
		return stale;
	}

	void get_telemetry(HallJoyPluginTelemetry::DeviceV1& out)
	{
		out = HallJoyPluginTelemetry::DeviceV1{};
		out.structSize = sizeof(HallJoyPluginTelemetry::DeviceV1);
		out.deviceId = id;
		out.vendorId = kbd.hid.vendor_id;
		out.productId = kbd.hid.product_id;
		out.usagePage = kbd.hid.usage_page;
		out.usage = kbd.hid.usage;
		out.flags = HallJoyPluginTelemetry::DeviceFlag_Connected;
		if (duplicate_safe_id)
		{
			out.flags |= HallJoyPluginTelemetry::DeviceFlag_DuplicateSafeId;
		}
		out.flags |= poll_transport
			? HallJoyPluginTelemetry::DeviceFlag_PolledTransport
			: HallJoyPluginTelemetry::DeviceFlag_StreamTransport;
		if (synchronous_poll)
		{
			out.flags |= HallJoyPluginTelemetry::DeviceFlag_SynchronousHallJoyPoll;
		}
		else if (poll_transport && UAP_POLL_TARGET_US != 0)
		{
			out.flags |= HallJoyPluginTelemetry::DeviceFlag_DeadlinePacedWorker;
		}
		else if (poll_transport)
		{
			out.flags |= HallJoyPluginTelemetry::DeviceFlag_UnthrottledWorker;
		}
		out.rows = telemetry_rows;
		out.columns = telemetry_columns;
		out.layoutKeySlots = telemetry_layout_key_slots;
		out.nominalRawLevels = telemetry_nominal_levels;
		out.inputReportBytes = kbd.hid.input_report_byte_length;
		out.outputReportBytes = kbd.hid.output_report_byte_length;
		out.featureReportBytes = kbd.hid.feature_report_byte_length;
		out.bluetooth = kbd.hid.isBluetooth() ? 1u : 0u;
		halljoy_copy_ascii(out.manufacturer, sizeof(out.manufacturer), manufacturer_name);
		halljoy_copy_ascii(out.name, sizeof(out.name), kbd.name);

		const uint64_t now_us = halljoy_telemetry_now_us();
		halljoy::uap::LockGuard<soup::RecursiveMutex> snapshot_lock(snapshot_mtx);
		out.observedDistinctLevels = telemetry_observed_levels;
		uint32_t observed_key_count = 0;
		uint32_t observed_key_min = UINT32_MAX;
		uint32_t observed_key_max = 0;
		uint64_t observed_key_sum = 0;
		for (const uint16_t levels : telemetry_key_observed_levels)
		{
			if (levels == 0)
			{
				continue;
			}
			++observed_key_count;
			observed_key_min = std::min<uint32_t>(observed_key_min, levels);
			observed_key_max = std::max<uint32_t>(observed_key_max, levels);
			observed_key_sum += levels;
		}
		out.observedKeys = observed_key_count;
		out.observedLevelsPerKeyMin = observed_key_count != 0 ? observed_key_min : 0;
		out.observedLevelsPerKeyMax = observed_key_max;
		out.observedLevelsPerKeyAverage10 = observed_key_count != 0
			? static_cast<uint32_t>((observed_key_sum * 10ull + observed_key_count / 2u) / observed_key_count)
			: 0;
		out.activeKeys = active_key_count;
		out.averageUpdateIntervalUs = telemetry_avg_interval_us;
		out.maximumUpdateIntervalUs = telemetry_max_interval_us;
		out.updateCount = telemetry_update_count;
		if (telemetry_avg_interval_us != 0)
		{
			out.updateHz10 = static_cast<uint32_t>(std::min<uint64_t>(1000000ull,
				(10000000ull + telemetry_avg_interval_us / 2u) / telemetry_avg_interval_us));
		}
		if (telemetry_last_update_us != 0 && now_us >= telemetry_last_update_us)
		{
			out.lastUpdateAgeMs = static_cast<uint32_t>(std::min<uint64_t>((now_us - telemetry_last_update_us) / 1000ull, 0xffffffffull));
		}
	}

	void update_from_keyboard()
	{
		std::array<float, KEY_COUNT> dense{};
		for (const auto& key : kbd.getActiveKeys())
		{
			const uint16_t code = mapToWootingKey(key.getSoupKey());
			if (code >= KEY_COUNT)
			{
				continue;
			}
			float value = key.getFValue();
			if (!std::isfinite(value) || value <= 0.0f)
				value = 0.0f;
			else if (value > 1.0f)
				value = 1.0f;
			dense[code] = (std::max)(dense[code], value);
		}
		publish_dense(dense);
	}
};

static soup::RecursiveMutex devices_mtx{};
static soup::RecursiveMutex full_buffer_mtx{};
static std::vector<soup::UniquePtr<Device>> devices{};

SOUP_CEXPORT int _device_info(DeviceInfo* buffer[], uint32_t len) noexcept
{
	if (buffer == nullptr || len == 0 || !is_initialised())
		return 0;
	return halljoy::uap::CAbiInvoke<int>(-2000, [&]() {
		halljoy::uap::LockGuard<soup::RecursiveMutex> devices_lock(devices_mtx);
		len = static_cast<uint32_t>(std::min<std::size_t>(len, devices.size()));
		for (uint32_t i = 0; i != len; ++i)
			buffer[i] = devices[i]->info;
		return static_cast<int>(len);
	}, []() noexcept { halljoy_mark_plugin_fault(0xE0470002u); });
}

SOUP_CEXPORT uint32_t halljoy_get_device_telemetry(
	HallJoyPluginTelemetry::DeviceV1* buffer,
	uint32_t len,
	uint32_t element_size) noexcept
{
	if (buffer == nullptr || len == 0 || !is_initialised()
		|| element_size != sizeof(HallJoyPluginTelemetry::DeviceV1))
	{
		return 0;
	}
	return halljoy::uap::CAbiInvoke<uint32_t>(0, [&]() {
		halljoy::uap::LockGuard<soup::RecursiveMutex> devices_lock(devices_mtx);
		const uint32_t count = static_cast<uint32_t>(std::min<size_t>(
			std::min<uint32_t>(len, HallJoyPluginTelemetry::kMaxDevices), devices.size()));
		for (uint32_t i = 0; i < count; ++i)
			devices[i]->get_telemetry(buffer[i]);
		return count;
	}, []() noexcept { halljoy_mark_plugin_fault(0xE0470003u); });
}


SOUP_CEXPORT uint32_t halljoy_get_dense_snapshots(
	HallJoyDenseSnapshot::DeviceV1* buffer,
	uint32_t len,
	uint32_t element_size) noexcept
{
	if (buffer == nullptr || len == 0 || !is_initialised()
		|| element_size != sizeof(HallJoyDenseSnapshot::DeviceV1))
	{
		return 0;
	}
	return halljoy::uap::CAbiInvoke<uint32_t>(0, [&]() {
		halljoy::uap::LockGuard<soup::RecursiveMutex> devices_lock(devices_mtx);
		const uint32_t count = static_cast<uint32_t>(std::min<size_t>(
			std::min<uint32_t>(len, HallJoyDenseSnapshot::kMaxDevices), devices.size()));
		for (uint32_t i = 0; i < count; ++i)
		{
			const auto& dev = devices[i];
			auto& out = buffer[i];
			out = HallJoyDenseSnapshot::DeviceV1{};
			out.structSize = sizeof(HallJoyDenseSnapshot::DeviceV1);
			out.version = HallJoyDenseSnapshot::kVersion;
			out.deviceId = dev->id;
			out.vendorId = dev->kbd.hid.vendor_id;
			out.productId = dev->kbd.hid.product_id;
			out.usagePage = dev->kbd.hid.usage_page;
			out.usage = dev->kbd.hid.usage;
			out.flags = HallJoyDenseSnapshot::DeviceFlag_Connected;
			if (dev->duplicate_safe_id)
			{
				out.flags |= HallJoyDenseSnapshot::DeviceFlag_DuplicateSafeId;
			}
			out.flags |= dev->poll_transport
				? HallJoyDenseSnapshot::DeviceFlag_PolledTransport
				: HallJoyDenseSnapshot::DeviceFlag_StreamTransport;
			halljoy::uap::LockGuard<soup::RecursiveMutex> snapshot_lock(dev->snapshot_mtx);
			out.generation = dev->snapshot_generation;
			out.timestampUs = dev->snapshot_timestamp_us;
			out.activeKeyCount = dev->active_key_count;
			std::copy(dev->key_values.begin(), dev->key_values.end(), out.values);
		}
		return count;
	}, []() noexcept { halljoy_mark_plugin_fault(0xE0470004u); });
}

// Actual important stuff

static soup::Thread discover_thread;

[[nodiscard]] static uint16_t mapToWootingKey(soup::Key key)
{
	switch (key)
	{
	case soup::KEY_NEXT_TRACK: return 0x3B5;
	case soup::KEY_PREV_TRACK: return 0x3B6;
	case soup::KEY_STOP_MEDIA: return 0x3B7;
	case soup::KEY_PLAY_PAUSE: return 0x3CD;
	case soup::KEY_OEM_5: return 0x401;
	case soup::KEY_OEM_6: return 0x402;
	case soup::KEY_OEM_1: return 0x403;
	case soup::KEY_OEM_2: return 0x404;
	case soup::KEY_OEM_3: return 0x405;
	case soup::KEY_OEM_4: return 0x408;
	case soup::KEY_FN: return 0x409;
	default:;
	}
	return soup::soup_key_to_hid_scancode(key);
}

using event_handler_t = void(*)(void* data, DeviceEventType eventType, DeviceInfo* deviceInfo);

static std::atomic<void*> event_handler_data{ nullptr };
static std::atomic<event_handler_t> event_handler{ nullptr };

static void send_device_event(DeviceEventType type, DeviceInfo* info)
{
	auto callback = event_handler.load(std::memory_order_acquire);
	if (callback != nullptr && running.load(std::memory_order_acquire))
	{
		callback(event_handler_data.load(std::memory_order_acquire), type, info);
	}
}

static void start_device_worker(Device& dev)
{
	dev.worker_started_us = halljoy_telemetry_now_us();
	dev.thrd.start([](soup::Capture&& cap)
	{
		try
		{
			Device& dev = *cap.get<Device*>();
			soup::AnalogueKeyboard& kbd = dev.kbd;
			const bool is_poll_device = kbd.isPoll();
			halljoy::uap::PollPacingPolicy poll_pacing(UAP_POLL_TARGET_US);

			while (running.load(std::memory_order_acquire) && !kbd.disconnected)
			{
				if (is_poll_device)
				{
					poll_pacing.BeginCycle(halljoy_telemetry_now_us());
				}
				dev.update_from_keyboard();
				if (is_poll_device)
				{
					const bool transaction_succeeded = !kbd.disconnected &&
						(kbd.hid.vendor_id != 0x373b || kbd.madlions.consecutive_failed_reports == 0);
					const std::uint32_t wait_us = poll_pacing.CompleteCycle(
						halljoy_telemetry_now_us(), transaction_succeeded);
					if (wait_us != 0)
					{
						std::this_thread::sleep_for(std::chrono::microseconds(wait_us));
					}
				}
			}
			dev.clear_snapshot();
		}
		catch (...)
		{
			halljoy_mark_plugin_fault(0xE0470010u);
		}
#if LOGGING
		std::cout << "Thread for " << kbd.name << " is stopping" << std::endl;
#endif
	}, &dev);
}

static bool contains_device_id(DeviceID id)
{
	bool known = false;
	halljoy::uap::LockGuard<soup::RecursiveMutex> devices_lock(devices_mtx);
	for (const auto& dev : devices)
	{
		if (dev->id == id)
		{
			known = true;
			break;
		}
	}
	return known;
}

static void remove_stopped_devices()
{
	for (;;)
	{
		Device* stopped = nullptr;
		{
			halljoy::uap::LockGuard<soup::RecursiveMutex> devices_lock(devices_mtx);
			for (const auto& dev : devices)
			{
				if (!dev->thrd.isRunning())
				{
					stopped = dev.get();
					break;
				}
			}
		}

		if (stopped == nullptr)
		{
			break;
		}

		// Notify while DeviceInfo is still valid. The callback is deliberately
		// outside devices_mtx because the SDK may synchronously call back into us.
		send_device_event(DeviceEventType::Disconnected, stopped->info);

		{
			halljoy::uap::LockGuard<soup::RecursiveMutex> devices_lock(devices_mtx);
			for (auto it = devices.begin(); it != devices.end(); ++it)
			{
				if (it->get() == stopped)
				{
					devices.erase(it);
					break;
				}
			}
		}
	}
}

static void discover_devices(bool initial)
{
	if (!initial)
	{
		remove_stopped_devices();
	}

	auto kbds = soup::AnalogueKeyboard::getAll();
	std::unordered_map<DeviceID, std::uint32_t> identity_occurrences;
	for (auto& kbd : kbds)
	{
#if defined(UAP_EXCLUDE_HALLJOY_NATIVE)
		// Redundant identity guard. In the dedicated native target Soup already
		// skips this VID/PID before CreateFileW, so the UAP child never opens EP82.
		// Keep this second check in case a future Soup enumeration path bypasses the
		// pre-open hook. All other UAP/Wooting devices remain unchanged.
		if (halljoy_uap_native_hid_excluded(kbd.hid.vendor_id, kbd.hid.product_id))
		{
#if LOGGING
			std::cout << "Skipping capability-validated HallJoy native analogue device in UAP" << std::endl;
#endif
			continue;
		}
#endif
#ifndef WOOTING_SUPPORT
		if (kbd.hid.usage_page == 0xFF54)
		{
			continue;
		}
#endif

		std::string manufacturer_name = kbd.hid.getManufacturerName();
		const halljoy::uap::DeviceIdentityInput identity_input{
			kbd.hid.vendor_id,
			kbd.hid.product_id,
			kbd.hid.usage_page,
			kbd.hid.usage,
			kbd.hid.path,
			manufacturer_name,
			kbd.name,
		};
		const DeviceID identity_base = halljoy::uap::MakeDeviceIdentityBase(identity_input);
		const std::uint32_t occurrence = identity_occurrences[identity_base]++;
		const halljoy::uap::DeviceIdentity identity =
			halljoy::uap::MakeDeviceIdentity(identity_input, occurrence);
		if (contains_device_id(identity.id))
		{
			continue;
		}

#if LOGGING
		std::cout << "New device: " << kbd.name << std::endl;
#endif
		auto upDev = soup::make_unique<Device>(
			std::move(kbd), identity, std::move(manufacturer_name));
		Device* raw = upDev.get();

		// Publish the object before starting its thread or announcing it to the SDK.
		{
			halljoy::uap::LockGuard<soup::RecursiveMutex> devices_lock(devices_mtx);
			devices.emplace_back(std::move(upDev));
		}

		if (!raw->synchronous_poll)
		{
			start_device_worker(*raw);
		}
		if (!initial)
		{
			send_device_event(DeviceEventType::Connected, raw->info);
		}
	}
}

SOUP_CEXPORT int _initialise(void* data, event_handler_t callback) noexcept
{
	return halljoy::uap::CAbiInvoke<int>(-2000, [&]() {
#if LOGGING
	AllocConsole();
	{
		FILE* f;
		freopen_s(&f, "CONIN$", "r", stdin);
		freopen_s(&f, "CONOUT$", "w", stderr);
		freopen_s(&f, "CONOUT$", "w", stdout);
	}
#endif

	if (halljoy_restart_blocked.load(std::memory_order_acquire))
		return -2000;
	if (running.exchange(true, std::memory_order_acq_rel))
	{
		halljoy::uap::LockGuard<soup::RecursiveMutex> devices_lock(devices_mtx);
		const auto count = static_cast<int>(devices.size());
		return count;
	}

	event_handler_data.store(data, std::memory_order_release);
	event_handler.store(callback, std::memory_order_release);
	discover_devices(true);

#if LOGGING
	std::cout << "Discovered " << devices.size() << " initial devices" << std::endl;
#endif

	int num_initial_devices = 0;
	{
		halljoy::uap::LockGuard<soup::RecursiveMutex> devices_lock(devices_mtx);
		num_initial_devices = static_cast<int>(devices.size());
	}

#if !UAP_DISABLE_HOTPLUG
	discover_thread.start([](soup::Capture&&)
	{
		try
		{
			while (running.load(std::memory_order_acquire))
			{
				for (uint16_t i = 0; i != 100 && running.load(std::memory_order_acquire); ++i)
					soup::os::sleep(10);
				if (running.load(std::memory_order_acquire))
					discover_devices(false);
			}
		}
		catch (...)
		{
			halljoy_mark_plugin_fault(0xE0470011u);
		}
	});
#endif
	return num_initial_devices;
	}, []() noexcept { halljoy_mark_plugin_fault(0xE0470005u); });
}

#if REPORT_RELEASED_KEYS
static std::unordered_set<uint16_t> pending_release;
#endif

SOUP_CEXPORT float read_analog(uint16_t code, DeviceID device_id) noexcept
{
	if (code >= HallJoyDenseSnapshot::kKeyCount || !is_initialised())
		return 0.0f;
	return halljoy::uap::CAbiInvoke<float>(0.0f, [&]() {
		float ret = 0.0f;
		halljoy::uap::LockGuard<soup::RecursiveMutex> devices_lock(devices_mtx);
		for (const auto& dev : devices)
		{
			if (device_id == 0 || dev->id == device_id)
			{
				if (dev->synchronous_poll && !dev->kbd.disconnected)
				{
					halljoy_plugin_checkpoint(HjPluginBeforeKeyboardUpdate);
					dev->update_from_keyboard();
					halljoy_plugin_checkpoint(HjPluginAfterKeyboardUpdate);
				}
				halljoy::uap::LockGuard<soup::RecursiveMutex> snapshot_lock(dev->snapshot_mtx);
				ret = (std::max)(ret, dev->key_values[code]);
			}
		}
		return ret;
	}, []() noexcept { halljoy_mark_plugin_fault(0xE0470006u); });
}

SOUP_CEXPORT int _read_full_buffer(uint16_t* code_buffer, float* analog_buffer, uint32_t len, DeviceID device_id) noexcept
{
	halljoy_plugin_checkpoint(HjPluginReadEntry);
	SOUP_IF_UNLIKELY (len == 0)
	{
		return 0;
	}
	SOUP_IF_UNLIKELY (code_buffer == nullptr || analog_buffer == nullptr)
	{
		return 0;
	}
	SOUP_IF_UNLIKELY (!is_initialised())
	{
		return 0;
	}

	return halljoy::uap::CAbiInvoke<int>(-2000, [&]() {
	// pending_release is process-global plugin state, so serialize the complete
	// full-buffer operation even if an SDK application calls it from two threads.
	halljoy::uap::LockGuard<soup::RecursiveMutex> full_buffer_lock(full_buffer_mtx);

	uint32_t actives = 0;
	bool matched_device = false;
	bool live_device = false;
	bool stale_poll_device = false;
	std::array<float, HallJoyDenseSnapshot::kKeyCount> merged{};
	const uint64_t snapshot_now_us = halljoy_telemetry_now_us();
	halljoy_plugin_checkpoint(HjPluginBeforeDeviceLock);
	{
		halljoy::uap::LockGuard<soup::RecursiveMutex> devices_lock(devices_mtx);
		for (const auto& dev : devices)
		{
			if (device_id == 0 || dev->id == device_id)
			{
				matched_device = true;
				if (dev->poll_worker_stale(snapshot_now_us, 1500000ull))
					stale_poll_device = true;
				if (dev->synchronous_poll && !dev->kbd.disconnected)
				{
					halljoy_plugin_checkpoint(HjPluginBeforeKeyboardUpdate);
					dev->update_from_keyboard();
					halljoy_plugin_checkpoint(HjPluginAfterKeyboardUpdate);
				}
				if (!dev->kbd.disconnected)
					live_device = true;
				halljoy::uap::LockGuard<soup::RecursiveMutex> snapshot_lock(dev->snapshot_mtx);
				for (std::size_t code = 0; code < merged.size(); ++code)
					merged[code] = (std::max)(merged[code], dev->key_values[code]);
			}
		}
	}

	for (std::size_t code = 0; code < merged.size() && actives < len; ++code)
	{
		if (merged[code] > 0.0f)
		{
			code_buffer[actives] = static_cast<uint16_t>(code);
			analog_buffer[actives] = merged[code];
			++actives;
		}
	}

	// A persistent transport failure must not leave the parent process using a
	// frozen snapshot forever. The isolated host treats this as a controlled
	// failure and restarts with a freshly enumerated HID handle.
	if (matched_device && (!live_device || stale_poll_device))
	{
		halljoy_plugin_checkpoint(349);
		return -2000;
	}

#if REPORT_RELEASED_KEYS
	if (!pending_release.empty())
	{
		for (uint32_t i = 0; i < actives; ++i)
		{
			pending_release.erase(code_buffer[i]);
		}

		for (const auto& code : pending_release)
		{
			if (actives == len)
			{
				break;
			}
			code_buffer[actives] = code;
			analog_buffer[actives] = 0.0f;
			++actives;
		}
		pending_release.clear();
	}

	for (uint32_t i = 0; i < actives; ++i)
	{
		if (analog_buffer[i] != 0.0f)
		{
			pending_release.emplace(code_buffer[i]);
		}
	}
#endif

	halljoy_plugin_checkpoint(HjPluginReadReturn);
	return static_cast<int>(actives);
	}, []() noexcept { halljoy_mark_plugin_fault(0xE0470007u); });
}

static bool halljoy_wait_thread_until(
	soup::Thread& thread,
	const std::chrono::steady_clock::time_point& deadline) noexcept
{
	if (!thread.isAttached())
		return true;
#if SOUP_WINDOWS
	const auto now = std::chrono::steady_clock::now();
	if (now >= deadline)
		return false;
	const auto remaining = std::chrono::duration_cast<std::chrono::milliseconds>(deadline - now);
	const DWORD wait_ms = static_cast<DWORD>(std::max<std::int64_t>(1, remaining.count()));
	if (WaitForSingleObject(thread.handle, wait_ms) != WAIT_OBJECT_0)
		return false;
	thread.awaitCompletion();
	return true;
#else
	while (thread.isRunning() && std::chrono::steady_clock::now() < deadline)
		soup::os::sleep(1);
	if (thread.isRunning())
		return false;
	thread.awaitCompletion();
	return true;
#endif
}

static bool halljoy_unload_impl(uint32_t timeout_ms)
{
	const bool was_running = running.exchange(false, std::memory_order_acq_rel);
	if (!was_running)
	{
		halljoy::uap::LockGuard<soup::RecursiveMutex> devices_lock(devices_mtx);
		if (devices.empty())
			return !halljoy_restart_blocked.load(std::memory_order_acquire);
	}
	halljoy_snapshot_wait_cv.notify_all();
	const auto deadline = std::chrono::steady_clock::now()
		+ std::chrono::milliseconds(std::max<uint32_t>(1, timeout_ms));

#if !UAP_DISABLE_HOTPLUG
	if (!halljoy_wait_thread_until(discover_thread, deadline))
	{
		halljoy_restart_blocked.store(true, std::memory_order_release);
		return false;
	}
#endif

	// Stop callbacks before destroying DeviceInfo objects.
	event_handler.store(nullptr, std::memory_order_release);
	event_handler_data.store(nullptr, std::memory_order_release);

	std::vector<Device*> workers;
	{
		halljoy::uap::LockGuard<soup::RecursiveMutex> devices_lock(devices_mtx);
		workers.reserve(devices.size());
		for (const auto& dev : devices)
			workers.emplace_back(dev.get());
	}
	for (Device* dev : workers)
	{
		if (!dev->synchronous_poll)
			dev->kbd.hid.cancelReceiveReport();
	}
	for (Device* dev : workers)
	{
		if (!dev->synchronous_poll && !halljoy_wait_thread_until(dev->thrd, deadline))
		{
			halljoy_restart_blocked.store(true, std::memory_order_release);
			return false;
		}
	}
	{
		halljoy::uap::LockGuard<soup::RecursiveMutex> devices_lock(devices_mtx);
		devices.clear();
	}

#if REPORT_RELEASED_KEYS
	{
		halljoy::uap::LockGuard<soup::RecursiveMutex> full_buffer_lock(full_buffer_mtx);
		pending_release.clear();
	}
#endif

#if LOGGING
	FreeConsole();
#endif
	return true;
}

SOUP_CEXPORT bool halljoy_unload_bounded(uint32_t timeout_ms) noexcept
{
	return halljoy::uap::CAbiInvoke<bool>(false,
		[&]() { return halljoy_unload_impl(timeout_ms); },
		[]() noexcept { halljoy_mark_plugin_fault(0xE0470008u); });
}

SOUP_CEXPORT void unload() noexcept
{
	halljoy::uap::CAbiInvokeVoid([]() {
		(void)halljoy_unload_impl(3000);
	}, []() noexcept { halljoy_mark_plugin_fault(0xE0470009u); });
}
