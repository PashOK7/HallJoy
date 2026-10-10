// Production Alumix104 session with synchronous fake HID I/O, no hardware.
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <setupapi.h>
#include <hidsdi.h>
#include <hidpi.h>
#include <array>
#include <cassert>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

static unsigned scenario = 0, readCount = 0;
static std::vector<unsigned> writes;
static std::array<unsigned char, 65> lastBatch{};
static bool batchReplyPending = false;
static std::vector<std::string> records;
void CheckActive();
void RequestStop();
static HANDLE FakeCreate(LPCWSTR, DWORD, DWORD, LPSECURITY_ATTRIBUTES,
                         DWORD, DWORD, HANDLE) {
    return CreateEventW(nullptr, TRUE, FALSE, nullptr);
}
static BOOLEAN FakeBuffers(HANDLE, ULONG) { return TRUE; }
static BOOL FakeWrite(HANDLE, LPCVOID data, DWORD bytes, LPDWORD,
                      LPOVERLAPPED overlapped) {
    assert(bytes == 65);
    const auto* p = static_cast<const unsigned char*>(data);
    assert(p[0] == 0 && p[1] == 0xaa &&
           (p[2] == 0x66 || p[2] == 0x67 || p[2] == 0x68));
    writes.push_back(p[2]);
    if (p[2] == 0x68) {
        std::memcpy(lastBatch.data(), p, 65);
        batchReplyPending = true;
    }
    if (p[2] == 0x66) readCount = 0;
    if (scenario == 3 && p[2] == 0x66) {
        SetLastError(ERROR_DEVICE_NOT_CONNECTED);
        return FALSE;
    }
    overlapped->InternalHigh = bytes;
    return TRUE;
}
static BOOL FakeResult(HANDLE, LPOVERLAPPED overlapped, LPDWORD bytes, BOOL) {
    *bytes = static_cast<DWORD>(overlapped->InternalHigh);
    return TRUE;
}
static BOOL FakeRead(HANDLE, LPVOID data, DWORD bytes, LPDWORD,
                     LPOVERLAPPED overlapped) {
    assert(bytes == 65);
    auto* p = static_cast<unsigned char*>(data);
    std::memset(p, 0, bytes);
    overlapped->InternalHigh = bytes;
    ++readCount;
    p[1] = 0x55;
    if (batchReplyPending) {
        batchReplyPending = false;
        std::memcpy(p, lastBatch.data(), 65);
        p[1] = 0x55;
        return TRUE;
    }
    if (writes.back() == 0x67) { p[2] = 0x67; return TRUE; }
    if (readCount == 1) { p[2] = 0x67; return TRUE; }
    if (readCount == 2 && scenario != 4) { p[2] = 0x66; return TRUE; }
    if (scenario == 2 && readCount == 5) {
        SetLastError(ERROR_DEVICE_NOT_CONNECTED);
        return FALSE;
    }
    p[2] = 0xfb; p[3] = 49; p[13] = 35;
    if (scenario == 1 && readCount == 4) p[3] = 121;
    else if (readCount == 3 || (scenario == 1 && readCount == 4)) p[11] = 175;
    else if (readCount == 4) {
        if (scenario != 4) CheckActive();
        p[3] = 69; p[11] = 175;
    }
    if (scenario == 0 && readCount == 5) {
        p[1] = 0x19; p[2] = 0x42; // Exact-device unknown report is captured.
    }
    if (readCount >= 5) RequestStop();
    return TRUE;
}

#define CreateFileW FakeCreate
#define HidD_SetNumInputBuffers FakeBuffers
#define WriteFile FakeWrite
#define ReadFile FakeRead
#define GetOverlappedResult FakeResult
#include "../HallJoy/alumix104_backend.cpp"
#undef CreateFileW
#undef HidD_SetNumInputBuffers
#undef WriteFile
#undef ReadFile
#undef GetOverlappedResult

void CheckActive() {
    const auto& descriptor = Alumix104_GetNativeBackendDescriptor();
    assert(!Alumix104_TrialConnected() && !descriptor.isConnected());
    assert(!descriptor.ownsHid(4) && descriptor.getMilli(4) == 0);
}
void RequestStop() { g_stop.store(true); }

