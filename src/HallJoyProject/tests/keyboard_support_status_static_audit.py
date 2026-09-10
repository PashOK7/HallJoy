#!/usr/bin/env python3
"""Keep RM-37 keyboard-input/analogue correlation safe and non-invasive."""

from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
HALL = ROOT / "HallJoy"
REPO = ROOT.parents[1]


def require(condition: bool, message: str) -> None:
    if not condition:
        raise SystemExit(f"FAIL: {message}")
    print(f"PASS: {message}")


def main() -> int:
    source = (HALL / "keyboard_support_status.cpp").read_text(encoding="utf-8-sig")
    header = (HALL / "keyboard_support_status.h").read_text(encoding="utf-8-sig")
    app = (HALL / "app.cpp").read_text(encoding="utf-8-sig")
    ui = (HALL / "keyboard_subpages.cpp").read_text(encoding="utf-8-sig")
    timer_ui = (HALL / "keyboard_ui.cpp").read_text(encoding="utf-8-sig")
    page = (HALL / "keyboard_page_main.cpp").read_text(encoding="utf-8-sig")
    links = (HALL / "community_links.h").read_text(encoding="utf-8-sig")
    project = (HALL / "HallJoy.vcxproj").read_text(encoding="utf-8-sig")
    runner = (REPO / "tools" / "run_native_backend_checks.py").read_text(encoding="utf-8-sig")

    require("SetSearchObservation" in source and "searchCompleted" in header and
            "analogSourceConnected" in header,
            "status distinguishes incomplete startup from a completed negative observation")
    require("CreateFileW" not in source and "HidD_" not in source and "WriteFile" not in source,
            "status has no HID operation or vendor report path")
    require("HasSupportedAnalogSource" in timer_ui and "telemetry.deviceCount > 0" in timer_ui and
            "pluginHostDenseDeviceCount > 0" in timer_ui,
            "UI derives connected status from HallJoy native/UAP telemetry")
    require("Backend_IsRuntimeAdmissionOpen" in timer_ui and
            "searchCompleted && !supportStatus.analogSourceConnected" in page,
            "banner stays hidden until the engine startup generation has completed")
    require("WM_APP_ANALOG_SOURCE_STATUS_CHANGED" in timer_ui and
            "WM_APP_ANALOG_SOURCE_STATUS_CHANGED" in page,
            "main page is relaid out immediately when source status changes")
    require("HallJoyKeyboardSupportBanner" in page and
            "No supported analogue keyboard detected" in page and
            "Join Discord" in page and "CustomPage_DrawButton" in page and
            "Copy link" in page and "DrawSupportQr" in page and
            "https://discord.gg/5FQ297yZh" in links and '#include "community_links.h"' in page and "ShellExecuteW" in page and
            "SetClipboardData(CF_UNICODETEXT" in page,
            "absent source gets a visible Discord banner with one real invite URL and QR")
    require('#include "community_links.h"' in ui and 'GLOB_ID_DISCORD, &st->rcDiscord' in ui and
            'L"HallJoy on Discord"' in ui and 'ShellExecuteW(hWnd, L"open", kDiscordInviteUrl' in ui,
            "Global settings has a permanent retained Discord card sharing the banner invite")
    require('std::min(x + sliderW + gap + chipW, x + S(hWnd, 480))' in ui and
            'communityRight - x < S(hWnd, 440)' in ui,
            "community card has a DPI-scaled width cap and preserves narrow-window stacking")
    require(ui.index('st->rcCommunity = RECT{') < ui.index('st->rcGlobalProfile = RECT{'),
            "community card is laid out before the first Global settings control")
    community = ui[ui.index('const bool stackedCommunity'):ui.index('const bool discordPressed')]
    require('DT_SINGLELINE | DT_CALCRECT' in community and
            'DT_WORDBREAK | DT_CALCRECT' in community and
            '(communityTextBottom - communityTextTop - communityBlockHeight) / 2' in community and
            'st->rcDiscord.top - S(hWnd, 10)' in community and
            'communityTitle.right = communityTextRight' in community,
            "community text is measured and centred as a pair with a separate stacked text area")
    require('<ClCompile Include="keyboard_support_status.cpp" />' in project,
            "production project compiles the status implementation")
    require("keyboard_support_status_test.cpp" in runner,
            "portable pure-policy regression is executed")
    print("KEYBOARD_SUPPORT_STATUS_STATIC_AUDIT=PASS")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
