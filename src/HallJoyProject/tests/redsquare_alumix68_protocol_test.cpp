#include "../HallJoy/redsquare_alumix68_protocol.h"

#include <array>
#include <cassert>
#include <cstdio>
#include <set>

int main()
{
    namespace a68 = halljoy::redsquare_alumix68;

    assert(a68::SelfTest());
    static_assert(!a68::kAdmitted, "Alumix 68 is disabled by owner decision 2026-10-12");

    // Exact identity and vendor collection.
    assert(a68::kVendorId == 0x0c45 && a68::kProductId == 0x80a2);
    assert(a68::ExactCollection(0xff68, 0x61, 65, 65));
    assert(!a68::ExactCollection(0xff68, 0x61, 33, 33));
    assert(!a68::ExactCollection(0xff67, 0x61, 65, 65));

    // 68 mapped scan slots, unique HID usages, Fn uses HallJoy's extended code.
    std::set<unsigned> usages;
    unsigned mapped = 0;
    for (std::size_t slot = 0; slot < a68::kSlots; ++slot) {
        const auto hid = a68::kSlotHid[slot];
        if (!hid) continue;
        ++mapped;
        assert(usages.insert(hid).second);  // no duplicate usage
    }
    assert(mapped == 68 && usages.size() == 68);
    assert(a68::kSlotHid[1] == 41 /* Esc */ && a68::kSlotHid[85] == halljoy::keycode::kFn);
    assert(a68::kSlotHid[11] == 4 /* A */ && a68::kSlotHid[18] == 57 /* Caps */);
    assert(a68::kSlotHid[0] == 0 && a68::kSlotHid[6] == 0 /* sentinel slots */);

    // Scan map (fingerprint) agrees with the mapped slots and the code window.
    unsigned scanMapped = 0;
    for (std::size_t slot = 0; slot < a68::kSlots; ++slot) {
        const bool sentinel = a68::kScanMap[slot] == 125;
        assert(sentinel == (a68::kSlotHid[slot] == 0));
        if (!sentinel) ++scanMapped;
    }
    assert(scanMapped == 68);
    assert(a68::kScanMapOffset == 0xc3d2 && a68::kCodeFpOffset == 0x4e00);
    assert(a68::kCodeFp.size() == 56 && a68::kCodeFp[36] == 0xaa /* cmp r0,#0xAA in dispatcher */);

    // Stroke -> permille against the 3.3 mm (330 unit) full scale.
    assert(a68::ToMilli(0) == 0 && a68::ToMilli(165) == 500 && a68::ToMilli(330) == 1000 &&
           a68::ToMilli(340) == 1000 && a68::kFullScaleStroke == 330);

    // AULA MINI 60 HE PRO shares 0C45:80A2: recognized by its device info (manufacturer 0x0166,
    // product 0x110C), never by the VID/PID alone; Alumix replies and garbage are not AULA.
    {
        std::array<std::uint8_t, a68::kReportBytes> infoRequest{};
        assert(a68::MakeReadRequest(a68::kCmdDeviceInfo, a68::kDeviceInfoBytes, 0, infoRequest));
        assert(infoRequest[0] == 0 && infoRequest[1] == 0xaa && infoRequest[2] == 0x10 &&
               infoRequest[3] == 56 && infoRequest[4] == 0 && infoRequest[5] == 0 && infoRequest[7] == 1);
        std::array<std::uint8_t, a68::kDeviceInfoBytes> info{};
        info[4] = 0x45; info[5] = 0x0c; info[6] = 0xa2; info[7] = 0x80;     // device bytes 12..15
        info[12] = 0x66; info[13] = 0x01; info[14] = 0x0c; info[15] = 0x11;  // device bytes 20..23
        assert(a68::IsAulaMini60DeviceInfo(info.data()));
        auto other = info; other[12] = 0x67;  assert(!a68::IsAulaMini60DeviceInfo(other.data()));   // manufacturer
        other = info; other[14] = 0x0d;       assert(!a68::IsAulaMini60DeviceInfo(other.data()));   // product
        other = info; other[6] = 0xa1;        assert(!a68::IsAulaMini60DeviceInfo(other.data()));   // pid
        other = info; other[4] = 0x46;        assert(!a68::IsAulaMini60DeviceInfo(other.data()));   // vid
        std::array<std::uint8_t, a68::kDeviceInfoBytes> zero{};
        assert(!a68::IsAulaMini60DeviceInfo(zero.data()) && !a68::IsAulaMini60DeviceInfo(nullptr));
        // The info reply parses with the ordinary read framing.
        std::array<std::uint8_t, a68::kReportBytes> infoReply{};
        infoReply[1] = a68::kReplyPrefix; infoReply[2] = a68::kCmdDeviceInfo; infoReply[3] = 56;
        std::copy(info.begin(), info.end(), infoReply.begin() + 9);
        const auto* parsed = a68::ParseReadReply(a68::kCmdDeviceInfo, 56, 0, infoReply.data(), infoReply.size());
        assert(parsed && a68::IsAulaMini60DeviceInfo(parsed));
    }

    // A read request is the 0xAA frame; its reply is validated and decoded.
    std::array<std::uint8_t, a68::kReportBytes> request{};
    assert(a68::MakeReadRequest(a68::kCmdReadRam, 56, a68::kDepthOffset, request));
    assert(request[0] == 0 && request[1] == 0xaa && request[2] == a68::kCmdReadRam &&
           request[3] == 56 && request[4] == 0x00 && request[5] == 0x02 && request[7] == 1);
    assert(!a68::MakeReadRequest(a68::kCmdReadRam, 0, a68::kDepthOffset, request));
    assert(!a68::MakeReadRequest(a68::kCmdReadRam, 57, a68::kDepthOffset, request));

    std::array<std::uint8_t, a68::kReportBytes> reply{};
    reply[0] = 0; reply[1] = 0x55; reply[2] = a68::kCmdReadRam;
    reply[3] = 56; reply[4] = 0x00; reply[5] = 0x02;
    reply[9] = 93; reply[11] = 170;  // slot0=93, slot1=170, slot2=0 (released)
    const auto* payload = a68::ParseReadReply(a68::kCmdReadRam, 56, a68::kDepthOffset,
                                              reply.data(), reply.size());
    assert(payload && a68::SlotStroke(payload, 0) == 93 && a68::SlotStroke(payload, 1) == 170 &&
           a68::SlotStroke(payload, 2) == 0);
    assert(!a68::ParseReadReply(a68::kCmdReadRam, 56, 0x0238, reply.data(), reply.size()));
    assert(!a68::ParseReadReply(a68::kCmdReadFlash, 56, a68::kDepthOffset, reply.data(), reply.size()));
    assert(!a68::ParseReadReply(a68::kCmdReadRam, 56, a68::kDepthOffset, reply.data(), 40));

    // The depth chunk plan reads every slot once, contiguously.
    std::size_t covered = 0, last = 0;
    for (const auto& chunk : a68::kDepthChunks) {
        assert(chunk.firstSlot == last);
        covered += chunk.length / 2u;
        last = chunk.firstSlot + chunk.length / 2u;
    }
    assert(covered == a68::kSlots && last == a68::kSlots);

    std::puts("REDSQUARE_ALUMIX68_PROTOCOL=PASS identity slots fingerprint travel request reply chunks");
    return 0;
}
