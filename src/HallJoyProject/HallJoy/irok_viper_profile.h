#pragma once
#include "irok_nd75_protocol.h"
#include <cstring>
#include <algorithm>
#include <string_view>

namespace irok_viper {
// Official Viper V2.06.06, KeyInfo_GK8163HERGB primary six physical rows.
inline constexpr irok_nd75::PositionToHid kFactoryMap{
    0x29, 0x00, 0x3a, 0x3b, 0x3c, 0x3d, 0x00, 0x3e, 0x3f, 0x40, 0x41, 0x42, 0x43, 0x44, 0x45, 0x46, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x35, 0x1e, 0x1f, 0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27, 0x2d, 0x2e, 0x00, 0x2a, 0x00, 0x00, 0x4c, 0x00, 0x00, 0x00, 0x00,
    0x2b, 0x14, 0x1a, 0x08, 0x15, 0x17, 0x1c, 0x18, 0x0c, 0x12, 0x13, 0x2f, 0x30, 0x00, 0x31, 0x00, 0x00, 0x4d, 0x00, 0x00, 0x00, 0x00,
    0x39, 0x00, 0x04, 0x16, 0x07, 0x09, 0x0a, 0x0b, 0x0d, 0x0e, 0x0f, 0x33, 0x34, 0x00, 0x28, 0x00, 0x00, 0x4b, 0x00, 0x00, 0x00, 0x00,
    0xe1, 0x00, 0x1d, 0x1b, 0x06, 0x19, 0x05, 0x11, 0x10, 0x36, 0x37, 0x38, 0x00, 0x00, 0xe5, 0x00, 0x52, 0x4e, 0x00, 0x00, 0x00, 0x00,
    0xe0, 0xe3, 0xe2, 0x00, 0x00, 0x00, 0x2c, 0x00, 0x00, 0x00, 0xe6, 0xfa, 0x00, 0xe4, 0x00, 0x50, 0x51, 0x4f, 0x00, 0x00, 0x00, 0x00,
};

inline bool Matches(const irok_nd75::DeviceInfo& identity) noexcept
{
    std::string_view controller(identity.controller.data());
    std::string_view product(identity.product.data());
    while (!controller.empty() && controller.back() == ' ') controller.remove_suffix(1);
    while (!product.empty() && product.back() == ' ') product.remove_suffix(1);
    return controller == "M484" && product == "GK8163HERGB";
}

inline irok_nd75::Report RawRequest(bool enabled) noexcept
{
    irok_nd75::Report report{1, 0x23};
    report[6] = enabled ? 1 : 0;
    return report;
}

// Firmware 1.13.06 initializer 0x8c66 sets minimum=150 and baseline=0.
// Saved calibration is NOT read. The observed maximum below is a provisional
// relative reference, not proof of release. A held first sample is unobservable
// until more travel is observed. This is not calibrated millimetre measurement.
class MatrixRange {
    struct Channel { unsigned reference=0, minimum=65535; bool seen=false, departed=false; };
    std::array<Channel,132> channels_{};
public:
    unsigned referenceRises=0, referenceReturns=0, invalid=0;
    unsigned ready=0, fullChannels=0, minReference=0, maxReference=0;
    unsigned minObserved=0, maxObserved=0;
    bool Sample(unsigned position,unsigned raw,unsigned& milli) noexcept {
        milli=0;
        if(position>=132 || !kFactoryMap[position]) return false;
        const unsigned value=raw>>3;
        // Actual scanner rejects scaled ADC >400. Never fabricate its depth.
        if(value>400) {++invalid;return false;}
        auto& c=channels_[position];
        if(!c.seen) {c.seen=true;c.reference=value;}
        else if(value>c.reference) {c.reference=value;++referenceRises;}
        if(value<c.minimum)c.minimum=value;
        if(value<c.reference)c.departed=true;
        else if(c.departed) {++referenceReturns;c.departed=false;}
        if(c.reference<190)return false; // Actual stock divisor would be zero.
        const auto denominator=(c.reference-150)/40;
        const auto depth=std::min(40u,((c.reference-value)*6/5)/denominator);
        milli=depth*25;
        return true;
    }
    unsigned Reference(unsigned position) const noexcept { return position<132?channels_[position].reference:0; }
    void Inspect() noexcept {
        ready=fullChannels=maxReference=maxObserved=0;minReference=minObserved=65535;
        for(unsigned p=0;p<132;++p) {
            if(!kFactoryMap[p] || !channels_[p].seen)continue;
            const auto& c=channels_[p];
            minReference=std::min(minReference,c.reference);maxReference=std::max(maxReference,c.reference);
            minObserved=std::min(minObserved,c.minimum);maxObserved=std::max(maxObserved,c.reference);
            if(c.reference<190)continue;
            ++ready;const auto denominator=(c.reference-150)/40;
            fullChannels+=((c.reference-c.minimum)*6/5)/denominator>=40;
        }
        if(minReference==65535)minReference=0;
        if(minObserved==65535)minObserved=0;
    }
};

// Raw ADC observations diagnose firmware event loss. They never synthesize
// depth or release, and no startup sensor value is assumed to be released.
class RawEvidence {
    std::array<std::uint16_t, 132> previous_{};
    std::array<bool, 132> seen_{};
    std::array<unsigned, 132> eventSequence_{};
    std::array<unsigned, 132> sequenceAtRow_{};
    std::array<unsigned, 132> depth_{};
    std::array<bool, 132> depthKnown_{}, referenceKnown_{}, referenceMatched_{};
    std::array<std::uint16_t, 132> zeroReference_{};
public:
    unsigned rows = 0, frames = 0, rejected = 0;
    unsigned changes = 0, changesWithoutEvent = 0;
    unsigned rawMinimum=65535, rawMaximum=0;
    unsigned zeroReferences = 0, zeroReferenceReturns = 0;
    void Event(std::size_t position, unsigned depth) noexcept
    {
        ++eventSequence_[position];
        depthKnown_[position] = true;
        depth_[position] = depth;
    }
    bool Feed(const irok_nd75::Report& report) noexcept
    {
        if (report[0] != 1 || report[1] != 0x23 || report[2] || report[3] ||
            report[4] < 1 || report[4] > 6 || report[5] != 44) {
            ++rejected;
            return false;
        }
        const unsigned row = report[4] - 1;
        rows |= 1u << row;
        ++frames;
        for (unsigned column = 0; column < 22; ++column) {
            const auto position = row * 22 + column;
            if (!kFactoryMap[position]) continue;
            const auto value = static_cast<std::uint16_t>((report[6+column*2] << 8) | report[7+column*2]);
            rawMinimum=std::min(rawMinimum,unsigned(value));rawMaximum=std::max(rawMaximum,unsigned(value));
            if (seen_[position] && previous_[position] != value) {
                ++changes;
                changesWithoutEvent += sequenceAtRow_[position] == eventSequence_[position];
            }
            if (depthKnown_[position] && depth_[position] == 0 && value != 0) {
                if (!referenceKnown_[position]) ++zeroReferences;
                referenceKnown_[position] = true;
                zeroReference_[position] = value;
            }
            // A measured return to a previous zero-associated ADC value is
            // evidence only. Quantization, hysteresis and report ordering can
            // also cause this match, so it must never clear an analog key.
            const bool matched = referenceKnown_[position] && depthKnown_[position] &&
                depth_[position] != 0 && zeroReference_[position] == value;
            if (matched && !referenceMatched_[position]) ++zeroReferenceReturns;
            referenceMatched_[position] = matched;
            seen_[position] = true;
            previous_[position] = value;
            sequenceAtRow_[position] = eventSequence_[position];
        }
        return true;
    }
};
}
