"""Wiring guards supplement the production-linked layout/editor event tests."""
from pathlib import Path

hall = Path(__file__).resolve().parents[1] / "HallJoy"
ui = (hall / "keyboard_subpages.cpp").read_text(encoding="utf-8-sig")
app = (hall / "app.cpp").read_text(encoding="utf-8-sig")
layout = (hall / "keyboard_layout.cpp").read_text(encoding="utf-8-sig")
server = (hall / "overlay_server.cpp").read_text(encoding="utf-8-sig")
settings = (hall / "settings_ini.cpp").read_text(encoding="utf-8-sig")
picker = (hall / "layout_picker.h").read_text(encoding="utf-8-sig")

assert 'KeyboardLayout_GetOverlaySnapshot()' in server
assert 'g_overlaySnapshot.load(std::memory_order_acquire)' in layout
assert 'static std::wstring g_overlayPresetName' in layout
assert 'L"LayoutPresetName"' in settings and 'KeyboardLayout_SetOverlayPresetName(overlayLayoutName)' in settings
assert 'if (follow) brands.insert(brands.begin(), L"")' in picker
assert 'bool Following() const' in picker
assert 'groups.push_back({L"Same as main layout",{-1}})' not in picker
assert 'if (!followingLayout) OverlayCustom_AddItem(st, OVERLAY_ID_LAYOUT' in ui
assert 'KeyboardLayout_SetOverlayPresetIndex(-1)' in ui
assert 'KeyboardLayout_SetOverlayPresetIndex(selected)' in ui
assert 'id == OVERLAY_ID_LAYOUT ? st->layoutPicker.ChooseModel() : st->layoutPicker.Selected()' in ui
assert 'LayoutEditor_OpenWindow(hWnd, KeyboardLayout_GetOverlayPresetIndex())' in ui
assert 'L"Save changes"' in ui and 'L"Discard changes"' in ui and 'L"Cancel"' in ui
assert 'if (choice == IDYES) return Layout_SaveDraft(hWnd, st);' in ui
assert 'return choice == IDNO;' in ui
assert 'if (st->resolvingDraft) return false;' in ui
assert 'if (!KeyboardUI_CloseLayoutEditor()) return 0;' in app
assert 'if (!KeyboardUI_CloseLayoutEditor(true)) return FALSE;' in app
assert 'ID_LAYOUT_LABEL_EDIT && HIWORD(wParam) == EN_CHANGE' in ui
assert 'Apply Label' not in ui
assert 'KeyboardSubpages_TestLayoutEditor()' in ui
assert 'HallJoyPersistence::SaveStage::Replace' in ui
assert 'ID_LAYOUT_KEYS' not in ui
assert 'halljoy::layout_editor::RowY' in ui
assert 'ID_LAYOUT_UNDO' in ui and 'ID_LAYOUT_REDO' in ui
assert 'KeyboardLayout_KeyY' in server
assert 'KeyboardLayout_TestReadPresetY' in layout
assert 'layout_editor_model_test.cpp' in (hall.parents[2] / 'tools' / 'run_native_backend_checks.py').read_text(encoding='utf-8-sig')
assert 'TaskDialogIndirect' not in ui
assert 'DialogBoxIndirectParamW' in ui and 'Layout_SaveDialogProc' in ui
assert 'SendMessageW(dialog, DM_SETDEFID, IDCANCEL, 0)' in ui
assert 'halljoy::layout_editor::GridForScale(scale)' in ui
assert 'Layout_DrawRulersAndGuides(hWnd, memDC, st)' in ui
assert 'Layout_ResizeEdges(st, pt)' in ui and 'Resize(origin, st->resizeEdges' in ui
assert 'halljoy::layout_editor::SnapPosition' in ui
assert 'FillRect(dis->hDC, &dis->rcItem, UiTheme::Brush_PanelBg())' in ui
assert 'st->resizeOrigin = {x, KeyboardLayout_KeyY(key), key.w, key.h}' in ui
assert 'g.DrawLine(&guide, r.GetRight(), canvas.Y, r.GetRight(), canvas.GetBottom())' in ui
assert 'g.DrawLine(&guide, canvas.X, r.GetBottom(), canvas.GetRight(), r.GetBottom())' in ui
assert 'SetItemButtonKind(st->cmbLayout' not in ui
assert 'BeginInlineEditSelected(st->cmbLayout' not in ui
assert 'groups.push_back({L"+ Create New Layout...",{Create}})' in picker
assert 'KeyboardLayout_CreatePreset(name, &created, st->editingPresetIdx, false, st->layoutPicker.CreationBrand())' in ui
assert 'LayoutEditor_DeletePreset(preset, true)' in ui
assert 'source == st->cmbPreset ? st->layoutPicker.PresetAt(index) : st->layoutPicker.VariantAt(index)' in ui
assert 'KeyboardLayout_SetPresetIndex' not in picker
assert 'L"Brand", p.brand.c_str()' in layout and 'ok &= p.brand == brand;' in layout
assert 'g_keychronK4HeKeys' not in layout
assert 'FindPresetByName(ResolveSavedPresetName(nameBuf))' in layout
assert 'FindPresetByName(ResolveSavedPresetName(name))' in layout
assert 'FileNamePolicy_Equivalent(e.path().stem().wstring(), L"Keychron K4 HE")' in layout
assert 'Click the same delete icon again to confirm.' not in ui
assert 'PremiumCombo::GetDeleteConfirmation(source) != index' in ui
print("LAYOUT_EDITOR_OVERLAY_STATIC_AUDIT=PASS")
