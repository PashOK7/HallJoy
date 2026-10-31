#include "sparklink_model_profiles.h"
#include "sparklink_key_codes.h"
#include "support_notice_catalog.h"
#include "keyboard_support_status.h"
#include "aula_win60he_protocol.h"
#include <algorithm>
#include <cassert>
#include <iostream>
int main(){
 using namespace halljoy;
 assert(!sparklink::ProbeIdentity(0x1038,0x1610));
 assert(!sparklink::ProbeInterface(0x1038,0x1610,0xffc0,1));
 assert(!sparklink::ProbeInterface(0x1038,0x1610,0xffb0,1));
 assert(!sparklink::ProbeInterface(0x1ca6,0x529,0xffc0,1));
 assert(!sparklink::ProbeInterface(0x1ca6,0x529,0xffb0,2));
 assert(!sparklink::ProbeIdentity(0x1ca6,0xffff));
 // Generic admission: the vendor's official identity list, never a bare vendor page.
 assert(std::is_sorted(sparklink::kOfficialIdentities.begin(),sparklink::kOfficialIdentities.end()));
 assert(std::adjacent_find(sparklink::kOfficialIdentities.begin(),sparklink::kOfficialIdentities.end())==sparklink::kOfficialIdentities.end());
 assert(sparklink::kOfficialIdentities.size()==551);
 for(auto id:sparklink::kOfficialIdentities) {
  const unsigned vid=id>>16,pid=id&0xffffu;
  assert(sparklink::OfficialListed(vid,pid));
  // Generic admission is a release switch: off in 1.6.9.1, so only exact models are admitted.
  assert(sparklink::ProbeIdentity(vid,pid)==(sparklink::ExactModel(vid,pid)||sparklink::kGenericAdmission));
  assert(sparklink::ProbeInterface(vid,pid,0xffb0,1)==sparklink::ProbeIdentity(vid,pid) && !sparklink::ProbeInterface(vid,pid,0xffb0,2) && !sparklink::ProbeInterface(vid,pid,0xffa0,1));
  assert(sparklink::GenericOnly(vid,pid)==(sparklink::kGenericAdmission && !sparklink::ExactModel(vid,pid)));
  assert(vid!=0x3151); // shared with RongYuan / Attack Shark boards that use another protocol
  assert(!aula_win60he::FindKnownUsbIdentity(static_cast<std::uint16_t>(vid),static_cast<std::uint16_t>(pid)));
 }
 assert(!sparklink::OfficialListed(0x3151,0x4010) && !sparklink::ProbeIdentity(0x3151,0x4010));
 assert(sparklink::OfficialListed(0x1ca6,0x1a0f) && sparklink::GenericOnly(0x1ca6,0x1a0f)==sparklink::kGenericAdmission && !sparklink::ExactModel(0x1ca6,0x1a0f)); // IPI Flash75 Pro candidate
 // Supported AULA AG75: every isAG75 PID is official-listed and exact (no generic notice).
 for(unsigned pid:{0x28deu,0x28dfu,0x28e0u,0x28e1u,0x28f0u,0x28f1u,0x28f2u})
  assert(sparklink::OfficialListed(0x38a6,pid) && sparklink::ExactModel(0x38a6,pid) && !sparklink::GenericOnly(0x38a6,pid) &&
         !sparklink::ExperimentalToken(0x38a6,pid,0xffb0) && sparklink::ProbeInterface(0x38a6,pid,0xffb0,1));
 assert(!sparklink::ExactModel(0x38a6,0x28dd) && sparklink::GenericOnly(0x38a6,0x28dd)==sparklink::kGenericAdmission);
 assert(!sparklink::ExactModel(0x1ca6,0x28de));
 assert(sparklink::ExactModel(0x1ca6,0x3002) && !sparklink::GenericOnly(0x1ca6,0x3002)); // Supported SU75 Pro
 assert(sparklink::OfficialListed(0x1ca6,0x5a01) && sparklink::ExactModel(0x1ca6,0x5a01) && !sparklink::GenericOnly(0x1ca6,0x5a01) && !sparklink::ExperimentalToken(0x1ca6,0x5a01,0xffb0)); // Supported Weikav EG75 HE
 assert(sparklink::ExactModel(0x1ca6,0x529) && !sparklink::GenericOnly(0x1ca6,0x529));
 assert(sparklink::ExactModel(0x1ca6,0x528) && !sparklink::OfficialListed(0x1ca6,0x528) && sparklink::ProbeIdentity(0x1ca6,0x528));
 assert(!sparklink::GenericOnly(0x1ca6,0xffff) && !sparklink::GenericOnly(0x1038,0x1614) && !sparklink::ProbeIdentity(0x1038,0x1614));
 assert(!sparklink::OfficialListed(0x10000u,0x3002u));
 assert(sparklink::ProbeInterface(0x1ca6,0x529,0xffb0,1));
 for(unsigned pid=0;pid<65536;++pid)if(sparklink::ExperimentalToken(0x1ca6,pid,0xffb0))assert(sparklink::ProbeInterface(0x1ca6,pid,0xffb0,1));

 for(unsigned pid:{0x528u,0x52au,0x52bu,0x52du,0x52cu,0x531u,0x540u}) {
  auto token=sparklink::ExperimentalToken(0x1ca6,pid,0xffb0);
  assert(token && keyboard_support::NativeNotice(4,token,true,pid)==keyboard_support::SparkLinkV2);
  assert(!keyboard_support::NativeNotice(4,token,false,pid));
  assert(!sparklink::ExperimentalToken(0x1ca5,pid,0xffb0));
  assert(!sparklink::ExperimentalToken(0x1ca6,pid,0xffa0));
 }
 assert(!sparklink::ExperimentalToken(0x1ca6,0x529,0xffb0)); // confirmed MG75 Max
 assert(!sparklink::ExperimentalToken(0x1ca6,0x3002,0xffb0)); // Supported SU75 Pro
 assert(sparklink::ProbeInterface(0x1ca6,0x3002,0xffb0,1));
 assert(!sparklink::ProbeInterface(0x1ca6,0x3002,0xffa0,1));
 assert(!keyboard_support::NativeNotice(4,0,true));
 assert(sparklink::DecodeKey(0xf101)==0x409 && sparklink::DecodeKey(0xf102)==0);
 for(unsigned key=4;key<256;++key)assert(sparklink::DecodeKey(key)==key);
 std::cout<<"SPARKLINK_MODEL_PROFILES=PASS exact identities official list generic admission notices confirmed-model exclusion Fn\n";
}
