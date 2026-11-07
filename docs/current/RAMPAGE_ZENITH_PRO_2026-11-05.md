# Rampage ZENITH PRO and analog pitch control

The manufacturer's V2 listing identifies an 82-key Hall Effect keyboard with
Gateron KS-20U Dual-Rail switches and 4.1 ± 0.2 mm stated mechanical travel.
Segment support lists the wired US version as SKU 42761, barcode 8680096149689.
Its batch 112025 download is `ZENITH_PRO_(_UPDATE_).zip`.
[Manufacturer product](https://www.rampage.com.tr/products/8000-hz-gateron-manyetik-anahtarli-rgb-gasket-oyuncu-klavyesi-siyah)
[Distributor product and batch driver](https://www.desteksegment.com/urun/rampage-zenith-pro-siyah-hot-swap-8000-hz-gasket-manyetik-rgb-rapid-trigger-gateron-switch-us-layout-english-dye-sub-pbt)

## Identity and the existing HallJoy path

Static extraction of the batch installer yielded `index.3ac9bc37.js` (SHA-256
`35b028ea3700334cb7eaf161d6cc5ce291483d6ca38172bd48b569254ab9317a`). Its only
ZENITHPROSoftware device is board 2310, 3151:502D, Common82_SG9000. Its exact
512-byte factory matrix equals the HallJoy profile. Space is standard HID
usage 44 in physical slot 41 (zero-based). This is strong evidence for the
batch driver association. Confirm the board ID on the actual unit before
extending the result to other batches. The older Rampage download includes both
2310 (4.1 mm configurator setting limit) and 2466 (3.6 mm) under the same model
name and USB pair. The configurator labels do not identify a retail unit's board
revision. In the older official driver, both model names dispatch to the same
`XEa` class at character 14,024,069 of `main_0e3bd636.js`. Its `MEa` factory
matrix equals the 512-byte HallJoy matrix for 2310. The shared class inherits
`jsa.getKeyPressTravelValues` at character 13,530,900, which reads four pages of
`E5 FE 01 page` and decodes little-endian 16-bit depth per slot. Its `MXe` base
also implements the `1B` stream toggle. The old configurator displays depth
from the page-read path. The shared toggle does not establish that every 2466
firmware emits the 32-byte report 5 shape used by HallJoy's stream backend.

A likely 2466 integration is an exact board profile on the same USB pair, the
shared factory map and an automatic-layout identity for the verified geometry.
Validate firmware units and effective depth range. The older driver advertises
a 3.6 mm limit for 2466. Confirm the live stream shape and releases before using
the stream path. If the unit exposes reliable depth only through E5, review the
existing snapshot path for this exact identity. The existing 2310 admission
continues to cover its own firmware and matrix.
[Official general downloads](https://www.rampage.com.tr/en/pages/download)

The published HallJoy v1.6.14 source contains the wired 2310 RongYuan profile and
its `RY1B-2310` automatic layout identity. The backend verifies the device's
board response, reads the active key assignments and enables the report 5 /
command 1B stream. It publishes per-key numeric travel, including Space, through
the ordinary bindings to a virtual Xbox controller axis. The factory preset is
`Rampage ZENITH PRO ANSI`, 82 keys. `layout_pipeline.py summary Rampage` passed.
[Release](https://github.com/PashOK7/HallJoy/releases/tag/v1.6.14)
[Admission evidence](RONGYUAN_BATCH_3_2026-09-24.md)

## Purchase and test scope

The matching board already has the requested software path. Exact physical
percentages still require testing. The current profile uses 4 mm provisional
normalization and the game can apply its own axis response. To assess pitch,
assign Space to one direction of a stick axis with a linear response and check
slow movement, hold and release in Gamepad Tester and then in the game. A 2005
title must accept and bind the virtual controller's analog axis. Windows can
expose XUSB controllers to DirectInput. The game title is needed to determine
its input behavior. No Rampage physical-depth or in-game test was located in the
project evidence or the reviewed tester log folders.
[Microsoft XInput and DirectInput comparison](https://learn.microsoft.com/en-us/windows/win32/xinput/xinput-and-directinput)

Live Main!C638 was read on 2026-11-05. Rampage / ZENITH PRO has the yellow
`Implemented; awaiting hardware testing` status and strict validation. The
existing catalog entry accurately covers board 2310. The prospective buyer's
message describes a purchase decision and a desired test, with no hardware
result. Vendor packages and hashes remain under
`.local/research/rampage-zenith-20261105/`. The installers and extracted code
were inspected as data.
