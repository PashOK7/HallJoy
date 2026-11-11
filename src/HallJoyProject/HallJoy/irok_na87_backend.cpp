#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <setupapi.h>
#include <hidsdi.h>
#include <hidpi.h>

#include "irok_na87_backend.h"
#include "ajazz_raw_limits.h"
#include "irok_na87_protocol.h"
#include "irok_na87_factory.h"
#include "irok_hz94_profile.h"
#include "native_layout_state.h"
#include "physical_analog_state.h"
#include "irok_na87_identity.h"
#include "generated/layout_pipeline/identities.h"
#include "native_analog_backend_registry.h"
#include <memory>
#include <shellapi.h>
#include "debug_log.h"
#include "hid_io_operation.h"
#include "native_analog_routing.h"
#include "realtime_loop.h"
#include "stability_trace.h"
#include "support_log.h"
#include "worker_exception_barrier.h"
#include "worker_join_policy.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <cwchar>
#include <cwctype>
#include <string_view>
#include <mutex>
#include <process.h>
#include <string>
#include <vector>

#pragma comment(lib, "setupapi.lib")
#pragma comment(lib, "hid.lib")

namespace
{
constexpr std::uint16_t kVendorId = 0x0416;
constexpr std::uint16_t kProductId = 0x7372;
constexpr std::uint16_t kUsagePage = 0xff1b;
constexpr std::uint16_t kUsage = 0x0091;
constexpr DWORD kIoTimeoutMs = 120;
constexpr DWORD kProofTimeoutMs = 1200;
constexpr DWORD kReconnectMs = 1000;
constexpr DWORD kStopTimeoutMs = 3000;

struct Handle
{
    HANDLE value = INVALID_HANDLE_VALUE;
    ~Handle() { if (value && value != INVALID_HANDLE_VALUE) CloseHandle(value); }
    Handle() = default;
    explicit Handle(HANDLE handle) : value(handle) {}
    Handle(const Handle&) = delete;
    Handle& operator=(const Handle&) = delete;
    Handle(Handle&& other) noexcept : value(other.value)
    {
        other.value = INVALID_HANDLE_VALUE;
    }
    Handle& operator=(Handle&& other) noexcept
    {
        if (this != &other)
        {
            if (value && value != INVALID_HANDLE_VALUE) CloseHandle(value);
            value = other.value;
            other.value = INVALID_HANDLE_VALUE;
        }
        return *this;
    }
    explicit operator bool() const
    {
        return value && value != INVALID_HANDLE_VALUE;
    }
};

struct Candidate
{
    std::wstring path;
    HIDD_ATTRIBUTES attributes{};
    HIDP_CAPS caps{};
    unsigned interface = 0xff, collection = 0xff;
};

struct Proof
{
    irok_nd75::DeviceInfo identity{};
    irok_nd75::CapabilityInfo capability{};
    irok_nd75::PositionToHid map{};
    bool useControlWrite = false;
    bool exclusive = false;
};

std::atomic<bool> g_prepared{ false }, g_running{ false }, g_stop{ false };
std::atomic<bool> g_present{ false }, g_connected{ false };
std::atomic<unsigned> g_diagnosticStatus{0};
std::atomic<bool> g_ajazzPlaying{false};
std::atomic<unsigned> g_hz94Pid{0};
constexpr auto kHz94Layout = halljoy::layout_identity::Token("witmod-hz94", "7187-AUSHERGB-ANSI");
static_assert(kHz94Layout != 0);

// Prepare and the worker enumerate serially. Store one record per observed
// outcome and leave transient opens eligible for retry.
std::vector<std::string> g_witmodEvidence;
std::vector<std::wstring> g_foreignIdentity;

template<class... Args>
void WitmodEvidence(const Candidate& candidate, const char* stage,
    bool repeat, const char* format, Args... args)
{
    if (!irok_hz94::WitmodProduct(candidate.attributes.ProductID)) return;
    char data[224]{}, record[304]{};
    _snprintf_s(data, sizeof(data), _TRUNCATE, format, args...);
    _snprintf_s(record, sizeof(record), _TRUNCATE,
        "evidence.witmod.%s pid=%04X mi=%02X collection=%02X %s", stage,
        unsigned(candidate.attributes.ProductID), candidate.interface, candidate.collection, data);
    if (!repeat) {
        if (g_witmodEvidence.size() >= 256 ||
            std::find(g_witmodEvidence.begin(), g_witmodEvidence.end(), record) != g_witmodEvidence.end()) return;
        g_witmodEvidence.emplace_back(record);
    }
    SupportLog_Evidence(record, false);
}

unsigned PathField(std::wstring_view path, std::wstring_view tag, unsigned digits, unsigned missing)
{
    const auto at = path.find(tag);
    if (at == path.npos || at + tag.size() + digits > path.size()) return missing;
    unsigned result = 0;
    for (const auto c : path.substr(at + tag.size(), digits)) {
        if (c >= L'0' && c <= L'9') result = result * 16 + unsigned(c - L'0');
        else if (c >= L'a' && c <= L'f') result = result * 16 + unsigned(c - L'a') + 10;
        else return missing;
    }
    return result;
}

std::string Identifier(std::string_view field)
{
    std::string result(field);
    for (char& c : result)
        if (!((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
              (c >= '0' && c <= '9') || c == '_' || c == '-' || c == '.')) c = '_';
    return result;
}

std::uint64_t StreamLayout() noexcept
{
    return g_hz94Pid.load(std::memory_order_acquire) ? kHz94Layout : irok_na87::kAnsiLayoutToken;
}
std::mutex g_serviceMutex, g_handleMutex;
HANDLE g_thread = nullptr, g_wake = nullptr;
HANDLE g_activeHandle = INVALID_HANDLE_VALUE;
std::array<std::atomic<std::uint16_t>, 256> g_milli{};
halljoy::physical_analog::Publication g_factoryValues, g_assignedValues;
std::array<std::atomic<std::uint16_t>,irok_nd75::kMatrixSlots> g_positionMilli{};
std::array<std::atomic<std::uint8_t>, 256> g_owned{};
std::atomic<std::uint32_t> g_inputBytes{ 0 }, g_outputBytes{ 0 };
std::atomic<std::uint32_t> g_active{ 0 }, g_hz10{ 0 }, g_mapped{ 0 };
std::atomic<std::uint32_t> g_avgUs{ 0 }, g_maxUs{ 0 };
std::atomic<std::uint64_t> g_updates{ 0 }, g_failures{ 0 }, g_lastMs{ 0 };
std::atomic<std::uint64_t> g_liveEvents{ 0 }, g_outOfRange{ 0 };
std::atomic<halljoy::worker::WorkerExceptionKind> g_fault{
    halljoy::worker::WorkerExceptionKind::None };

std::uint64_t NowUs()
{
    static const std::uint64_t frequency = [] {
        LARGE_INTEGER value{};
        QueryPerformanceFrequency(&value);
        return static_cast<std::uint64_t>(value.QuadPart);
    }();
    LARGE_INTEGER now{};
    QueryPerformanceCounter(&now);
    if (!frequency) return GetTickCount64() * 1000ull;
    const auto ticks = static_cast<std::uint64_t>(now.QuadPart);
    return (ticks / frequency) * 1000000ull +
        ((ticks % frequency) * 1000000ull) / frequency;
}

bool IsDeviceLossError(DWORD error) noexcept
{
    switch (error)
    {
    case ERROR_DEVICE_NOT_CONNECTED:
    case ERROR_INVALID_HANDLE:
    case ERROR_GEN_FAILURE:
    case ERROR_BAD_COMMAND:
    case ERROR_NO_SUCH_DEVICE:
    case ERROR_NOT_READY:
    case ERROR_OPERATION_ABORTED:
        return true;
    default:
        return false;
    }
}

void TraceReport(const wchar_t*, const std::uint8_t*, std::size_t) {}

bool TimedIo(HANDLE handle, bool write, void* data, DWORD bytes,
    DWORD timeout, DWORD* transferred)
{
    if (transferred) *transferred = 0;
    HidIoOperation operation(handle);
    DWORD error = 0;
    const auto start = write
        ? operation.StartWrite(data, bytes, &error)
        : operation.StartRead(data, bytes, &error);
    if (start == HidIoOperation::StartResult::Failed)
    {
        SetLastError(error);
        return false;
    }
    if (start == HidIoOperation::StartResult::Pending)
    {
        const DWORD wait = operation.Wait(timeout);
        if (wait == WAIT_OBJECT_0)
        {
            const bool ok = operation.Finish(transferred, &error, false);
            if (!ok) SetLastError(error);
            return ok;
        }
        const DWORD waitError = wait == WAIT_TIMEOUT ? WAIT_TIMEOUT : GetLastError();
        operation.CancelAndDrain(transferred, &error);
        SetLastError(waitError ? waitError : ERROR_GEN_FAILURE);
        return false;
    }
    const bool ok = operation.Finish(transferred, &error, false);
    if (!ok) SetLastError(error);
    return ok;
}

std::vector<Candidate> Enumerate(bool routedOnly, bool verbose)
{
    GUID guid{};
    HidD_GetHidGuid(&guid);
    HDEVINFO set = SetupDiGetClassDevsW(&guid, nullptr, nullptr,
        DIGCF_PRESENT | DIGCF_DEVICEINTERFACE);
    if (set == INVALID_HANDLE_VALUE) return {};

    std::vector<Candidate> result;
    std::vector<std::wstring> seen;
    for (DWORD index = 0;; ++index)
    {
        SP_DEVICE_INTERFACE_DATA interfaceData{};
        interfaceData.cbSize = sizeof(interfaceData);
        if (!SetupDiEnumDeviceInterfaces(set, nullptr, &guid, index,
                &interfaceData))
        {
            if (GetLastError() == ERROR_NO_MORE_ITEMS) break;
            continue;
        }
        DWORD needed = 0;
        SetupDiGetDeviceInterfaceDetailW(set, &interfaceData, nullptr, 0,
            &needed, nullptr);
        if (needed < sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA_W)) continue;
        std::vector<std::uint8_t> storage(needed);
        auto* detail = reinterpret_cast<SP_DEVICE_INTERFACE_DETAIL_DATA_W*>(
            storage.data());
        detail->cbSize = sizeof(*detail);
        if (!SetupDiGetDeviceInterfaceDetailW(set, &interfaceData, detail,
                needed, nullptr, nullptr))
            continue;

        std::wstring prefix(detail->DevicePath);
        const auto deviceSpecific = prefix.find(L'#', prefix.find(L'#') + 1);
        if (deviceSpecific != prefix.npos) prefix.resize(deviceSpecific);
        std::transform(prefix.begin(), prefix.end(), prefix.begin(), [](wchar_t c) {
            return static_cast<wchar_t>(std::towlower(c));
        });
        const auto pid = PathField(prefix, L"&pid_", 4, 0);
        if (PathField(prefix, L"#vid_", 4, 0) != kVendorId || !irok_hz94::WitmodProduct(pid)) continue;

        Candidate candidate{};
        candidate.path = detail->DevicePath;
        candidate.attributes.ProductID = static_cast<USHORT>(pid);
        candidate.interface = PathField(prefix, L"&mi_", 2, 0xff);
        candidate.collection = PathField(prefix, L"&col", 2, 0xff);
        seen.push_back(candidate.path);
        const bool routed = NativeAnalogRouting_IsClaimedBy(
            candidate.path.c_str(), NativeAnalogProtocol::IrokNa87M484);
        if (NativeAnalogRouting_IsClaimed(candidate.path.c_str()) && !routed) {
            WitmodEvidence(candidate, "route", false, "other_owner=1");
            continue;
        }
        if (routedOnly && !routed) continue;

        Handle metadata(CreateFileW(candidate.path.c_str(), 0,
            FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING,
            FILE_ATTRIBUTE_NORMAL, nullptr));
        if (!metadata) {
            WitmodEvidence(candidate, "metadata_open", false, "win32=%lu", GetLastError());
            continue;
        }
        HIDD_ATTRIBUTES attributes{};
        attributes.Size = sizeof(attributes);
        if (!HidD_GetAttributes(metadata.value, &attributes)) {
            WitmodEvidence(candidate, "attributes", false, "win32=%lu", GetLastError());
            continue;
        }
        if (attributes.VendorID != kVendorId || attributes.ProductID != pid) {
            WitmodEvidence(candidate, "attributes", false, "identity_changed=1");
            continue;
        }
        candidate.attributes = attributes;
        PHIDP_PREPARSED_DATA preparsed = nullptr;
        if (!HidD_GetPreparsedData(metadata.value, &preparsed)) {
            WitmodEvidence(candidate, "preparsed", false, "win32=%lu", GetLastError());
            continue;
        }
        const NTSTATUS status = HidP_GetCaps(preparsed, &candidate.caps);
        HidD_FreePreparsedData(preparsed);
        if (status != HIDP_STATUS_SUCCESS) {
            WitmodEvidence(candidate, "caps", false, "hid_status=%08lX", static_cast<unsigned long>(status));
            continue;
        }
        const bool protocolShape = candidate.caps.UsagePage == kUsagePage &&
            candidate.caps.Usage == kUsage &&
            candidate.caps.InputReportByteLength == irok_nd75::kReportBytes &&
            candidate.caps.OutputReportByteLength == irok_nd75::kReportBytes;
#if defined(HALLJOY_AJAZZ_DIAGNOSTIC)
        if (verbose) StabilityTrace_Write(L"INFO",L"ajazz",L"interface",
            L"vid=0416 pid=7372 usage_page=%u usage=%u input_bytes=%u output_bytes=%u protocol_shape=%u",
            candidate.caps.UsagePage,candidate.caps.Usage,
            candidate.caps.InputReportByteLength,candidate.caps.OutputReportByteLength,protocolShape?1u:0u);
#else
        (void)verbose;
#endif
        WitmodEvidence(candidate, "caps", false,
            "page=%04X usage=%04X in=%u out=%u feature=%u bcd=%04X eligible=%u",
            unsigned(candidate.caps.UsagePage), unsigned(candidate.caps.Usage),
            unsigned(candidate.caps.InputReportByteLength), unsigned(candidate.caps.OutputReportByteLength),
            unsigned(candidate.caps.FeatureReportByteLength), unsigned(attributes.VersionNumber), unsigned(protocolShape));
        if (protocolShape && std::find(g_foreignIdentity.begin(), g_foreignIdentity.end(), candidate.path)
                == g_foreignIdentity.end()) result.push_back(std::move(candidate));
    }
    SetupDiDestroyDeviceInfoList(set);
    std::erase_if(g_foreignIdentity, [&](const auto& path) {
        return std::find(seen.begin(), seen.end(), path) == seen.end();
    });
    return result;
}

class Session
{
public:
    Session(const Candidate& candidate, bool exclusive, bool control)
        : candidate_(candidate), exclusive_(exclusive), control_(control) {}
    ~Session() { reader_.reset(); ReleaseActive(); }

