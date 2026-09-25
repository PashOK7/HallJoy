#pragma once
#include <cstdint>
namespace halljoy::keyboard_support {
// UI-side observer: no HID calls, key values, logging or background polling.
struct CommunicationHealth {
 bool established=false, previousConnected=false, warning=false;
 std::uint64_t lostAt=0, lastDrop=0, healthyAt=0;
 unsigned drops=0;
 bool Observe(std::uint64_t now,bool present,bool connected,bool anomaly=false) noexcept {
  if(connected) established=true;
  if(anomaly){warning=true;healthyAt=0;}
  if(previousConnected && !connected){
   drops=(drops && now-lastDrop<=30000)?drops+1:1;lastDrop=now;
   lostAt=now;healthyAt=0;
   if(drops>=2)warning=true;
  }
  if(established && present && !connected && lostAt && now-lostAt>=1500)warning=true;
  if(connected && !anomaly){
   if(!healthyAt)healthyAt=now;
   if(now-healthyAt>=15000){warning=false;drops=0;lostAt=0;}
  }else healthyAt=0;
  // A detached keyboard alone is not evidence of another application's access.
  if(!present && !connected && lastDrop && now-lastDrop>30000){*this={};return false;}
  previousConnected=connected;return warning;
 }
};
// Explicit protocol evidence supplements the generic connection observer.
// Sources 1..250 are native protocols; 254/255 reserved for SDK/UAP.
void ReportCommunicationAnomaly(unsigned source) noexcept;
unsigned CommunicationAnomalySequence(unsigned source) noexcept;
}
