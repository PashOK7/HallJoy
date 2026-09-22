> Follow-up: owner authorized known-protocol implementation. Four entries are now yellow after code/layout work; this read-only review is the prior checkpoint. See [implementation and Sheet changes](KNOWN_PROTOCOL_ADDITIONS_2026-09-21.md).

# Wooting / Keychron / Razer Sheet review - 2026-09-21

Read-only review requested by owner: whether Not investigated rows can be marked
Supported. Live workbook 1ueQ4labXpuBOmGjCkUcJllNJ68Jzx4py2MuQm-p-R7c, Main sheetId 0,
bounded read A1:C558. No sheet cells, code, EXE or public README changed.

- Wooting rows 538-547: nine Supported; only 80HE+ (542) Not investigated.
  Shipped Soup admits VID31E3 and usages FF54/FF53 without a PID allowlist.
  Therefore a compatible 80HE+ interface can already use the generic reader.
  Exact new hardware/interface and split-key identity are not established here;
  compatibility is plausible, not a verified unconditional Supported addition.
- Razer rows 432-443: five existing Supported; seven Not investigated (two
  Magnetic models, full-size/TKL/low-profile 8KHz revisions, Tartarus Pro/V2 Pro).
  Shipped Soup admits exactly PIDs0266,0282,02A6,02A7,02B0 and corresponding
  report IDs, with Synapse running. There are no explicit routes for the seven
  extra models. Original Tartarus Pro PID0244 (OpenRazer) is not admitted.
  Do not infer compatibility of new revisions from the Huntsman brand alone.
- Keychron rows281-313: fourteen Supported with custom firmware, nineteen
  Not investigated. Existing owner confirmation of custom firmware and its
  preparation tool remains authoritative; do not reopen analog feasibility.
  K6 HE, Q2 HE, Q4 HE, Q2 HE 8K and Q0 HE were already researched: second Fn /
  macro physical mappings and model integration remain unfinished in the current
  catalog record. J12/J14/Q16 HE8K also have prior source search evidence but an
  incomplete exact source set. Not investigated is historically imprecise for
  these entries; Research incomplete is more accurate than a blanket Supported.
  Other newer C/J/V variants were not independently admitted by this review.

Evidence: third_party/UniversalAnalogPluginFixed/overlay/Soup/soup/AnalogueKeyboard.cpp
(checkDeviceName and getActiveKeys), matching cached compiled dependency source;
docs/current/KEYCHRON_COMPOUND_LAYOUTS.md (unfinished models and owner policy).
Official/upstream supporting pages:
https://github.com/AnalogSense/universal-analog-plugin
https://wooting.io/wooting-80he-plus
https://openrazer.github.io/
Official 80HE+ product page establishes a distinct analog model, not HallJoy testing.
No new model-specific hardware test performed. Not investigated is not synonymous
with Unsupported. No blanket status promotion is justified by this evidence.