    bool Open()
    {
        const DWORD share = exclusive_ ? 0 : FILE_SHARE_READ | FILE_SHARE_WRITE;
        handle_ = Handle(CreateFileW(candidate_.path.c_str(),
            GENERIC_READ | GENERIC_WRITE, share, nullptr, OPEN_EXISTING,
            FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OVERLAPPED, nullptr));
        if (!handle_) {
            const DWORD error = GetLastError();
            WitmodEvidence(candidate_, "open", false, "share=%lu win32=%lu", share, error);
            SetLastError(error);
            return false;
        }
        WitmodEvidence(candidate_, "open", false, "share=%lu win32=0", share);
        HidD_SetNumInputBuffers(handle_.value, 256);
        reader_ = std::make_unique<HidIoOperation>(handle_.value);
        if (!reader_->IsValid()) return false;
        RegisterActive();
        return true;
    }

    bool Send(const irok_nd75::Report& report)
    {
        TraceReport(L"TX", report.data(), report.size());
        auto copy = report;
        bool ok = false;
        if (control_)
            ok = HidD_SetOutputReport(handle_.value, copy.data(),
                static_cast<ULONG>(copy.size())) != FALSE;
        else
        {
            DWORD sent = 0;
            ok = TimedIo(handle_.value, true, copy.data(),
                static_cast<DWORD>(copy.size()), kIoTimeoutMs, &sent) &&
                sent == copy.size();
        }
        if (!ok) g_failures.fetch_add(1, std::memory_order_relaxed);
        return ok;
    }

    bool Read(irok_nd75::Report* out, DWORD timeout)
    {
        if (!out || !reader_) { SetLastError(ERROR_INVALID_PARAMETER); return false; }
        DWORD error=0, received=0;
        if (!readPending_) {
            const auto started=reader_->StartRead(readBuffer_.data(),64,&error);
            if (started==HidIoOperation::StartResult::Failed) { SetLastError(error); return false; }
            readPending_=true;
        }
        const DWORD wait=reader_->Wait(timeout);
        if (wait==WAIT_TIMEOUT) { SetLastError(WAIT_TIMEOUT); return false; }
        if (wait!=WAIT_OBJECT_0) {
            error=GetLastError(); reader_->CancelAndDrain(nullptr,nullptr);
            readPending_=false; SetLastError(error); return false;
        }
        const bool ok=reader_->Finish(&received,&error,false);
        readPending_=false;
        if (!ok || received!=64) { SetLastError(ok?ERROR_BAD_LENGTH:error); return false; }
        *out=readBuffer_;
        return true;
    }

    HANDLE Native() const { return handle_.value; }
    const Candidate& Device() const { return candidate_; }

private:
    void RegisterActive()
    {
        std::lock_guard<std::mutex> lock(g_handleMutex);
        g_activeHandle = handle_.value;
        active_ = true;
    }

    void ReleaseActive()
    {
        if (!active_) return;
        std::lock_guard<std::mutex> lock(g_handleMutex);
        if (g_activeHandle == handle_.value)
            g_activeHandle = INVALID_HANDLE_VALUE;
        active_ = false;
    }

