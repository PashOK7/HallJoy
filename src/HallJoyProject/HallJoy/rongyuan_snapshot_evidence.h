#pragma once
// Included inside the backend's private namespace. No hardware work on log/UI threads.
struct SnapshotEvidence {
  unsigned board=0, pid=0, usb=0, rf=0, units=0, mapped=0, pageMask=0;
  unsigned maxActive=0, peak=0, metadata=0, layout=0;
  std::uint64_t samples=0, releases=0, changes=0, sweeps=0, rejected=0;
  std::uint64_t revision=0;
  std::array<unsigned,128> last{}, maximum{};
  std::array<bool,128> seen{}, positive{}, released{};
  std::array<std::uint64_t,4> pages{};
};
std::mutex g_evidenceLock;
SnapshotEvidence g_evidence;
void Record(const char* stage, const char* reason, unsigned command=0,
            unsigned page=0, unsigned error=0, unsigned observed=0) {
  char line[512]{};
  sprintf_s(line,"evidence.rongyuan_snapshot.stage stage=%s reason=%s command=%u page=%u win32=%u observed=%u stop=%u",
            stage,reason,command,page,error,observed,g_stop.load()?1:0);
  SupportLog_Evidence(line,false);
}
void WriteSnapshotEvidence() noexcept {
  SnapshotEvidence e;
  { std::lock_guard<std::mutex> lock(g_evidenceLock); e=g_evidence; }
  if(!e.board && !e.revision) return;
  unsigned seen=0, positive=0, returned=0, varying=0, active=0;
  for(unsigned i=0;i<128;++i) {
    seen+=e.seen[i];positive+=e.positive[i];returned+=e.released[i];
    varying+=e.positive[i] && e.last[i]!=e.maximum[i];active+=e.last[i]>0;
  }
  const auto* model=mg::Find(e.board);
  char line[384]{};
  sprintf_s(line,"evidence.rongyuan_snapshot.current revision=%llu board=%u pid=%u usb=%u rf_version_queried=0 units_per_mm=%u range_um=%u range_basis=%s matrix_sampling=paged_not_atomic calibrated_by_host=0 mapped=%u metadata=%u layout_cache=%u connected=%u",
    e.revision,e.board,e.pid,e.usb,e.units,model?model->rangeUm:0,
    e.board==3113?"retained_hot_swap_reference":"manufacturer_stock",e.mapped,e.metadata,e.layout,
    (g_connected.load() && g_board.load()==e.board)?1:0);
  SupportLog_Evidence(line,false);
  sprintf_s(line,"evidence.rongyuan_snapshot.coverage board=%u page_mask=%u pages=%llu,%llu,%llu,%llu sweeps=%llu samples=%llu rejected=%llu observed_channels=%u",
    e.board,e.pageMask,e.pages[0],e.pages[1],e.pages[2],e.pages[3],e.sweeps,e.samples,e.rejected,seen);
  SupportLog_Evidence(line,false);
  sprintf_s(line,"evidence.rongyuan_snapshot.travel board=%u positive_channels=%u released_channels=%u changes=%llu releases=%llu active=%u max_simultaneous=%u peak_depth_units=%u varying_channels=%u release_observation_pending=%u",
    e.board,positive,returned,e.changes,e.releases,active,e.maxActive,e.peak,varying,
    (!positive || positive>returned)?1:0);
  SupportLog_Evidence(line,false);
}
void ObservePage(unsigned page,const mg::Values& values,const mg::Model& model) {
  std::lock_guard<std::mutex> lock(g_evidenceLock);
  auto& e=g_evidence;++e.samples;++e.pages[page];e.pageMask|=1u<<page;
  if(page==3) ++e.sweeps;
  unsigned active=0;
  for(unsigned i=0;i<32;++i) {
    const auto slot=page*32+i;
    if(!mg::Decode(model.matrix->data()+slot*4)) continue;
    const unsigned v=values[i];
    if(e.seen[slot] && e.last[slot]!=v) ++e.changes;
    if(e.seen[slot] && e.last[slot] && !v) {++e.releases;e.released[slot]=true;}
    e.last[slot]=v;e.seen[slot]=true;e.positive[slot]=e.positive[slot]||v>0;
    e.maximum[slot]=std::max(e.maximum[slot],v);e.peak=std::max(e.peak,v);
  }
  for(unsigned i=0;i<128;++i) active+=e.last[i]>0;
  e.maxActive=std::max(e.maxActive,active);
}