bool SupportLog_RedSquareResearch(const char* line) noexcept {
    assert(std::strlen(line) < 240);
    records.emplace_back(line); return true;
}
void SupportLog_Event(const char*, std::uint64_t, SupportLogDetail) noexcept {}
std::uint64_t SupportLog_RequestSnapshot() noexcept { return 1; }
bool Backend_GetVirtualGamepadsEnabled() { return true; }
BackendStatus Backend_GetStatus() { BackendStatus result{}; result.vigemOk = true; return result; }
bool Bindings_IsHidBoundForPad(int pad, std::uint16_t hid) {
    return pad == 0 && hid == 4;
}
bool Bindings_IsHidBound(std::uint16_t hid) { return hid == 4; }
AxisBinding Bindings_GetAxisForPad(int pad, Axis axis) {
    AxisBinding binding{};
    if (pad == 0 && axis == Axis::LX) binding.minusHids[0] = 4;
    return binding;
}
std::uint16_t Bindings_GetTriggerForPad(int, Trigger) { return 0; }
bool Bindings_ButtonHasHidForPad(int, GameButton, std::uint16_t) { return false; }

int main() {
    assert(halljoy::alumix104::SelfTest());
    {
        records.clear(); writes.clear();
        BatchProbe batch;
        HANDLE device = CreateEventW(nullptr, TRUE, FALSE, nullptr);
        for (std::uint64_t now = 100; now < 400 && !batch.sweepReported; now += 10) {
            batch.Step(device, now);
            assert(batch.awaiting);
            if (now == 100) assert(lastBatch[9] == 49); // Bound A before matrix sweep.
            std::vector<std::uint8_t> echo(lastBatch.begin(), lastBatch.end());
            echo[1] = 0x55;
            batch.Observe(echo, 65, now);
        }
        assert(batch.sweepReported && batch.unique == 121 &&
               batch.echo == 18 && batch.candidate == 0);
        assert(writes.size() == 18 && writes.front() == 0x68);
        batch.Step(device, 400);
        assert(batch.awaiting);
        std::vector<std::uint8_t> pressed(lastBatch.begin(), lastBatch.end());
        pressed[1] = 0x55; pressed[9] = 1; pressed[12] = 25;
        batch.Observe(pressed, 65, 400);
        assert(batch.candidate == 1 && batch.positive == 1 && batch.nextSlot == 2);
        for (std::uint64_t now = 410; now < 5000 && !batch.releases; now += 10) {
            batch.Step(device, now);
            if (!batch.awaiting) continue;
            std::vector<std::uint8_t> released(lastBatch.begin(), lastBatch.end());
            released[1] = 0x55; released[9] = 1; // Non-echo zero measurement.
            batch.Observe(released, 65, now);
        }
        assert(batch.releases >= 1);
        batch.Final();
        CloseHandle(device);
        bool slotLine = false;
        for (const auto& line : records)
            if (line.find("batch68_slot slot=1 ") == 0 &&
                line.find(" releases=1 ") != std::string::npos) slotLine = true;
        assert(slotLine);
    }
    {
        const std::uint8_t ids[]{49, 69, 4, 77, 85, 120, 0};
        protocol::BatchFrame request{};
        std::array<protocol::BatchReading, protocol::kBatchKeys> readings{};
        assert(protocol::MakeBatchRequest(ids, 7, 56, request));
        assert(request[1] == 0xaa && request[2] == 0x68 &&
               request[3] == 56 && request[4] == 56 && request[7] == 1 &&
               request[9] == 49 && request[17] == 69);
        auto reply = request;
        reply[1] = 0x55;
        assert(protocol::ParseBatchReply(reply.data(), reply.size(), request,
               ids, 7, readings) == protocol::BatchReplyKind::Echo);
        reply[3] = 1; // Official parser allows a type in this header field.
        assert(protocol::ParseBatchReply(reply.data(), reply.size(), request,
               ids, 7, readings) == protocol::BatchReplyKind::Echo);
        reply[9] = 1;
        reply[12] = 0x34; reply[13] = 0x12;
        assert(protocol::ParseBatchReply(reply.data(), reply.size(), request,
               ids, 7, readings) == protocol::BatchReplyKind::Candidate);
        assert(readings[0].index == 49 && readings[0].status == 1 &&
               readings[0].stroke == 0x1234);
        ++reply[4];
        assert(protocol::ParseBatchReply(reply.data(), reply.size(), request,
               ids, 7, readings) == protocol::BatchReplyKind::Malformed);
        assert(!protocol::MakeBatchRequest(ids, 0, 0, request));
        const std::uint8_t duplicate[]{49, 49};
        assert(!protocol::MakeBatchRequest(duplicate, 2, 0, request));
    }
    // A repeated held value and then silence are distinct from an explicit
    // release. Digital state is recorded as reference and never changes depth.
    AnalogTrace trace;
    trace.Reset(100);
    protocol::TravelSample a{};
    a.index = 49; a.stroke = 175; a.maximum = 35;
    trace.Observe(a, 100, 1u);
    trace.Observe(a, 110, 1u);
    assert(trace.sensors[49].samePositive == 1);
    assert(trace.sensors[49].digitalHeldPositive == 2);
    assert(trace.sensors[49].lastPositive);
    protocol::TravelSample b{};
    b.index = 69; b.stroke = 150; b.maximum = 35;
    trace.Observe(b, 900, 0u);
    assert(trace.sensors[49].lastPositive && trace.sensors[49].explicitRelease == 0);
    a.stroke = 0;
    trace.Observe(a, 1000, 0u);
    assert(!trace.sensors[49].lastPositive);
    assert(trace.sensors[49].maxGapAfterPositiveMs == 890);
    assert(trace.sensors[49].explicitRelease == 1);
    assert(trace.sensors[69].digitalOffPositive == 1);
    std::vector<std::uint8_t> other(65);
    other[1] = 0xaa; other[2] = 0x68;
    assert(!trace.ObserveOther(other, 65, 0));
    assert(trace.aaResponses == 1 && trace.aaOpcodeCounts[0x68] == 1);
    other[1] = 0x19; other[2] = 0;
    assert(trace.ObserveOther(other, 65, (std::uint64_t(1) << 32) | 3u));
    other[7] = 1;
    assert(trace.ObserveOther(other, 65, (std::uint64_t(1) << 32) | 3u));
    assert(!trace.ObserveOther(other, 65, (std::uint64_t(1) << 32) | 3u));
    assert(trace.otherPrefix == 3 && trace.nextUnknownSlot == 2);
    assert(trace.unknown[0x19].reports == 3 &&
           trace.unknown[0x19].changed == 1 &&
           trace.unknown[0x19].stableChanged == 1 &&
           trace.unknown[0x19].held2 == 3 &&
           trace.unknown[0x19].held2Changed == 1 &&
           trace.unknown[0x19].changedOffsets == (std::uint64_t(1) << 5));
    for (scenario = 0; scenario < 5; ++scenario) {
        readCount = 0; writes.clear(); records.clear();
        batchReplyPending = false;
        g_stop.store(false); g_connected.store(false);
        g_published.store(0); g_consumed.store(0);
        g_outputPublished.store(0); g_outputNonneutral.store(0);
        RunSession({L"fake", 65, 65});
        assert(!Connected() && Get(4) == 0);
        assert(writes.front() == 0x67 && writes.back() == 0x67);
        bool summary = false, offAck = false, traceFinal = false;
        bool unknownStart = false, unknownClass = false;
        bool rawContext = false, rawBytes = false, rawSummary = false;
        bool mapping = false;
        for (const auto& line : records) {
            if (line.find("analog_pipeline published=") == 0) summary = true;
            if (line.find("final_off_ack=1") != std::string::npos) offAck = true;
            if (line.find("trace_final windows=") == 0) traceFinal = true;
            if (line.find("unknown_trace begin=1") == 0) unknownStart = true;
            if (line.find("trace_unknown slot=1 reports=1") == 0) unknownClass = true;
            if (line.find("unknown_packet id=1 slot=1 report_n=1 bytes=65") == 0)
                rawContext = line.find("ref_index=") != std::string::npos;
            if (line.find("unknown_bytes id=1 hex=00194231") == 0)
                rawBytes = line.size() == std::strlen("unknown_bytes id=1 hex=") + 130;
            if (line.find("unknown_packet_summary eligible=1 captured=1") == 0)
                rawSummary = true;
            if (line.find("trace_mapping slot=1 ") == 0) mapping = true;
        }
        assert(summary && offAck && traceFinal);
        assert(std::find(writes.begin(), writes.end(), 0x68) == writes.end());
        if (scenario < 3) assert(unknownStart);
        if (scenario == 0) assert(unknownClass && rawContext && rawBytes && rawSummary);
        if (scenario == 0) assert(mapping);
        if (scenario == 0) {
            assert(g_published.load() == 0 && g_consumed.load() == 0);
            assert(g_outputNonneutral.load() == 0);
        }
        if (scenario == 1) assert(g_failed.load() > 0);
        if (scenario == 2) assert(g_failed.load() > 0);
        if (scenario == 3) assert(g_published.load() == 0);
        if (scenario == 4) assert(g_published.load() == 0);
    }
    std::puts("Alumix104 unknown-packet trace PASS: changed full payload and reference, no repeat 0x68, disconnect, failed ON, absent ACK, OFF cleanup");
}