    Candidate candidate_;
    bool exclusive_ = false;
    bool control_ = false;
    bool active_ = false;
    Handle handle_{};
    irok_nd75::Report readBuffer_{};
    std::unique_ptr<HidIoOperation> reader_;
    bool readPending_ = false;
};

bool IsAjazzRgb(const irok_nd75::DeviceInfo& identity)
{
    // Firmware identity strings are space-padded on the tester's device.
    std::size_t length=std::strlen(identity.firmware.data());
    while(length && identity.firmware[length-1]==' ') --length;
    // Exact hardware/firmware with the tester-verified sensor map and raw scale.
    return std::strcmp(identity.controller.data(), "M484") == 0 &&
        std::strcmp(identity.product.data(), "SG8994HERGB") == 0 &&
        length==8 && std::memcmp(identity.firmware.data(), "V1.13.17",8) == 0;
}

bool ReceiveIdentity(Session& session, irok_nd75::DeviceInfo* out)
{
    if (g_stop.load(std::memory_order_acquire)) return false;
    const auto& device = session.Device();
    if (!session.Send(irok_nd75::BuildIdentityRequest())) {
        WitmodEvidence(device, "identity_write", false, "opcode=0D win32=%lu", GetLastError());
        return false;
    }
    const auto deadline = GetTickCount64() + kProofTimeoutMs;
    irok_nd75::Report report{};
    unsigned reports = 0;
    DWORD error = WAIT_TIMEOUT;
    while (!g_stop.load(std::memory_order_acquire) && GetTickCount64() < deadline) {
        if (!session.Read(&report, 100)) {
            error = GetLastError();
            if (IsDeviceLossError(error)) break;
            continue;
        }
        ++reports;
        irok_nd75::DeviceInfo parsed{};
        if (!irok_nd75::DecodeDeviceInfo(report.data(), report.size(), &parsed)) {
            if (reports <= 4) {
                WitmodEvidence(device, "identity_reply", false,
                    "id=%02X opcode=%02X decoded=0", unsigned(report[0]), unsigned(report[1]));
                if (report[0] == 1 && report[1] == 0x0d)
                    WitmodEvidence(device, "identity_refused", false, "status=%u size_field=%u",
                        unsigned(report[2]), unsigned(report[5]));
            }
            continue;
        }
        *out = parsed;
        const bool hz94 = irok_hz94::Matches(*out);
        const bool accepted = hz94 || (device.attributes.ProductID == kProductId &&
            (irok_na87::IsExpectedDevice(*out) || IsAjazzRgb(*out)
#if defined(HALLJOY_AJAZZ_DIAGNOSTIC)
            || (std::strcmp(out->controller.data(), "M484") == 0 &&
                (std::strcmp(out->product.data(), "SG8994HERGB") == 0 ||
                 std::strcmp(out->product.data(), "SG8994HE") == 0))
#endif
            ));
        WitmodEvidence(device, "identity", false, "controller=%s product=%s firmware=%s hz94=%u accepted=%u",
            Identifier(out->controller.data()).c_str(), Identifier(out->product.data()).c_str(),
            Identifier(out->firmware.data()).c_str(), unsigned(hz94), unsigned(accepted));
        return accepted;
    }
    if (!g_stop.load(std::memory_order_acquire))
        WitmodEvidence(device, "identity_timeout", false, "opcode=0D reports=%u win32=%lu", reports, error);
    return false;
}

bool ReceiveCapability(Session& session, irok_nd75::CapabilityInfo* out)
{
    if (g_stop.load(std::memory_order_acquire)) return false;
    const auto& device = session.Device();
    if (!session.Send(irok_nd75::BuildCapabilityRequest())) {
        WitmodEvidence(device, "capability_write", false, "opcode=21 sub=04 win32=%lu", GetLastError());
        return false;
    }
    const auto deadline = GetTickCount64() + kProofTimeoutMs;
    irok_nd75::Report report{};
    unsigned replies = 0;
    while (!g_stop.load(std::memory_order_acquire) && GetTickCount64() < deadline) {
        if (!session.Read(&report, 100)) continue;
        if (irok_nd75::DecodeCapabilityInfo(report.data(), report.size(), out)) {
            WitmodEvidence(device, "capability", false, "value=%u decoded=1", unsigned(out->sensitivity));
            return true;
        }
        if (report[0] == 1 && report[1] == 0x21 && report[6] == 0x04)
            WitmodEvidence(device, "capability", false, "value=%u decoded=0", unsigned(report[7]));
        ++replies;
    }
    if (!g_stop.load(std::memory_order_acquire))
        WitmodEvidence(device, "capability_timeout", false, "opcode=21 sub=04 replies=%u", replies);
    return false;
}

bool ReceiveMap(Session& session, irok_nd75::PositionToHid* map,
    bool allowDuplicateHids = false, bool hz94 = false)
{
    const auto& device = session.Device();
    if (!session.Send(irok_na87::MapRequest())) {
        WitmodEvidence(device, "map_write", false, "opcode=10 win32=%lu", GetLastError());
        return false;
    }
    irok_na87::MapReader reader(allowDuplicateHids);
    const auto deadline = GetTickCount64() + kProofTimeoutMs;
    while (!g_stop.load() && GetTickCount64() < deadline) {
        irok_nd75::Report report{};
        if (session.Read(&report, 100)) reader.Feed(report);
        if (reader.Complete()) {
            const auto geometry = irok_hz94::InspectMap(reader.Map());
            WitmodEvidence(device, "map", false,
                "rows=%02X assigned=%u hz94=%u factory_match=%u outside=%u",
                reader.RowMask(), geometry.assigned, unsigned(hz94), geometry.factory, geometry.outside);
            if (hz94 && !geometry.Admitted()) return false;
            *map = reader.Map();
            return true;
        }
        if (reader.Conflict()) break;
    }
    if (!g_stop.load())
        WitmodEvidence(device, "map_refused", false, "rows=%02X conflict=%u assignments=%zu",
            reader.RowMask(), unsigned(reader.Conflict()), irok_nd75::MappedKeyCount(reader.Map()));
    return false;
}

bool Prove(const Candidate& candidate, Proof* out)
{
    Session session(candidate, false, false);
    Proof proof{};
    if (g_stop.load() || !session.Open()) return false;
    if (!ReceiveIdentity(session, &proof.identity)) {
        if (proof.identity.product[0] && !g_stop.load())
            g_foreignIdentity.push_back(candidate.path);
        return false;
    }
    const bool hz94 = irok_hz94::Matches(proof.identity);
    if (!ReceiveCapability(session, &proof.capability) || proof.capability.sensitivity != 40 ||
        !ReceiveMap(session, &proof.map, !irok_na87::IsExpectedDevice(proof.identity), hz94)) return false;
    DebugLog_Write(L"[witmod.proof] pass identity_validated=1 mapped_positions=%llu scale=0..40",
        static_cast<unsigned long long>(irok_nd75::MappedKeyCount(proof.map)));
    if (out) *out = proof;
    return true;
}

bool EnsureClaim(const Candidate& candidate, Proof* proof)
{
    if (NativeAnalogRouting_IsClaimedBy(candidate.path.c_str(),
            NativeAnalogProtocol::IrokNa87M484))
        return Prove(candidate, proof);
    Proof validated{};
    if (!Prove(candidate, &validated)) return false;
    if (!NativeAnalogRouting_Claim(candidate.attributes.VendorID,
            candidate.attributes.ProductID, candidate.path.c_str(),
            NativeAnalogProtocol::IrokNa87M484))
        return false;
    if (proof) *proof = validated;
    return true;
}

void Clear()
{
    halljoy::native_layout::Clear(StreamLayout());
    g_hz94Pid.store(0, std::memory_order_release);
    const bool assignedChanged=g_assignedValues.Clear();
    bool changed=g_factoryValues.Clear() || assignedChanged;
    for (auto& value:g_positionMilli) value.store(0);
    for (auto& value : g_milli)
        if (value.exchange(0, std::memory_order_relaxed)) changed = true;
    for (auto& owned : g_owned) owned.store(0, std::memory_order_relaxed);
    g_active.store(0, std::memory_order_relaxed);
    if (changed) RealtimeLoop_NotifyInputChanged();
}

bool PublishOwnership(const irok_nd75::PositionToHid& map)
{
    const bool hz94 = g_hz94Pid.load() != 0;
    constexpr auto na87Factory = irok_na87::FactoryMap();
    const auto& factory = hz94 ? irok_hz94::kFactoryMap : na87Factory;
    std::array<halljoy::native_layout::Key, irok_nd75::kMatrixSlots> keys{};
    std::size_t count = 0;
    for (std::size_t pos = 0; pos < map.size(); ++pos) {
        const auto id = static_cast<std::uint8_t>(pos + 1);
        const std::uint16_t physical = hz94 ? irok_hz94::HostCode(factory[pos]) : factory[pos];
        const std::uint16_t assigned = hz94 ? irok_hz94::HostCode(map[pos]) : map[pos];
        g_factoryValues.Bind(id, physical);
        g_assignedValues.Bind(id, assigned);
        if (map[pos]) g_owned[map[pos]].store(1, std::memory_order_relaxed);
        if (physical) keys[count++] = {physical, assigned};
    }
    return halljoy::native_layout::Publish(StreamLayout(), keys.data(), count);
}

void Publish(const irok_nd75::LiveEvent& event, const Proof& proof,
    LONGLONG qpc)
{
    const std::size_t position = std::size_t(event.row) *
        irok_nd75::kColumns + event.column;
    if (position >= proof.map.size()) return;
    const std::uint8_t hid = proof.map[position];
    std::uint16_t milli = 0;
    if (!irok_nd75::TryTravelToMilli(event.travel, &milli))
    {
        g_outOfRange.fetch_add(1, std::memory_order_relaxed);
        return;
    }
    const auto stamp=GetTickCount64();
    const auto id=static_cast<std::uint8_t>(position+1);
    g_factoryValues.Publish(id,milli,stamp);g_assignedValues.Publish(id,milli,stamp);
    if (hid) g_milli[hid].store(g_assignedValues.Read(hid,stamp,~std::uint64_t{0}).milli);
    const auto old=g_positionMilli[position].exchange(milli,std::memory_order_relaxed);
    if ((old == 0) != (milli == 0))
    {
        if (milli) g_active.fetch_add(1, std::memory_order_relaxed);
        else g_active.fetch_sub(1, std::memory_order_relaxed);
    }
    g_updates.fetch_add(1, std::memory_order_relaxed);
    g_lastMs.store(GetTickCount64(), std::memory_order_relaxed);
    if (old != milli) RealtimeLoop_NotifyInputChangedAt(qpc);
}

bool Run(const Candidate& candidate, Proof proof)
{
    const bool hz94 = irok_hz94::Matches(proof.identity);
    Session session(candidate, proof.exclusive, proof.useControlWrite);
    if (!session.Open()) return false;
    irok_nd75::DeviceInfo sessionIdentity{};
    irok_nd75::CapabilityInfo sessionCapability{};
    if (!ReceiveIdentity(session, &sessionIdentity) ||
        std::strcmp(sessionIdentity.controller.data(),
            proof.identity.controller.data()) != 0 ||
        std::strcmp(sessionIdentity.product.data(), proof.identity.product.data()) != 0 ||
        !ReceiveCapability(session, &sessionCapability) ||
        sessionCapability.sensitivity != 40 || !ReceiveMap(session, &proof.map, hz94, hz94))
    {
        DebugLog_Write(L"[irok.na87.session] capability_changed_after_claim=1");
        return false;
    }
    proof.identity = sessionIdentity;
    proof.capability = sessionCapability;
    g_inputBytes.store(candidate.caps.InputReportByteLength);
    g_outputBytes.store(candidate.caps.OutputReportByteLength);
    Clear();
    if (hz94) g_hz94Pid.store(candidate.attributes.ProductID, std::memory_order_release);
    if (!PublishOwnership(proof.map) && hz94) {
        WitmodEvidence(candidate, "layout_refused", false, "token_publish=0");
        Clear();
        return false;
    }
    g_mapped.store(static_cast<unsigned>(irok_nd75::MappedKeyCount(proof.map)));
    auto mask = irok_nd75::SubscriptionMask(proof.map);
    const auto factoryMask = irok_nd75::SubscriptionMask(
        hz94 ? irok_hz94::kFactoryMap : irok_na87::FactoryMap());
    for (std::size_t col=0;col<mask.size();++col) mask[col]|=factoryMask[col];
    if (!session.Send(irok_nd75::BuildUnsubscribeRequest()) ||
        !session.Send(irok_nd75::BuildSubscriptionRequest(mask)))
    {
        if (hz94) {
            WitmodEvidence(candidate, "subscribe_failed", false, "opcode=21 win32=%lu", GetLastError());
            session.Send(irok_nd75::BuildUnsubscribeRequest());
        }
        Clear();
        return false;
    }
    if (hz94) WitmodEvidence(candidate, "subscribe", false, "opcode=21 sub=02 channels=94 waiting_for_travel=1");
    g_diagnosticStatus.store(0);
    g_connected.store(!hz94, std::memory_order_release);
    DebugLog_Write(L"[witmod.session] subscribed vid=0416 pid=7372 controller=%hs product=%hs firmware=%hs mapped_positions=dynamic nominal_max=40 sensitivity=%u mode=%ls exclusive=%d strategy=live_subscription_only",
        proof.identity.controller.data(), proof.identity.product.data(),
        proof.identity.firmware.data(), proof.capability.sensitivity,
        proof.useControlWrite ? L"control" : L"write",
        proof.exclusive ? 1 : 0);

    std::uint64_t previousUs = 0, intervalSum = 0, intervalCount = 0;
    std::uint64_t maxInterval = 0, lastRollup = GetTickCount64();
    std::uint64_t lastRollupLive = g_liveEvents.load(std::memory_order_relaxed);
    std::uint64_t lastProofMs=GetTickCount64(), lastProbeMs=lastProofMs;
    std::uint32_t consecutiveReadErrors = 0;
    bool transportLost = false;
    const auto streamStart = GetTickCount64();
    auto nextEvidence = streamStart + 5000;
    std::uint64_t samples = 0, positive = 0, releases = 0, malformed = 0;
    unsigned depthMax = 0, peak = 0, otherReports = 0;
    const auto streamEvidence = [&](const char* stage) {
        WitmodEvidence(candidate, stage, true,
            "elapsed_ms=%llu samples=%llu positive=%llu releases=%llu malformed=%llu max_depth=%u peak_active=%u",
            GetTickCount64() - streamStart, static_cast<unsigned long long>(samples),
            static_cast<unsigned long long>(positive), static_cast<unsigned long long>(releases),
            static_cast<unsigned long long>(malformed), depthMax, peak);
    };
#if defined(HALLJOY_DIAGNOSTIC) || defined(HALLJOY_DEVICE_SUPPORT_LOG)
    std::array<std::uint64_t, 256> diagnosticEvents{};
    std::array<std::uint64_t, 256> diagnosticReleases{};
    std::array<bool, 256> diagnosticPressed{};
    std::array<std::uint8_t, 256> diagnosticMin{};
    std::array<std::uint8_t, 256> diagnosticMax{};
    diagnosticMin.fill(0xff);
    std::uint64_t diagnosticPositiveEdges = 0, diagnosticZeroEdges = 0;
    std::uint32_t diagnosticUnique = 0, diagnosticPeakActive = 0;
    const auto diagnosticStartedMs = GetTickCount64();
    const auto diagnosticStartedLive = g_liveEvents.load(std::memory_order_relaxed);
    const auto diagnosticStartedOutOfRange = g_outOfRange.load(std::memory_order_relaxed);
#endif

    while (!g_stop.load(std::memory_order_acquire))
    {
        irok_nd75::Report report{};
        const bool received = session.Read(&report, 100);
        if (!received)
        {
            const DWORD readError = GetLastError();
            const bool expectedStopCancellation =
                g_stop.load(std::memory_order_acquire) &&
                (readError == ERROR_OPERATION_ABORTED ||
                    readError == ERROR_INVALID_HANDLE);
            if (expectedStopCancellation)
            {
                StabilityTrace_Write(L"INFO", L"irok-na87",
                    L"protocol.cancelled", L"operation=read win32=%lu reason=stop",
                    readError);
                break;
            }
            const bool timedOut = readError == WAIT_TIMEOUT;
            if (timedOut)
            {
                consecutiveReadErrors = 0;
            }
            else
            {
                ++consecutiveReadErrors;
                g_failures.fetch_add(1, std::memory_order_relaxed);
                const bool deviceLost = IsDeviceLossError(readError);
                if (hz94) WitmodEvidence(candidate, "travel_read", false, "win32=%lu", readError);
                DebugLog_WriteBuffered(L"[irok.na87.transport] read_failed win32=%lu consecutive=%u device_lost=%d",
                    readError, consecutiveReadErrors, deviceLost ? 1 : 0);
                if (irok_nd75::ClassifyReadFailure(
                        g_stop.load(std::memory_order_acquire), false,
                        deviceLost, consecutiveReadErrors) ==
                    irok_nd75::ReadFailureAction::EndSession)
                {
                    transportLost = true;
                    break;
                }
                WaitForSingleObject(g_wake, 10);
            }
        }
        else
        {
            consecutiveReadErrors = 0;
        }
        irok_nd75::LiveEvent event{};
        bool live = received && report[2] == 0 && report[3] == 0 && report[4] == 0 &&
            report[5] == 3 && irok_nd75::DecodeLiveEvent(report.data(), report.size(), &event);
        if (hz94 && received && report[0] == 1 && report[1] == 0x21 && report[6] == 1) {
            const auto pos = std::size_t(event.row) * irok_nd75::kColumns + event.column;
            const bool mapped = live && irok_hz94::kFactoryMap[pos];
            const bool inRange = live && event.travel <= irok_nd75::kNominalTravelMaximum;
            if (!(mapped && inRange)) {
                if (++malformed == 1)
                    WitmodEvidence(candidate, "travel_refused", false,
                        "report_id=01 opcode=21 size_field=%u envelope=%u mapped=%u range_ok=%u",
                        unsigned(report[5]), unsigned(live), unsigned(mapped), unsigned(inRange));
                live = false;
            }
        } else if (hz94 && received && report[1] != 0x0d && otherReports < 4) {
            ++otherReports;
            WitmodEvidence(candidate, "travel_other", false, "id=%02X opcode=%02X",
                unsigned(report[0]), unsigned(report[1]));
        }
        if (live)
        {
            const auto nowUs = NowUs();
            if (previousUs && nowUs > previousUs)
            {
                const auto interval = nowUs - previousUs;
                intervalSum += interval;
                ++intervalCount;
                maxInterval = std::max(maxInterval, interval);
            }
            previousUs = nowUs;
            LARGE_INTEGER qpc{};
            QueryPerformanceCounter(&qpc);
#if defined(HALLJOY_DIAGNOSTIC) || defined(HALLJOY_DEVICE_SUPPORT_LOG)
            const std::size_t position = std::size_t(event.row) *
                irok_nd75::kColumns + event.column;
            const std::uint8_t diagnosticHid = proof.map[position];
            const std::uint16_t diagnosticOld = diagnosticHid
                ? g_milli[diagnosticHid].load(std::memory_order_relaxed) : 0;
#endif
            bool firstRelease = false;
            if (hz94) {
                const auto pos = std::size_t(event.row) * irok_nd75::kColumns + event.column;
                ++samples;
                positive += event.travel != 0;
                if (event.travel == 0 && g_positionMilli[pos].load() != 0)
                    firstRelease = ++releases == 1;
                depthMax = std::max(depthMax, unsigned(event.travel));
            }
            Publish(event, proof, qpc.QuadPart);
            if (hz94) {
                peak = std::max(peak, unsigned(g_active.load()));
                if (event.travel && !g_connected.exchange(true, std::memory_order_acq_rel)) {
                    streamEvidence("travel_started");
                    RealtimeLoop_NotifyInputChanged();
                }
                if (firstRelease) streamEvidence("release_seen");
            }
            g_liveEvents.fetch_add(1, std::memory_order_relaxed);
#if defined(HALLJOY_DIAGNOSTIC) || defined(HALLJOY_DEVICE_SUPPORT_LOG)
            if (diagnosticHid)
            {
                ++diagnosticEvents[diagnosticHid];
                std::uint16_t diagnosticMilli = 0;
                const bool validTravel = irok_nd75::TryTravelToMilli(
                    event.travel, &diagnosticMilli);
                if (validTravel && event.travel && !diagnosticPressed[diagnosticHid])
                {
                    diagnosticPressed[diagnosticHid] = true;
                    ++diagnosticUnique;
                }
                diagnosticMin[diagnosticHid] = std::min(
                    diagnosticMin[diagnosticHid], event.travel);
                diagnosticMax[diagnosticHid] = std::max(
                    diagnosticMax[diagnosticHid], event.travel);
                if (validTravel && diagnosticOld == 0 && diagnosticMilli != 0)
                    ++diagnosticPositiveEdges;
                if (validTravel && diagnosticOld != 0 && diagnosticMilli == 0)
                {
                    ++diagnosticZeroEdges;
                    ++diagnosticReleases[diagnosticHid];
                }
                diagnosticPeakActive = std::max(diagnosticPeakActive,
                    g_active.load(std::memory_order_relaxed));
            }
#endif
        }

        const auto nowMs = GetTickCount64();
        irok_nd75::DeviceInfo heartbeat{};
        if (received && irok_nd75::DecodeDeviceInfo(report.data(), report.size(), &heartbeat) &&
            (hz94 ? irok_hz94::Matches(heartbeat) : irok_na87::IsExpectedDevice(heartbeat))) lastProofMs = nowMs;
        if (hz94 && nowMs >= nextEvidence) {
            streamEvidence("travel");
            nextEvidence = nowMs + 30000;
        }
        if (nowMs-lastProbeMs>=1000) {
            if (!session.Send(irok_nd75::BuildIdentityRequest())) {
                if (hz94) WitmodEvidence(candidate, "heartbeat_write", false, "win32=%lu", GetLastError());
                transportLost=true;
                break;
            }
            lastProbeMs=nowMs;
        }
        if (nowMs-lastProofMs>=3000) {
            if (hz94) WitmodEvidence(candidate, "heartbeat_timeout", false, "elapsed_ms=%llu", nowMs-lastProofMs);
            DebugLog_Write(L"[irok.na87.transport] heartbeat_timeout neutralized=1 reconnect_pending=1");
            transportLost=true; break;
        }
        if (nowMs - lastRollup >= 1000)
        {
            const auto liveEventCount = g_liveEvents.load(std::memory_order_relaxed);
            const auto delta = liveEventCount - lastRollupLive;
            g_hz10.store(static_cast<std::uint32_t>(delta * 10000ull /
                std::max<ULONGLONG>(1, nowMs - lastRollup)));
            if (intervalCount)
            {
                g_avgUs.store(static_cast<std::uint32_t>(
                    intervalSum / intervalCount));
                g_maxUs.store(static_cast<std::uint32_t>(
                    std::min<std::uint64_t>(maxInterval, 0xffffffffull)));
            }
            DebugLog_WriteBuffered(L"[irok.na87.telemetry] live_event_hz=%.1f live_events=%llu publications=%llu active=%u mapped_positions=dynamic avg_event_interval_us=%u max_event_interval_us=%u out_of_range=%llu failures=%llu last_age_ms=%llu",
                g_hz10.load() / 10.0, static_cast<unsigned long long>(liveEventCount),
                static_cast<unsigned long long>(g_updates.load()), g_active.load(),
                g_avgUs.load(), g_maxUs.load(),
                static_cast<unsigned long long>(g_outOfRange.load()),
                static_cast<unsigned long long>(g_failures.load()),
                static_cast<unsigned long long>(g_lastMs.load()
                    ? nowMs - g_lastMs.load() : 0));
#if defined(HALLJOY_DIAGNOSTIC) || defined(HALLJOY_DEVICE_SUPPORT_LOG)
            unsigned releasedKeys=0;
            for (auto count:diagnosticReleases) if (count) ++releasedKeys;
            const bool enough=nowMs-diagnosticStartedMs>=30000 && diagnosticUnique>=8 &&
                releasedKeys>=8 && diagnosticPeakActive>=2;
            if (enough) g_diagnosticStatus.store(1);
            else if (nowMs-diagnosticStartedMs>=60000 && g_diagnosticStatus.load()!=1)
                g_diagnosticStatus.store(2);
            DebugLog_WriteBuffered(L"[irok.na87.diagnostic] elapsed_ms=%llu unique_pressed_keys=%u peak_simultaneous=%u positive_edges=%llu release_to_zero_edges=%llu out_of_range=%llu active_now=%u",
                static_cast<unsigned long long>(nowMs - diagnosticStartedMs),
                diagnosticUnique, diagnosticPeakActive,
                static_cast<unsigned long long>(diagnosticPositiveEdges),
                static_cast<unsigned long long>(diagnosticZeroEdges),
                static_cast<unsigned long long>(g_outOfRange.load() -
                    diagnosticStartedOutOfRange), g_active.load());
#endif
            lastRollup = nowMs;
            lastRollupLive = liveEventCount;
        }
    }

    if (transportLost)
    {
        g_connected.store(false, std::memory_order_release);
        Clear();
        StabilityTrace_Write(L"WARN", L"irok-na87",
            L"transport.disconnected",
            L"neutralized=1 reconnect_pending=1 failures=%llu",
            static_cast<unsigned long long>(g_failures.load()));
    }

#if defined(HALLJOY_DIAGNOSTIC) || defined(HALLJOY_DEVICE_SUPPORT_LOG)
    DebugLog_Write(L"[irok.na87.session_summary] duration_ms=%llu unique_pressed_keys=%u peak_simultaneous=%u positive_edges=%llu release_to_zero_edges=%llu out_of_range=%llu active_at_stop=%u session_live_events=%llu failures=%llu",
        static_cast<unsigned long long>(GetTickCount64() - diagnosticStartedMs),
        diagnosticUnique, diagnosticPeakActive,
        static_cast<unsigned long long>(diagnosticPositiveEdges),
        static_cast<unsigned long long>(diagnosticZeroEdges),
        static_cast<unsigned long long>(g_outOfRange.load() -
            diagnosticStartedOutOfRange), g_active.load(),
        static_cast<unsigned long long>(g_liveEvents.load() -
            diagnosticStartedLive),
        static_cast<unsigned long long>(g_failures.load()));
#endif
    if (hz94) {
        g_connected.store(false, std::memory_order_release);
        streamEvidence("travel_end");
        const bool stopped = session.Send(irok_nd75::BuildUnsubscribeRequest());
        WitmodEvidence(candidate, "unsubscribe", false, "sent=%u stop=%u transport_lost=%u win32=%lu",
            unsigned(stopped), unsigned(g_stop.load()), unsigned(transportLost), stopped ? 0ul : GetLastError());
    } else if (!transportLost) {
        session.Send(irok_nd75::BuildUnsubscribeRequest());
    }
    return true;
}

// The AJAZZ candidate uses the same validated M484 event framing, but never
// publishes the NA87 layout identity or factory matrix.
Proof g_ajazzProof;
std::unique_ptr<halljoy::ajazz::DynamicLimits> g_ajazzLimits;
void BindAjazzMap(const Proof& proof)
{
    Clear();
    g_ajazzProof=proof;
    for (std::size_t pos=0;pos<proof.map.size();++pos) {
        if (!halljoy::ajazz::Physical(unsigned(pos))) continue;
        const auto hostCode=static_cast<std::uint16_t>(halljoy::ajazz::HostCode(proof.map[pos]));
        const auto id=static_cast<std::uint8_t>(pos+1);
        g_factoryValues.Bind(id,hostCode);
        g_assignedValues.Bind(id,hostCode);
        if (proof.map[pos]) g_owned[proof.map[pos]].store(1);
#if defined(HALLJOY_AJAZZ_DIAGNOSTIC)
        StabilityTrace_Write(L"INFO",L"ajazz",L"map.position",L"row=%u col=%u hid=%u",
            unsigned(pos/22),unsigned(pos%22),unsigned(proof.map[pos]));
#endif
    }
    g_mapped.store(82);
    g_inputBytes=64;g_outputBytes=64;
    g_ajazzPlaying=true;g_present=true;
}
#if defined(HALLJOY_AJAZZ_DIAGNOSTIC)
bool BeginAjazzPlay(Session& session)
{
    Proof proof{};
    if (!ReceiveMap(session, &proof.map, true) || !irok_nd75::MappedKeyCount(proof.map)) {
        StabilityTrace_Write(L"WARN",L"ajazz",L"map.failed",L"analog_not_published=1");
        return false;
    }
    // Preliminary per-key dynamic bounds; assume released at startup.
    for (std::size_t pos=0;pos<proof.map.size();++pos)
        StabilityTrace_Write(L"INFO",L"ajazz",L"map.assignment",
            L"row=%u col=%u key_code=%u",unsigned(pos/22),unsigned(pos%22),unsigned(proof.map[pos]));
    BindAjazzMap(proof);
    g_ajazzLimits=std::make_unique<halljoy::ajazz::DynamicLimits>();
    StabilityTrace_Write(L"INFO",L"ajazz",L"dynamic_limits.begin",L"device_map=1 preliminary=1 initial_zero=first_sample initial_full=log25 per_key=1 manual_calibration=0");
    return true;
}
void PublishAjazzPlay(const irok_nd75::LiveEvent& event)
{
    LARGE_INTEGER stamp{};QueryPerformanceCounter(&stamp);
    Publish(event,g_ajazzProof,stamp.QuadPart);
}
void ObserveAjazzDepth(const irok_nd75::LiveEvent& event)
{
    if(g_ajazzLimits) g_ajazzLimits->Event(unsigned(event.row)*22+event.column,event.travel,NowUs());
}
#endif
bool IsAjazzRawRow(const irok_nd75::Report& report)
{
    return report[0]==1 && report[1]==0x23 && report[2]==0 &&
        report[3]==0 && report[4]>=1 && report[4]<=6 && report[5]==44;
}
void PublishAjazzRawRow(const irok_nd75::Report& report)
{
    if(!g_ajazzLimits || !IsAjazzRawRow(report)) return;
    const unsigned row=report[4]-1;
    const auto us=NowUs(),ms=GetTickCount64();
    bool changed=false;
    for(unsigned col=0;col<22;++col) {
        const unsigned pos=row*22+col;
        if(!halljoy::ajazz::Physical(pos)) continue;
        const unsigned raw=(unsigned(report[6+col*2])<<8)|report[7+col*2];
        unsigned milli=0;g_ajazzLimits->Raw(pos,raw,us,milli);
        const auto id=static_cast<std::uint8_t>(pos+1);
        changed=g_factoryValues.Publish(id,static_cast<std::uint16_t>(milli),ms)||changed;
        g_assignedValues.Publish(id,static_cast<std::uint16_t>(milli),ms);
        const auto old=g_positionMilli[pos].exchange(static_cast<std::uint16_t>(milli));
        if((old==0)!=(milli==0)) {if(milli)++g_active;else --g_active;}
    }
    ++g_updates;g_lastMs=ms;
    if(changed) RealtimeLoop_NotifyInputChanged();
}
#if defined(HALLJOY_AJAZZ_DIAGNOSTIC)
unsigned AjazzReadyKeys() {return g_ajazzLimits?g_ajazzLimits->Ready():0;}
void LogAjazzLearning()
{
    if(!g_ajazzLimits) return;
    for(unsigned p=0;p<132;++p) {
        const auto s=g_ajazzLimits->Inspect(p);
        if(s.samples) StabilityTrace_Write(L"INFO",L"ajazz",L"dynamic_limits.key",
            L"row=%u col=%u ready=%u samples=%u expansions=%u rest_adc=%u full_adc=%u",
            p/22,p%22,s.ready?1u:0u,s.samples,s.expansions,s.rest,s.full);
    }
}

#endif
void EndAjazzPlay()
{
    g_connected=false;g_ajazzPlaying=false;g_ajazzLimits.reset();Clear();
}
#if defined(HALLJOY_AJAZZ_DIAGNOSTIC)
#include "ajazz_diagnostic.inl"
#endif


// Production path: no diagnostic depth subscription, raw dumps or forced log.
bool RunAjazz(const Candidate& candidate)
{
    Session session(candidate, false, false);
    Proof proof{};
    if (!session.Open() || !ReceiveIdentity(session, &proof.identity) ||
        !IsAjazzRgb(proof.identity) || !ReceiveCapability(session, &proof.capability) ||
        proof.capability.sensitivity!=40 || !ReceiveMap(session, &proof.map, true))
        return false;
    unsigned mapped=0;
    for(unsigned p=0;p<132;++p)
        if(halljoy::ajazz::Physical(p) && proof.map[p]) ++mapped;
    if(mapped==0) return false; // Disabled/remapped keys need not have an assignment.
    BindAjazzMap(proof);
    g_connected=false; // Announce only after every sensor row is live.
    g_ajazzLimits=std::make_unique<halljoy::ajazz::DynamicLimits>();
    struct Cleanup {
        Session& session;
        ~Cleanup() {
            EndAjazzPlay();
            irok_nd75::Report off{};off[0]=1;off[1]=0x23;
            session.Send(off);
        }
    } cleanup{session};
    irok_nd75::Report request{};request[0]=1;request[1]=0x23;request[6]=1;
    if(!session.Send(request)) return false;
    std::array<ULONGLONG,6> seen{};
    const auto started=GetTickCount64();
    while(!g_stop.load(std::memory_order_acquire)) {
        irok_nd75::Report report{};
        if(session.Read(&report,20)) {
            if(IsAjazzRawRow(report)) {
                PublishAjazzRawRow(report);
                seen[report[4]-1]=GetTickCount64();
                if(!g_connected.load() && std::all_of(seen.begin(),seen.end(),
                    [](ULONGLONG stamp){return stamp!=0;})) {
                    g_connected=true;
                    RealtimeLoop_NotifyInputChanged();
                }
            }
        } else {
            const auto error=GetLastError();
            if(error!=WAIT_TIMEOUT && error!=ERROR_TIMEOUT) break;
        }
        const auto now=GetTickCount64();
        // A dead row must not retain pressed keys while other rows continue.
        if(std::any_of(seen.begin(),seen.end(),[&](ULONGLONG stamp){
            return now-(stamp?stamp:started)>(stamp?100u:1200u);
        })) break;
    }
    return true;
}

std::uint32_t WorkerBody()
{
#if defined(HALLJOY_AJAZZ_DIAGNOSTIC)
    StabilityTrace_Write(L"INFO",L"ajazz",L"diagnostic.start",L"revision=5 forced_log=1 no_time_limit=1 generic_uap_refresh_suppressed=1");
#endif

    while (!g_stop.load(std::memory_order_acquire))
    {
        const auto candidates = Enumerate(false, true);
        bool provenPresent = false;
        bool ran = false;
        for (const auto& candidate : candidates)
        {
            Proof proof{};
            if (!EnsureClaim(candidate, &proof)) continue;
            if (IsAjazzRgb(proof.identity)) {
                provenPresent=true;
#if defined(HALLJOY_AJAZZ_DIAGNOSTIC)
                ran=RunAjazzDiagnostic(candidate);
#else
                ran=RunAjazz(candidate);
#endif
                if(ran) break;
                continue;
            }
            if (!irok_na87::IsExpectedDevice(proof.identity) && !irok_hz94::Matches(proof.identity)) continue;
            provenPresent = true;
            g_present.store(true, std::memory_order_release);
            ran = Run(candidate, proof);
            if (ran) break;
        }
        if (!provenPresent)
            g_present.store(false, std::memory_order_release);
        g_connected.store(false, std::memory_order_release);
        {
            std::lock_guard<std::mutex> lock(g_handleMutex);
            g_activeHandle = INVALID_HANDLE_VALUE;
        }
        Clear();
        if (g_stop.load(std::memory_order_acquire)) break;
        WaitForSingleObject(g_wake, ran ? 200 : kReconnectMs);
        ResetEvent(g_wake);
    }
    g_present.store(false, std::memory_order_release);
    g_running.store(false, std::memory_order_release);
    return 0;
}

void WorkerFault(const halljoy::worker::WorkerExceptionRecord& record) noexcept
{
    g_fault.store(record.kind, std::memory_order_release);
    g_stop.store(true, std::memory_order_release);
    g_connected.store(false, std::memory_order_release);
    g_present.store(false, std::memory_order_release);
    Clear();
    StabilityTrace_WriteCritical(L"ERROR", L"irok-na87", L"worker.fault",
        L"kind=%u neutralized=1", static_cast<unsigned>(record.kind));
}

void WorkerComplete(const halljoy::worker::WorkerExceptionRecord& record) noexcept
{
    g_running.store(false, std::memory_order_release);
    StabilityTrace_Write(
        record.kind == halljoy::worker::WorkerExceptionKind::None
            ? L"INFO" : L"ERROR",
        L"irok-na87", L"worker.exit", L"fault_kind=%u",
        static_cast<unsigned>(record.kind));
}

unsigned __stdcall Worker(void*) noexcept
{
    return static_cast<unsigned>(halljoy::worker::RunWorkerEntryBarrier(
        [] { return WorkerBody(); }, WorkerFault, WorkerComplete, 0xE0416737u));
}

bool Prepare()
{
    if (g_prepared.exchange(true, std::memory_order_acq_rel))
        return NativeAnalogRouting_ProtocolHasClaims(
            NativeAnalogProtocol::IrokNa87M484);
    SupportLog_Evidence("evidence.witmod.discovery revision=1", false);
    bool claimed = false;
    for (const auto& candidate : Enumerate(false, true))
    {
        Proof proof{};
        if (EnsureClaim(candidate, &proof)) claimed = true;
    }
    g_present.store(claimed, std::memory_order_release);
    return claimed;
}

bool Start()
{
    std::lock_guard<std::mutex> lock(g_serviceMutex);
    g_stop.store(false, std::memory_order_release);
    if (!g_prepared.load(std::memory_order_acquire)) Prepare();
    if (g_thread) return g_running.load(std::memory_order_acquire);
    g_stop.store(false, std::memory_order_release);
    g_running.store(true, std::memory_order_release);
    g_fault.store(halljoy::worker::WorkerExceptionKind::None,
        std::memory_order_release);
    g_wake = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    if (!g_wake)
    {
        g_running.store(false, std::memory_order_release);
        return false;
    }
    unsigned id = 0;
    g_thread = reinterpret_cast<HANDLE>(_beginthreadex(nullptr, 0, Worker,
        nullptr, 0, &id));
    if (!g_thread)
    {
        CloseHandle(g_wake);
        g_wake = nullptr;
        g_running.store(false, std::memory_order_release);
        return false;
    }
    StabilityTrace_Write(L"INFO", L"irok-na87", L"start.ok",
        L"thread_id=%u hotplug_ready=1", id);
    return true;
}

halljoy::lifecycle::StopResult Stop(
    halljoy::lifecycle::GenerationId generation)
{
    std::lock_guard<std::mutex> lock(g_serviceMutex);
    if (!g_thread) return NativeAnalogBackendStopJoined(generation);
    g_stop.store(true, std::memory_order_release);
    if (g_wake) SetEvent(g_wake);
    {
        std::lock_guard<std::mutex> active(g_handleMutex);
        if (g_activeHandle != INVALID_HANDLE_VALUE)
            CancelIoEx(g_activeHandle, nullptr);
    }
    const DWORD wait = WaitForSingleObject(g_thread, kStopTimeoutMs);
    if (wait != WAIT_OBJECT_0)
        return halljoy::lifecycle::ObserveWorkerJoin(generation,
            wait == WAIT_TIMEOUT
                ? halljoy::lifecycle::JoinWaitStatus::TimedOut
                : halljoy::lifecycle::JoinWaitStatus::Failed,
            wait == WAIT_TIMEOUT ? WAIT_TIMEOUT : GetLastError());
    CloseHandle(g_thread);
    g_thread = nullptr;
    if (g_wake) CloseHandle(g_wake);
    g_wake = nullptr;
    g_running.store(false, std::memory_order_release);
    g_connected.store(false, std::memory_order_release);
    Clear();
    return NativeAnalogBackendStopJoined(generation);
}

void Notify()
{
    std::lock_guard<std::mutex> lock(g_serviceMutex);
    if (g_wake) SetEvent(g_wake);
}

bool Present()
{
    return g_present.load(std::memory_order_acquire);
}

bool Connected()
{
    return g_connected.load(std::memory_order_acquire);
}

bool Owns(std::uint16_t hid)
{
    if(g_ajazzPlaying.load()) return Connected() && g_factoryValues.Owns(static_cast<std::uint16_t>(halljoy::ajazz::HostCode(hid)));
    if (g_hz94Pid.load()) return Connected() && (halljoy::native_layout::UsesRemapping(kHz94Layout)
        ? g_assignedValues : g_factoryValues).Owns(hid);
    return hid < g_owned.size() && Connected() &&
        (!g_ajazzPlaying.load() && halljoy::native_layout::UsesRemapping(irok_na87::kAnsiLayoutToken)
            ? g_owned[hid].load(std::memory_order_relaxed)!=0 : g_factoryValues.Owns(hid));
}

std::uint16_t Get(std::uint16_t hid)
{
    if(g_ajazzPlaying.load()) return Connected()?g_factoryValues.Read(static_cast<std::uint16_t>(halljoy::ajazz::HostCode(hid)),GetTickCount64(),100).milli:0;
    return Owns(hid) ? (!g_ajazzPlaying.load() && halljoy::native_layout::UsesRemapping(StreamLayout())
        ? g_assignedValues : g_factoryValues).Read(hid,GetTickCount64(),~std::uint64_t{0}).milli : 0;
}

void Telemetry(NativeAnalogBackendTelemetry* out)
{
    if (!out) return;
    *out = {};
    if(g_ajazzPlaying.load()) wcsncpy_s(out->deviceName,L"AJAZZ AK820 MAX HE (wired, RGB)",_TRUNCATE);
    out->present = Present();
    out->connected = Connected();
    const auto hz94Pid = g_hz94Pid.load();
    if (hz94Pid) wcsncpy_s(out->deviceName, L"E-YOOSO HZ-94", _TRUNCATE);
    if (out->connected && !g_ajazzPlaying.load())
        out->verifiedLayoutToken = hz94Pid ? kHz94Layout : irok_na87::kAnsiLayoutToken;
    out->genericProtocol = out->connected && hz94Pid != 0;
    // AJAZZ plays only after the exact SG8994HERGB identity proof: official illumipc layout.
    if (out->connected && g_ajazzPlaying.load())
        out->verifiedLayoutToken=halljoy::layout_identity::Token("ajazz-m484","SG8994HERGB");
    out->vendorId = kVendorId;
    // The stored PID comes from the 16-bit HIDD_ATTRIBUTES::ProductID.
    out->productId = hz94Pid ? static_cast<std::uint16_t>(hz94Pid) : kProductId;
    out->usagePage = kUsagePage;
    out->usage = kUsage;
    out->mappedKeys = g_mapped.load();
    out->activeKeys = g_active.load();
    out->nominalRawLevels = g_ajazzPlaying.load() ? 1001u : irok_nd75::kNominalTravelMaximum + 1u;
    out->inputReportBytes = g_inputBytes.load();
    out->outputReportBytes = g_outputBytes.load();
    out->updateHz10 = g_hz10.load();
    out->averageIntervalUs = g_avgUs.load();
    out->maximumIntervalUs = g_maxUs.load();
    out->successfulUpdates = g_updates.load();
    out->failedUpdates = g_failures.load();
    const auto last = g_lastMs.load(), now = GetTickCount64();
    out->lastUpdateAgeMs = last && now >= last
        ? static_cast<std::uint32_t>(std::min<std::uint64_t>(now - last,
            0xffffffffull)) : 0;
    _snwprintf_s(out->status, _countof(out->status), _TRUNCATE,
        g_ajazzPlaying.load() ? L"AJAZZ AK820 MAX HE (wired, RGB): USB analog, automatic key range, range_errors=%llu" :
        hz94Pid ? L"E-YOOSO HZ-94: Witmod trial, device map, raw 0..40, range_errors=%llu" :
        L"IROK NA87 M484 experimental, device map, raw 0..40, range_errors=%llu",
        static_cast<unsigned long long>(g_outOfRange.load()));
}
}

