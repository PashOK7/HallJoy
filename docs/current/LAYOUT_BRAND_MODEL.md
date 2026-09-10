# Layout: Brand / Model

## 2026-09-10: Input Overlay follow belongs to Brand

Owner requested Same as main layout as the first Brand option, not a Model row.
The follow sentinel is an empty internal brand name, separate from manufacturer
names. Choosing it immediately restores existing overlay follow mode (-1/empty
saved layout name), queues the normal settings save, and removes both Model and
Variant labels/retained controls. Brand is wide enough for the full follow caption.
Choosing a manufacturer or All shows Model again and only browses; follow remains
active until a concrete model is selected. Main/editor pickers have no follow item.
No settings migration, extra timers, or analog/backend changes.

Production event tests cover ISO -> follow via Brand, hidden controls absent from
retained hit-test items, refresh restoring Brand, and follow -> All without an
implicit layout change. Existing settings roundtrip tests cover persistence.
Backup: .local/backups/overlay-follow-brand-20260910/.
Historical Model-row descriptions below are superseded by this section.

## 2026-09-09 follow-up: conditional Variant picker

Global settings, Input Overlay and Layout Editor now share Brand / Model / Variant.
ANSI, ISO and JIS terminal suffixes are grouped within the same manufacturer/model;
Custom and Other names stay opaque. Model shows the base name (including the brand
in All). Variant appears only when the selected model has multiple concrete layouts,
and contains only those available variants, in ANSI / ISO / JIS order. A75 Pro
remains distinct from A75. Browsing a brand never changes the active layout.

Switching model preserves the previous physical variant if available, otherwise
uses the first available variant. Existing saved full preset names, file identities,
geometry and first-run identification are unchanged. Overlay follow hides Variant;
explicit overlay choices remain independent of the main preview. No new settings,
timers, polling, or user-file migration.

Editor multi-variant deletion is on each Variant popup row, with the existing inline
confirmation; it cannot delete the whole model group. Single-layout models retain
their Model-row deletion. Variant switches use the same Save/Discard/Cancel gate;
cancellation restores the concrete preset and both pickers. Same-count catalog
mutations invalidate both row maps. Layout placement/hit testing updates on changes.

Validation: simulator/native builds and complete static audits PASS (521 inventory
records). Production-linked private-desktop tests cover every catalog variant,
All labels, independent overlay selection/follow, editor cancellation and confirmed
single-variant deletion using disposable presets, plus existing persistence tests.
Final repeated suite evidence: `HJProfileTest-22c69290778748638a5ae0aa81c09264`
and `HJProfileTest-4958620333a94bc4a41c3d627945fc90` under Windows Temp. An earlier
overlay event suite returned false without a localized assertion; after adding
specific variant failure diagnostics, both complete reruns passed. No visual or
physical-keyboard validation claimed. Existing optional ViGEm PDB warning unchanged.

Backup: `.local/backups/layout-variants-20260909/` (sources, prior documentation,
known-good release EXE). Delivered EXE SHA256:
`A719CE8C5C96B32459B256A688C9FC06FB4973E33A39305D7F7F5E48BF9D07CF`.

The historical descriptions below refer to the earlier two-picker implementation.

2026-09-09. Owner-approved replacement for the flat catalog in Global settings,
Input Overlay and the layout editor. All three use `layout_picker.h` with the
existing PremiumCombo renderer and fonts. No extra timer or polling was added.

## Contract

### Dropdown scrollbar fix (2026-09-09)

The old scrollbar was paint-only: mouse-down selected the underlying row and
closed the popup. PremiumCombo now shares track/thumb/lane geometry between
painting and hit testing. The scrollbar lane cannot select or invoke row actions.
Thumb drag uses the dropdown's existing combo capture, keeps the grab offset,
clamps outside the popup, and never commits selection on release. Track clicks
page without closing; wheel input is ignored during a thumb drag. Closing,
capture loss and cancellation clear dragging. No polling or extra timers.

Regression coverage in the private-desktop layout picker test exercises a real
popup: scrollbar/row exclusion, dragging to both limits outside the window,
release without selection, track paging and cancellation. Backup of sources and
previous release: `.local/backups/combo-scroll-20260909/`.

Validation: simulator/native builds and static audits PASS; production-linked
suite PASS at `%TEMP%/HJProfileTest-0a3fd0df65534056a4f5e022a2a3af93`.
The first two attempts were blocked by the running release's single-instance
guard, not test assertions. Close the release before running this suite.
Release SHA256: `3E2CAA4AB1FB2A060A9959DBE294EB5D14F29838003CA7D9CBFB77E4D3E8461D`.

