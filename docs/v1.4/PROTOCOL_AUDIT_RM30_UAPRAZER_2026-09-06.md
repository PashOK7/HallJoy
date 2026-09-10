# RM-30-UAPRAZER — embedded UAP Razer protocol review

Date: 2026-09-06

## Current admission boundary

The pinned UAP names only five exact Razer products: Huntsman V2 Analog
(`1532:0266`), Mini Analog (`0282`), and V3 Pro / TKL / Mini (`02A6`, `02A7`,
`02B0`). Discovery requires the analogue report ID declared on the selected
interface (`07` for V2, `0B` for V3) and, on Windows, a running recognised
Synapse process. The dormant feature-report mode-switch code remains compiled
out because enabling it can affect ordinary Razer input and lacks project-owned
hardware evidence.

V2 reports decode pairs; V3 reports decode three-byte records with one
unclassified byte. Both parsers map only known Razer scan codes, clamp values
through the common device snapshot path and declare disconnect after ten empty
reports.

## Evidence boundary retained

The code has descriptor-level report-ID admission, but the parser does not
independently prove that every received frame is the selected analogue report
before it consumes records. Nor can source review prove the lifecycle where
Synapse starts, stops or changes mode after discovery. Sending a guessed mode
command or treating a nonmatching frame as a device fault would change a
working vendor-software-dependent path without wire evidence.

Accordingly no speculative change was made. Physical capture must establish the
actual frame-ID/multiplexing behavior and Synapse transition behavior before a
stricter runtime filter can be added. No executable or HID command was run in
this review.