const NativeAnalogBackendDescriptor& IrokNa87_GetNativeBackendDescriptor()
{
    static const NativeAnalogBackendDescriptor descriptor{
        kNativeAnalogBackendAbiVersion,
        sizeof(NativeAnalogBackendDescriptor),
        "irok-na87-m484-experimental",
        L"IROK NA87 Mag",
        NativeAnalogProtocol::IrokNa87M484,
        NativeAnalogStartPhase::BeforeUap,
        NativeAnalogBackendFlag_StreamTransport |
            NativeAnalogBackendFlag_ReadOnlyProbe |
            NativeAnalogBackendFlag_ReversibleControlProbe,
        &Prepare,
        &Start,
        &Stop,
        &Notify,
        &Present,
        &Connected,
        &Owns,
        &Get,
        &Telemetry,
    };
    return descriptor;
}

bool IrokNa87_TryRunSelfTest(int& result) noexcept
{
#if !defined(HALLJOY_IROK_NA87_NATIVE)
    (void)result; return false;
#else
    int argc=0; auto argv=CommandLineToArgvW(GetCommandLineW(),&argc);
    const bool selected=argv && argc==2 && wcscmp(argv[1],L"--halljoy-na87-native-self-test")==0;
    if (argv) LocalFree(argv);
    if (!selected) return false;
    result=10;
    const std::array<std::array<std::uint8_t,22>,6> rows{{
        {0x29,0x0,0x3a,0x3b,0x3c,0x3d,0x0,0x3e,0x3f,0x40,0x41,0x42,0x43,0x44,0x45,0x46,0x47,0x48,0x0,0x0,0x0,0x0},
        {0x35,0x1e,0x1f,0x20,0x21,0x22,0x23,0x24,0x25,0x26,0x27,0x2d,0x2e,0x0,0x2a,0x49,0x4a,0x4b,0x0,0x0,0x0,0x0},
        {0x2b,0x14,0x1a,0x8,0x15,0x17,0x1c,0x18,0xc,0x12,0x13,0x2f,0x30,0x0,0x31,0x4c,0x4d,0x4e,0x0,0x0,0x0,0x0},
        {0x39,0x0,0x4,0x16,0x7,0x9,0xa,0xb,0xd,0xe,0xf,0x33,0x34,0x32,0x28,0x0,0x0,0x0,0x0,0x0,0x0,0x0},
        {0xe1,0x64,0x1d,0x1b,0x6,0x19,0x5,0x11,0x10,0x36,0x37,0x38,0x87,0x0,0xe5,0x0,0x52,0x0,0x0,0x0,0x0,0x0},
        {0xe0,0xe3,0xe2,0x0,0x0,0x0,0x2c,0x0,0x0,0x0,0xe6,0xfa,0x65,0xe4,0x0,0x50,0x51,0x4f,0x0,0x0,0x0,0x0}
    }};
    irok_na87::MapReader reader;
    for (unsigned row=0; row<6; ++row) {
        irok_nd75::Report r{}; r[0]=1; r[1]=0x10; r[5]=22;
        r[3]=row==5?255:0; r[4]=row==5?255:static_cast<std::uint8_t>(row+1);
        std::copy(rows[row].begin(),rows[row].end(),r.begin()+6);
        if (!reader.Feed(r)) return true;
        if (row<5 && reader.Complete()) return true;
    }
    if (!reader.Complete() || irok_nd75::MappedKeyCount(reader.Map())!=90) return true;
    Proof proof{}; proof.map=reader.Map();
    Clear(); PublishOwnership(proof.map); g_connected=true;
    bool pass=true;
    const auto put=[&](unsigned row,unsigned col,unsigned raw) {
        irok_nd75::Report r{1,0x21,0,0,0,3,1};
        r[7]=static_cast<std::uint8_t>(row);r[8]=static_cast<std::uint8_t>(col);r[9]=static_cast<std::uint8_t>(raw);
        std::uint8_t hid=0;std::uint16_t milli=0;
        if (!irok_na87::DecodeSample(r,proof.map,hid,milli)) return false;
        irok_nd75::LiveEvent e{};irok_nd75::DecodeLiveEvent(r.data(),64,&e);
        Publish(e,proof,0);return Get(hid)==milli;
    };
    // Real device map: W, A, S, D. Values must survive unrelated updates.
    pass=put(2,2,10) && put(3,2,20) && put(3,3,30) && put(3,4,40);
    pass=pass && Get(0x1a)==250 && Get(4)==500 && Get(0x16)==750 && Get(7)==1000;
    const auto routed=NativeAnalogBackends_ReadMilli(4);
    pass=pass && NativeAnalogBackends_CatalogIsValid() && routed.owned && routed.connected && routed.milli==500;
    NativeAnalogBackendTelemetry telemetry{};Telemetry(&telemetry);
    pass=pass && telemetry.connected && telemetry.verifiedLayoutToken==irok_na87::kAnsiLayoutToken;
    pass=pass && put(2,2,0) && Get(0x1a)==0 && Get(4)==500 && Get(0x16)==750 && Get(7)==1000;
    Publish({3,2,255},proof,0); pass=pass && Get(4)==500;
    for (unsigned depth=0;depth<=40;++depth) pass=put(3,2,depth) && pass;
    irok_nd75::Report bad{1,0x21,0,0,0,3,1,6,0,10};
    std::uint8_t hid=0;std::uint16_t milli=0;
    pass=pass && !irok_na87::DecodeSample(bad,proof.map,hid,milli);
    bad[7]=3;bad[8]=2;bad[2]=1;
    pass=pass && !irok_na87::DecodeSample(bad,proof.map,hid,milli);
    auto duplicate=reader;auto altered=irok_na87::MapRequest();altered[5]=22;altered[4]=1;
    duplicate.Feed(altered);pass=pass && !duplicate.Complete();
    const auto mask=irok_nd75::SubscriptionMask(proof.map);
    const auto subscription=irok_nd75::BuildSubscriptionRequest(mask);
    pass=pass && subscription[0]==1 && subscription[1]==0x21 && subscription[5]==0x18 && subscription[6]==2;
    Clear(); pass=pass && Get(4)==0 && Get(7)==0 && !Owns(4) && g_active==0;
    PublishOwnership(proof.map);pass=put(3,2,20) && pass;
    g_connected=false;pass=pass && Get(4)==0; Clear();
    // Exercise the production publication in both modes with duplicate assignments.
    proof.map[2*22+2]=4;
    PublishOwnership(proof.map);g_connected=true;
    Publish({2,2,30},proof,0);Publish({3,2,20},proof,0);
    pass=pass && Get(0x1a)==750 && Get(4)==500;
    halljoy::native_layout::activeToken=irok_na87::kAnsiLayoutToken;
    pass=pass && Get(4)==750 && Get(0x1a)==0;
    Publish({2,2,0},proof,0);pass=pass && Get(4)==500;
    halljoy::native_layout::enabled=false;
    pass=pass && Get(4)==500 && Get(0x1a)==0;
    halljoy::native_layout::enabled=true;g_connected=false;Clear();
#if defined(HALLJOY_AJAZZ_DIAGNOSTIC)
    // Exercise the real AJAZZ publication and registry routing with a device map
    // that differs from NA87. Duplicate assignments retain the deeper position.
    g_ajazzProof=proof;BindAjazzMap(proof);g_connected=true;
    PublishAjazzPlay({2,2,30});PublishAjazzPlay({3,2,20});
    pass=pass && Get(4)==750 && !Owns(0x1a);
    PublishAjazzPlay({2,2,0});pass=pass && Get(4)==500;
    PublishAjazzPlay({3,2,0});pass=pass && Get(4)==0;
    PublishAjazzPlay({3,2,40});PublishAjazzPlay({3,2,255});
    pass=pass && Get(4)==1000;
    Telemetry(&telemetry);pass=pass && telemetry.verifiedLayoutToken==0;
    const auto ajazzRouted=NativeAnalogBackends_ReadMilli(4);
    pass=pass && ajazzRouted.owned && ajazzRouted.milli==1000;
    PublishAjazzPlay({5,11,20});pass=pass && Get(1033)==500 && Get(250)==500;
    EndAjazzPlay();pass=pass && Get(4)==0 && !Connected();
#endif
    // Ordinary-build AJAZZ publication: raw-only input, exact admission,
    // isolated release, malformed rows and the Fn alias use the real registry.
    irok_nd75::DeviceInfo rgb{};
    strcpy_s(rgb.controller.data(),rgb.controller.size(),"M484");
    strcpy_s(rgb.product.data(),rgb.product.size(),"SG8994HERGB");
    strcpy_s(rgb.firmware.data(),rgb.firmware.size(),"V1.13.17");
    pass=pass && IsAjazzRgb(rgb);
    strcpy_s(rgb.firmware.data(),rgb.firmware.size(),"V1.13.17 ");
    pass=pass && IsAjazzRgb(rgb);
    strcpy_s(rgb.firmware.data(),rgb.firmware.size(),"V1.13.170");
    pass=pass && !IsAjazzRgb(rgb);
    rgb.product[8]='X';pass=pass && !IsAjazzRgb(rgb);
    proof.map.fill(0);proof.map[68]=4;proof.map[69]=22;proof.map[121]=250;
    BindAjazzMap(proof);g_ajazzLimits=std::make_unique<halljoy::ajazz::DynamicLimits>();g_connected=true;
    auto rawRow=[&](unsigned row,unsigned first,unsigned second) {
        irok_nd75::Report r{};r[0]=1;r[1]=0x23;r[4]=static_cast<unsigned char>(row+1);r[5]=44;
        for(unsigned col=0;col<22;++col) {
            const unsigned adc=(row==3 && col==2)?first:(row==3 && col==3)?second:
                (row==5 && col==11)?first:2400;
            r[6+2*col]=static_cast<unsigned char>(adc>>8);r[7+2*col]=static_cast<unsigned char>(adc);
        }
        for(unsigned i=0;i<3;++i)PublishAjazzRawRow(r);
    };
    rawRow(3,2400,2400);rawRow(5,2400,2400);
    rawRow(3,1353,1355);pass=pass && Get(4)==1000 && Get(22)==1000;
    rawRow(3,2400,1355);pass=pass && Get(4)==0 && Get(22)==1000;
    rawRow(5,1289,2400);pass=pass && Get(1033)==1000 && Get(250)==1000;
    auto malformed=irok_nd75::Report{};malformed[0]=1;malformed[1]=0x23;
    malformed[4]=7;malformed[5]=44;PublishAjazzRawRow(malformed);
    pass=pass && Get(22)==1000 && !IsAjazzRawRow(malformed);
    const auto rawRouted=NativeAnalogBackends_ReadMilli(22);
    Telemetry(&telemetry);
    pass=pass && rawRouted.owned && rawRouted.milli==1000 &&
        telemetry.verifiedLayoutToken==halljoy::layout_identity::Token("ajazz-m484","SG8994HERGB") &&
        wcscmp(telemetry.deviceName,L"AJAZZ AK820 MAX HE (wired, RGB)")==0;
    EndAjazzPlay();pass=pass && Get(22)==0 && !Connected();

    // HZ-94 uses the same route with its own physical matrix and Fn identity.
    const auto mapEvidence = irok_hz94::InspectMap(irok_hz94::kFactoryMap);
    pass = pass && mapEvidence.Admitted() && mapEvidence.factory == 94 &&
        mapEvidence.assigned == 94 && !irok_hz94::InspectMap(irok_na87::FactoryMap()).Admitted();
    Proof hz94Proof{};
    hz94Proof.map = irok_hz94::kFactoryMap;
    hz94Proof.map[3 * 22 + 3] = 4; // S remapped to A; physical channels remain independent.
    g_hz94Pid.store(0x7473);
    pass = PublishOwnership(hz94Proof.map) && pass;
    Telemetry(&telemetry);
    pass = pass && !telemetry.connected && !telemetry.verifiedLayoutToken;
    Publish({3, 2, 20}, hz94Proof, 0);
    Publish({3, 3, 30}, hz94Proof, 0);
    Publish({5, 11, 10}, hz94Proof, 0);
    g_connected.store(true);
    Telemetry(&telemetry);
    pass = pass && telemetry.connected && telemetry.verifiedLayoutToken == kHz94Layout &&
        telemetry.productId == 0x7473 && wcscmp(halljoy::layout_identity::Match(kHz94Layout), L"E-YOOSO HZ-94 ANSI") == 0 &&
        Get(4) == 500 && Get(22) == 750 && Get(halljoy::keycode::kFn) == 250;
    halljoy::native_layout::activeToken = kHz94Layout;
    pass = pass && Get(4) == 750;
    Publish({3, 3, 0}, hz94Proof, 0);
    pass = pass && Get(4) == 500;
    g_connected.store(false);
    Clear();
    pass = pass && !Get(4) && !halljoy::native_layout::Read(kHz94Layout).complete;

    const auto pipeName=L"\\\\.\\pipe\\HallJoyNa87SelfTest-"+std::to_wstring(GetCurrentProcessId());
    {
        Handle server(CreateNamedPipeW(pipeName.c_str(),PIPE_ACCESS_DUPLEX|FILE_FLAG_OVERLAPPED,
            PIPE_TYPE_BYTE|PIPE_READMODE_BYTE|PIPE_WAIT,1,4096,4096,0,nullptr));
        Candidate candidate{};candidate.path=pipeName;candidate.attributes.ProductID=0x7473;
        if (!server) pass=false;
        else {
            Session stream(candidate,false,false);
            if (!stream.Open()) pass=false;
            else {
                irok_nd75::Report report{};
                pass=(!stream.Read(&report,20) && GetLastError()==WAIT_TIMEOUT) && pass;
                auto sent=irok_nd75::BuildIdentityRequest();DWORD bytes=0;
                pass=TimedIo(server.value,true,sent.data(),64,200,&bytes) && bytes==64 && pass;
                pass=stream.Read(&report,200) && report==sent && pass;
                pass=(!stream.Read(&report,20) && GetLastError()==WAIT_TIMEOUT) && pass;
                // Exercise the ordinary proof functions over real overlapped I/O.
                const auto writeReply = [&](irok_nd75::Report& reply) {
                    DWORD size = 0;
                    return TimedIo(server.value, true, reply.data(), 64, 200, &size) && size == 64;
                };
                const auto requestWas = [&](irok_nd75::Report expected) {
                    irok_nd75::Report request{};
                    DWORD size = 0;
                    return TimedIo(server.value, false, request.data(), 64, 200, &size) &&
                        size == 64 && request == expected;
                };
                const auto identityReply = [&](const char* product) {
                    irok_nd75::Report reply{1, 0x0d};
                    const auto size = sprintf_s(reinterpret_cast<char*>(reply.data() + 6),
                        reply.size() - 6, "GDXXX,01,KB,FR,%s,V1_00_09", product);
                    reply[5] = static_cast<std::uint8_t>(size + 6);
                    return reply;
                };
                irok_nd75::DeviceInfo board{};
                auto neighbor = identityReply("7186_AUSHERGB");
                pass = writeReply(neighbor) && !ReceiveIdentity(stream, &board) &&
                    requestWas(irok_nd75::BuildIdentityRequest()) && pass;
                auto boardReply = identityReply("7187_AUSXHERGB");
                pass = writeReply(boardReply) && ReceiveIdentity(stream, &board) &&
                    irok_hz94::Matches(board) && requestWas(irok_nd75::BuildIdentityRequest()) && pass;
                irok_nd75::Report capReply{1, 0x21, 0, 0, 0, 2, 4, 40};
                irok_nd75::CapabilityInfo capability{};
                pass = writeReply(capReply) && ReceiveCapability(stream, &capability) &&
                    capability.sensitivity == 40 && requestWas(irok_nd75::BuildCapabilityRequest()) && pass;
                for (unsigned row = 0; row < 6; ++row) {
                    irok_nd75::Report mapReply{1, 0x10};
                    mapReply[3] = row == 5 ? 255 : 0;
                    mapReply[4] = row == 5 ? 255 : static_cast<std::uint8_t>(row + 1);
                    mapReply[5] = 22;
                    std::copy_n(irok_hz94::kFactoryMap.begin() + row * 22, 22, mapReply.begin() + 6);
                    pass = writeReply(mapReply) && pass;
                }
                irok_nd75::PositionToHid map{};
                pass = ReceiveMap(stream, &map, true, true) && map == irok_hz94::kFactoryMap &&
                    requestWas(irok_na87::MapRequest()) && pass;
                pass = !stream.Read(&report, 20) && GetLastError() == WAIT_TIMEOUT && pass;
                // Destruction must cancel and reap the still-pending read.
            }
        }
    }
    // The isolated test owns its log. No real HID or virtual gamepad is opened.
    StabilityTrace_Init();DebugLog_Init();
#if defined(HALLJOY_AJAZZ_DIAGNOSTIC)
    DebugLog_Write(L"[support] ajazz_log_route_self_test=PASS");
    StabilityTrace_Write(L"INFO",L"ajazz",L"sensor_log_route.self_test",L"key_code=250 samples=4095,2048,0");
    StabilityTrace_Write(L"INFO",L"ajazz",L"log_route.self_test",L"result=PASS hardware_opened=0");
#endif
    DebugLog_Write(L"[irok.na87.self_test] result=%ls device_map=1 independent_depths=1 release_isolation=1 full_range=1 malformed_rejected=1 disconnect_neutral=1 hardware_opened=0",pass?L"PASS":L"FAIL");
    DebugLog_Write(L"[backend.input] hid=PRIVACY_SENTINEL raw=PRIVACY_SENTINEL");
    StabilityTrace_Write(L"INFO",L"irok-na87",L"privacy.test",L"error=0 active_profile=PRIVACY_SENTINEL private text next=PRIVATE_TAIL");
    const bool logStopped=DebugLog_Shutdown().RestartSafe();
    result=pass && logStopped?0:10;StabilityTrace_Shutdown(result);
    return true;
#endif
}