### Catalog behavior

- All is the first Brand entry and lists every preset by its full display name,
  including the manufacturer prefix. It stays selected across model selection
  and refresh in all three consumers. It is a view, not a stored manufacturer:
  editor creation from All assigns Custom. Other brand views keep short labels.
- Keychron's historical ` - Imported` suffix is hidden in display names only.
  Stable preset/file/settings identities are preserved; no file migration.
- Section heading Layout, controls Brand and Model. Manufacturers are sorted;
  Other contains WASD Only and Generic; Custom contains unclassified user layouts.
- Browsing a brand only filters models. It does not change the main/overlay
  selection or the editor draft, save settings, or discard unsaved work.
  An empty selection says Choose model. Ordinary settings refresh preserves it.
- Model selection uses an explicit row-to-catalog map. Create and overlay-follow
  are distinct sentinels. Delete confirmation still lives inside the model popup.
- Catalog revision invalidates stale maps even after create/delete with unchanged
  final count. Stale rows cannot select/delete an unrelated model.
- Overlay retains Same as main layout as the first Model entry; no brand choice
  is necessary when following. Main and overlay continue saving stable full names.
- Editor retains its Save/Discard/Cancel gate. Choosing a different brand alone
  does not prompt; choosing another model resolves the draft. Creating a layout
  copies the current draft's saved source and assigns the browsed category.

## Storage / compatibility

### Natural model ordering (2026-09-09 follow-up)

The initial implementation sorted brands only; models accidentally followed
built-in insertion order. `layout_sort.h` now orders the display entries naturally
(K2, K3, K4, K8, K10; Q1, Q3, Q5, Q6, Q12). Region suffixes are separated from
the base: ordinary HE ANSI/ISO/JIS, then HE 8K ANSI/ISO/JIS for each model.
All uses the full display name, so manufacturers remain grouped. ASCII letter
case is ignored; numeric runs are compared without integer conversion/overflow.
Equal sort keys preserve catalog order. Follow stays first, Create stays last.

Only picker entries are sorted, with labels computed once per list rebuild.
The catalog, filenames, IDs, saved selection and geometry are unchanged. Shared
controller applies this in Global settings, Overlay and Layout Editor. No extra
polling/timers. Existing row-to-preset mapping is retained for deletion/selection.

PASS: natural order/large-number/strict-order C++ test, production Windows picker
and editor events (`HJProfileTest-c45ab14e38dc4a1f9f5a3d5d42ed7376`), static suite,
native/simulator builds. Backup: `.local/backups/layout-sort-20260909-194707/`.
Release SHA256: `D85C79FD74C375E2A2EAA44503A5C33276A5AC034CC95570631F4DC81DFCDC1B`.

`[LayoutPreset] Brand` is optional metadata, validated with the existing atomic
save transaction. Built-ins declare their brand explicitly. Legacy files receive
the matching built-in's category only on exact known identity; arbitrary names
are not guessed and default to Custom. No file rename or geometry rewrite.
New importer output carries Brand=Keychron (this is the Keychron-only adapter).
Model display removes the matching brand prefix and the imported display suffix;
the underlying full identity and first-run VID/PID mapping remain unchanged.

## Validation

- Release and AnalogSimulator x64 builds; existing optional ViGEm PDB warning only.
- Complete static suite and 16 importer tests.
- Production-linked events on an undisplayed private desktop with backend init
  forbidden: filtering, uncommitted brand browsing across settings refresh,
  independent overlay selection/follow, create/delete with filtered row indices,
  dirty editor cancellation, stale-map protection after same-count mutation,
  metadata file roundtrip and legacy Custom fallback. Existing persistence and
  first-run tests also pass. No visual inspection or physical-input tests.
- Final evidence (including All row coverage/full labels/selection/refresh):
  `%TEMP%/HJProfileTest-7461b16560ca4d29b062e3460d530336`.
- Delivered `build/release/HallJoy.exe`, SHA256:
  `391D182CFD09E263017CBD03560F0AC96EAE008B201FDC124A93DB512D525416`.

Backup: `.local/backups/layout-brand-model-20260909-175151/` contains the previous
production source directory and known-good release EXE. Runtime profiles/layouts
were not modified by tests; they use a private test data root.
Pre-All EXE and changed production files: `.local/backups/layout-all-20260909/`.
