#pragma once
#include "midi_engine.h"
#include "analog_observation.h"
#include <cstdint>
#include <string>
#include <vector>

enum class MidiOutputState : unsigned { Paused, Monitor, Connected, MissingPort, Error, Disabled };
struct MidiTelemetry {
    MidiOutputState state = MidiOutputState::Paused;
    unsigned error = 0, generatedActive = 0, sentActive = 0;
    unsigned lastGenerated = 0, lastSent = 0; // address+1 in low 12 bits, velocity above it.
    std::uint64_t sentMessages = 0, sendErrors = 0, overflows = 0, sparseAttacks = 0;
    std::uint64_t historyLosses=0, invalidObservations=0, snapshotObservations=0;
    std::uint64_t historySamples=0, maximumHistoryAgeUs=0, snapshotGaps=0;
    unsigned uapHistoryMode=0;
    halljoy::midi::TouchReport touch{};
    std::array<halljoy::midi::Note, halljoy::midi::kAddressCount> generated{}, sent{};
};
struct MidiPort { std::wstring name; bool ambiguous = false; };
// Arm the optional runtime; creates an owner/session only when MIDI is enabled.
bool MidiOutput_Start() noexcept;
bool MidiOutput_Reconcile() noexcept; // Existing supervisor, nonblocking reap.
void MidiOutput_SetLifecycleWake(void (*wake)() noexcept) noexcept;
bool MidiOutput_Stop() noexcept; // Failed join retains ownership; caller must poison restart.
void MidiOutput_Panic() noexcept;
void MidiOutput_Retry() noexcept;
void MidiOutput_Tick(halljoy::midi::Input (*read)(void*, std::uint16_t), void* context,
    void (*visit)(void*,halljoy::observation::Sink)=nullptr);
std::vector<MidiPort> MidiOutput_Ports();
void MidiOutput_GetTelemetry(MidiTelemetry& out) noexcept;
std::uint64_t MidiOutput_Generation() noexcept;
std::uint64_t MidiOutput_StatusToken() noexcept;
std::wstring MidiOutput_Status();
// Windows API adapter, production by default. Overrides only in an isolated test target.
#if defined(HALLJOY_MIDI_OUTPUT_TEST)
struct MidiTestApi {
    unsigned (*open)(const std::wstring&, std::uintptr_t&) = nullptr;
    unsigned (*send)(std::uintptr_t, std::uint32_t) = nullptr;
    void (*close)(std::uintptr_t) = nullptr;
    bool (*rejectStart)() = nullptr;
};
void MidiOutput_TestApi(MidiTestApi api);
struct MidiTestStats { bool owner, allocated; std::uint64_t starts, scans, ticks, enumerations; };
MidiTestStats MidiOutput_TestStats() noexcept;
#endif