void IrokNa87_UpdateWindow(HWND window) noexcept
{
#if defined(HALLJOY_AJAZZ_DIAGNOSTIC)
    static unsigned previous=~0u;
    const unsigned state=g_diagnosticStatus.load();
    static ULONGLONG lastHealth=0;
    const auto now=GetTickCount64();
    if(now-lastHealth>=2000) {
        lastHealth=now;
        StabilityTrace_Write(L"INFO",L"ajazz",L"ui.health",L"worker_running=%u status=%u stopping=%u",
            g_running.load()?1u:0u,state,g_stop.load()?1u:0u);
    }

    if(state==previous) return;
    previous=state;
    SetWindowTextW(window,state==10?L"HallJoy - AJAZZ: dynamic key limits (preliminary); logging enabled":
        state==11?L"HallJoy - AJAZZ sensor diagnostic: depth events received; waiting for raw sensor coverage":
        state==13?L"HallJoy - AJAZZ: dynamic key limits (preliminary); logging enabled":
        state==12?L"HallJoy - AJAZZ diagnostic: disconnected; data saved":
        L"HallJoy - AJAZZ diagnostic: searching; automatic logging enabled");
#elif defined(HALLJOY_IROK_NA87_NATIVE) && defined(HALLJOY_DIAGNOSTIC)
    static unsigned previous=0;
    const unsigned state=Connected()?1+g_diagnosticStatus.load():Present()?4:0;
    if (state==previous) return;
    previous=state;
    SetWindowTextW(window,state==1?L"HallJoy - NA87: analog connected":
        state==2?L"HallJoy - NA87: analog connected; diagnostic data collected":
        state==3?L"HallJoy - NA87: analog connected; limited diagnostic coverage":
        state==4?L"HallJoy - NA87: reconnecting":L"HallJoy");
#else
    (void)window;
#endif
}
