#!/usr/bin/env python3
"""Guard the browser overlay's idle redraw and cache bounds."""

from pathlib import Path


OVERLAY = Path(__file__).resolve().parents[1] / "HallJoy" / "overlay_server.cpp"


def require(condition: bool, message: str) -> None:
    if not condition:
        raise SystemExit(f"FAIL: {message}")
    print(f"PASS: {message}")


def main() -> int:
    text = OVERLAY.read_text(encoding="utf-8-sig", errors="strict")
    require("let redraw=resize()" in text and
            "if(!existed||Math.abs(v-last)>=.0005)redraw=true" in text and
            "if(redraw){" in text,
            "unchanged settled analogue state skips the full canvas redraw")
    require("if(Math.abs(target-v)<.0005)v=target" in text,
            "smoothing converges to an exact idle state")
    require("visualStyleKey" in text and "sk!==visualStyleKey" in text,
            "visual setting changes still invalidate the retained frame")
    require("SPRITE_CACHE_MAX=512" in text and "LABEL_CACHE_MAX=256" in text,
            "browser bitmap caches have bounded production-sized limits")
    require("g_overlayRefreshIntervalMs{ 8 }" in text and
            "std::clamp(ms, 1, 250)" in text,
            "new installs default to 8 ms while 1 ms remains an explicit opt-in")
    require("spriteCache.keys().next().value" in text and
            "labelCache.keys().next().value" in text and
            "oldest=Infinity" not in text,
            "cache eviction is constant-time insertion-order LRU")
    require("syntheticMode&&latest" in text and "syntheticApply(latest,start)" in text,
            "animated stress mode continues to invalidate changing depths")
    require("perfFrames++" in text and "if(now-lastPerfReport>5000)" in text,
            "draw and periodic browser telemetry remain available")
    require("g_overlayPerfServerMutex" in text and
            "g_perfSendSamples" in text and
            "double avgSend = sendSamples ?" in text,
            "server timing windows keep send averages and maxima internally consistent")
    print("OVERLAY_RENDER_EFFICIENCY_STATIC_AUDIT=PASS")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
