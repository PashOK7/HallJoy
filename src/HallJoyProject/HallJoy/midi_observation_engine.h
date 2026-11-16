#pragma once
#include "analog_observation.h"
#include "midi_engine.h"
#include <bitset>

namespace halljoy::midi {
// Physical contributions remain independent even when two devices/positions have
// the same HID or note. Only final note aliases are deliberately combined.
class ObservationEngine {
    static constexpr unsigned kSlots=2048,kNone=kSlots;
    struct Voice {
        std::uint64_t source=0,connection=0,seen=0,lastUs=0;
        std::uint16_t physical=0,hid=0,code=0,next=kNone;
        AttackState attack{};
        int depth=0;
        bool seeded=false;
        observation::Event baseline{};
    };
    std::array<Voice,kSlots> voices_{};
    std::array<std::uint16_t,kAddressCount> heads_{};
    static constexpr unsigned kTable=4096,kDeleted=kSlots+1;
    std::array<std::uint16_t,kTable> lookup_{};
    std::array<Note,kAddressCount> notes_{};
    std::bitset<keycode::kCount> covered_;
    std::uint64_t frame_=0,sparse_=0,invalid_=0,capacityLoss_=0;
    unsigned used_=0;
    TouchReport touch_{};
    std::unique_ptr<std::array<MotionTrace,kSlots>> traces_;
    bool snapshotRoutingDeferred_=false;
    void Unlink(unsigned index) noexcept {
        auto& v=voices_[index];if(!v.code) return;
        auto* link=&heads_[v.code-1];
        while(*link!=kNone) {if(*link==index){*link=v.next;break;}link=&voices_[*link].next;}
    }
    static unsigned Hash(std::uint64_t source,std::uint64_t connection,unsigned physical) noexcept {
        const auto value=(source*0x9E3779B97F4A7C15ull) ^ (connection*0xBF58476D1CE4E5B9ull) ^ (std::uint64_t(physical)*0x94D049BB133111EBull);
        return static_cast<unsigned>((value^(value>>32))&(kTable-1));
    }
    unsigned Find(const observation::Event& e) const noexcept {
        const auto hash=Hash(e.source,e.connection,e.physical);
        for(unsigned step=0;step<kTable;++step) {
            const auto index=lookup_[(hash+step)&(kTable-1)];
            if(index==kNone) break;
            if(index==kDeleted) continue;
            const auto& v=voices_[index];
            if(v.source==e.source && v.connection==e.connection && v.physical==e.physical) return index;
        }
        return kNone;
    }
    void Index(unsigned i) noexcept {
        const auto& v=voices_[i];const auto hash=Hash(v.source,v.connection,v.physical);
        for(unsigned step=0;step<kTable;++step) {
            auto& entry=lookup_[(hash+step)&(kTable-1)];
            if(entry==kNone || entry==kDeleted){entry=static_cast<std::uint16_t>(i);return;}
        }
    }
    void RemoveIndex(unsigned i) noexcept {
        const auto& v=voices_[i];const auto hash=Hash(v.source,v.connection,v.physical);
        for(unsigned step=0;step<kTable;++step) {
            auto& entry=lookup_[(hash+step)&(kTable-1)];
            if(entry==i){entry=kDeleted;return;}
            if(entry==kNone) return;
        }
    }
    template<class Emit> void Render(unsigned code,const Profile& config,Emit&& emit) {
        if(!code) return;
        const unsigned a=code-1;Note desired{};
        for(unsigned i=heads_[a];i!=kNone;i=voices_[i].next) {
            const auto& v=voices_[i];if(!v.attack.down || !v.attack.velocity) continue;
            desired.velocity=std::max(desired.velocity,v.attack.velocity);
            desired.pressure=std::max(desired.pressure,static_cast<std::uint8_t>((v.depth*127+500)/1000));
        }
        const auto old=notes_[a];
        if(old.velocity && !desired.velocity) emit(Message(0x80,a,0));
        if(desired.velocity && !old.velocity) emit(Message(0x90,a,desired.velocity));
        if(old.velocity && desired.velocity) desired.velocity=old.velocity;
        if(config.depthPressure && desired.velocity && (!old.velocity || old.pressure!=desired.pressure)) emit(Message(0xA0,a,desired.pressure));
        notes_[a]=desired;
    }
    template<class Emit> void Apply(Voice& v,const observation::Event& e,const Profile& config,Emit&& emit,
                                    AttackSink diagnostics) {
        if(e.milli>1000 || (e.timeUs && v.lastUs>e.timeUs)) {
            ++invalid_;v.attack={};v.seeded=true;
            if(traces_)(*traces_)[static_cast<unsigned>(&v-voices_.data())]={};
            // A timestamp failure must be rearmed by a received release, not
            // by a later route snapshot or an unknown previous-sample time.
            Render(v.code,config,emit);return;
        }
        auto* trace=traces_?&(*traces_)[static_cast<unsigned>(&v-voices_.data())]:nullptr;
        v.depth=e.milli;
        const bool wasDown = v.attack.down;
        ObserveAttack(v.attack, config, e.timeUs,
            {int(e.milli), e.available, e.timing == observation::Timing::HostSnapshot, true,
                e.previousValid && e.milli==e.previous && e.timeUs!=0}, sparse_,trace);
        if (!wasDown && v.attack.down) {
            RecordTouch(touch_, v.attack);
            if(trace)diagnostics(v.attack,*trace,config,{e.source,e.connection,e.sequence,e.physical,e.hid,v.code});
        }
        v.lastUs=e.timeUs;Render(v.code,config,emit);
    }
public:
    ObservationEngine() {heads_.fill(kNone);lookup_.fill(kNone);}
    void EnableDiagnostics() {traces_=std::make_unique<std::array<MotionTrace,kSlots>>();}
    const auto& Notes() const noexcept {return notes_;}
    std::uint64_t SparseAttacks() const noexcept {return sparse_;}
    const TouchReport& Touch() const noexcept { return touch_; }
    std::uint64_t Invalid() const noexcept {return invalid_;}
    std::uint64_t CapacityLoss() const noexcept {return capacityLoss_;}
    const auto& Covered() const noexcept {return covered_;}
    template<class Emit> void Reset(Emit&& emit) {
        for(unsigned a=0;a<notes_.size();++a)if(notes_[a].velocity)emit(Message(0x80,a,0));
        notes_.fill({});voices_.fill({});heads_.fill(kNone);lookup_.fill(kNone);used_=0;covered_.reset();
        if(traces_)traces_->fill({});
    }
    void BeginFrame() {++frame_;covered_.reset();snapshotRoutingDeferred_=false;}
    template<class Emit> void Route(const observation::Event& e,const Profile& config,Emit&& emit) {
        if(e.kind==observation::Kind::RetainSnapshot) {
            snapshotRoutingDeferred_=true;
            for(unsigned i=0;i<used_;++i) if(voices_[i].source && voices_[i].baseline.timing==observation::Timing::HostSnapshot) voices_[i].seen=frame_;
            return;
        }
        if(!e.source || !e.connection || !e.physical || !keycode::IsSupported(e.hid)) return;
        covered_.set(e.hid);const auto code=config.keys[e.hid];if(!code || code>kAddressCount) return;
        unsigned i=Find(e);
        if(i==kNone) {
            if(!e.available) return; // Missing/unowned keys consume no voice capacity.
            for(unsigned candidate=0;candidate<used_;++candidate) if(!voices_[candidate].source) {i=candidate;break;}
            if(i==kNone) {if(used_==kSlots) {++capacityLoss_;return;} i=used_++;}
            if(traces_)(*traces_)[i]={};
            auto& v=voices_[i];v.source=e.source;v.connection=e.connection;
            v.physical=e.physical;v.hid=e.hid;v.code=code;v.next=heads_[code-1];heads_[code-1]=static_cast<std::uint16_t>(i);Index(i);
        }
        auto& v=voices_[i];
        if(v.hid!=e.hid || v.code!=code) {
            // Dynamic assignment change must not reinterpret a sounding voice.
            v.attack={};v.seeded=false;Render(v.code,config,emit);Unlink(i);
            if(traces_)(*traces_)[i]={};
            v.hid=e.hid;v.code=code;v.lastUs=0;v.next=heads_[code-1];heads_[code-1]=static_cast<std::uint16_t>(i);
        }
        v.seen=frame_;v.baseline=e;
    }
    template<class Emit> void Replay(const observation::Event& e,const Profile& config,Emit&& emit,
                                     AttackSink diagnostics = {}) {
        if(e.kind==observation::Kind::Disconnect) {
            for(unsigned i=0;i<used_;++i)if(voices_[i].source==e.source && voices_[i].connection==e.connection) {
                voices_[i].attack={};voices_[i].seeded=false;Render(voices_[i].code,config,emit);
                if(traces_)(*traces_)[i]={};
            }
            return;
        }
        auto index=Find(e);
        if(index==kNone && snapshotRoutingDeferred_ && e.timing==observation::Timing::HostSnapshot) {
            // The host was proved alive but its baseline view is being written.
            // A captured, validated per-device snapshot sample is itself a route
            // proof; do not silently lose a new device's complete short press.
            Route(e,config,emit);index=Find(e);
        }
        if(index==kNone) return;
        auto& v=voices_[index];if(v.seen!=frame_ || e.hid!=v.hid) return;
        if(!v.seeded) {
            // A previously observed released value can arm the first captured edge.
            // Its earlier timestamp is unknown: this first attack remains sparse.
            if(e.previousValid) SeedReleased(v.attack, config, e.previous);
            v.seeded=true;
        }
        Apply(v,e,config,emit,diagnostics);
    }
    template<class Emit> void EndFrame(const Profile& config,Emit&& emit) {
        for(unsigned i=0;i<used_;++i) {
            auto& v=voices_[i];
            if(v.seen!=frame_) {
                v.attack={};Render(v.code,config,emit);Unlink(i);RemoveIndex(i);v={};
            } else if(!v.baseline.available) {
                v.attack={};v.seeded=false;Render(v.code,config,emit);
                if(traces_)(*traces_)[i]={};
            } else if(!v.seeded) {
                // Baselines are state only. A held key never creates an attack.
                SeedReleased(v.attack, config, v.baseline.milli);
                v.seeded=true;
            }
        }
    }
};
}
