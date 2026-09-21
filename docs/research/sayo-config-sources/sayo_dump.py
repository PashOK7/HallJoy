#!/usr/bin/env python
# -*- coding: utf-8 -*-
"""
SayoDevice 設定読み込みツール (read-only)

SayoDevice 2x6V RGB (VID=0x8089, PID=0x000B) の現在の設定を HID 経由で読み込み、
JSON ファイルにダンプします。

プロトコル: fidian/sayo-keyboard-configuration (OSS) の実装を参照。
  - 64 バイト固定 HID レポート
  - 構造: [id(2)] [cmd] [data_len] [data...] [check_sum]
  - check_sum = sum(bytes[0 .. data_len+2]) & 0xFF  (cmd, data_len, data の和)
  - 通信: hid_write(64B) -> hid_read(64B)

★読み込みのみ行います。書き込み(Save)は一切行いません。
"""

import os
import sys
import json
import time

# このスクリプトの配置ディレクトリ(ここに hidapi.dll を置く)
_HERE = os.path.dirname(os.path.abspath(__file__))

_hid_ready = False


def init_hid():
    """hidapi.dll が読めるように DLL 検索パスを登録し、hid モジュールを import する。
    GUI アプリ等からも呼び出し可能。成功したら hid モジュールを返す。"""
    global _hid_ready, hid
    if _hid_ready:
        return hid
    if hasattr(os, "add_dll_directory"):
        try:
            os.add_dll_directory(_HERE)
        except Exception:
            pass
    try:
        import hid as _hid
        hid = _hid
        _hid_ready = True
        return hid
    except Exception as e:
        raise RuntimeError(
            "hidapi の読み込みに失敗しました。hidapi.dll をスクリプトと同じフォルダに配置してください: " + str(e)
        )

# === 定数 ===
VID = 0x8089
PID = 0x000B

# 設定通信用インターフェース: MI_01 & Col01 (usage_page=0xFF00, vendor-defined)
USAGE_PAGE_VENDOR = 0xFF00

# コマンド
CMD_INFO = 0          # デバイス情報取得
CMD_KEY_READ = 6      # キー設定読み込み (pattern=0)
CMD_SCRIPT = 0xF0     # スクリプト領域読み書き (addr_h, addr_l)

# デバイス識別コード
MODEL_NAMES = {
    0x0002: "O2",
    0x0003: "O2C",
    0x0004: "O2S",
    0x0005: "O2T_ES",
    0x0006: "O2T_QS",
    0x0007: "O2MINI",
    0x0008: "M1T4K",
}

# キー動作モード (mode code = resp[5]) — fidian main.json / main_vid_2.json より
# ※原語は中国語。日本語のみに整理済み。
KEY_MODE_NAMES = {
    0:   "キーボード",
    1:   "マルチキー",
    2:   "マウス",
    3:   "コンシューマ",
    5:   "キーボード(中断しない)",
    6:   "二段階キーボード",
    7:   "二段階キーボード",
    8:   "パスワード入力",
    9:   "連打 AAA",
    10:  "連打 ABABAB",
    11:  "連打 ABCABC",
    12:  "連打 ABABC",
    13:  "連打 AAAAA+停止",
    14:  "連打 ABABAB+停止",
    15:  "連打 ABCABC+停止",
    16:  "連打 ABABCABABC+停止",
    17:  "遅延指定キーボード(ms)",
    18:  "遅延+ランダム(ms)",
    19:  "遅延+ランダム連打(ms)",
    20:  "連打 ABBBB+停止",
    21:  "ABC順次→終了",
    22:  "連打 ABABAB+間隔",
    23:  "連打 ABCCCC+停止",
    25:  "押しっぱなし連打 AAA",
    26:  "押しっぱなし連打 ABABAB",
    27:  "押しっぱなし連打 ABCABC",
    28:  "押しっぱなし連打 ABABCABABC",
    32:  "ランダム連打",
    33:  "二段階キーボード",
    40:  "ゲームパッド",
    48:  "定型文入力",
    49:  "エンコーダ(コンシューマ)",
    62:  "カスタムスクリプト(モーメンタリ)",
    63:  "カスタムスクリプト(トグル)",
    128: "FN レイヤー切替",
    129: "Bluetooth 切替",
    130: "BLE 切替",
}
# 64〜127 は ユーザースクリプト1〜64 に対応(動的生成)
for _c in range(64, 128):
    KEY_MODE_NAMES[_c] = f"ユーザースクリプト{_c - 63}"

# USB HID キーコードの代表値(読みやすさのため)
HID_KEY_NAMES = {
    0x04: "A", 0x05: "B", 0x06: "C", 0x07: "D", 0x08: "E",
    0x09: "F", 0x0a: "G", 0x0b: "H", 0x0c: "I", 0x0d: "J",
    0x0e: "K", 0x0f: "L", 0x10: "M", 0x11: "N", 0x12: "O",
    0x13: "P", 0x14: "Q", 0x15: "R", 0x16: "S", 0x17: "T",
    0x18: "U", 0x19: "V", 0x1a: "W", 0x1b: "X", 0x1c: "Y",
    0x1d: "Z", 0x1e: "1", 0x1f: "2", 0x20: "3", 0x21: "4", 0x22: "5",
    0x23: "6", 0x24: "7", 0x25: "8", 0x26: "9", 0x27: "0",
    0x28: "Enter", 0x29: "ESC", 0x2a: "Backspace", 0x2b: "Tab",
    0x2c: "Space", 0x2d: "-", 0x2e: "=", 0x2f: "[", 0x30: "]",
    0x31: "\\", 0x33: ";", 0x34: "'", 0x35: "`", 0x36: ",",
    0x37: ".", 0x38: "/", 0x39: "CapsLock",
    0x3a: "F1", 0x3b: "F2", 0x3c: "F3", 0x3d: "F4", 0x3e: "F5",
    0x3f: "F6", 0x40: "F7", 0x41: "F8", 0x42: "F9", 0x43: "F10",
    0x44: "F11", 0x45: "F12",
    0x50: "←", 0x51: "↓", 0x52: "↑", 0x53: "→",
    0x4f: "→", 0x4a: "Home", 0x4b: "PageUp",
    0x4e: "PageDown", 0x4d: "End", 0x49: "Insert", 0x4c: "Delete",
}

# HID修飾キービットマスク
MOD_BITS = [
    (0x01, "Ctrl"), (0x02, "Shift"), (0x04, "Alt"), (0x08, "GUI"),
    (0x10, "Ctrl"), (0x20, "Shift"), (0x40, "Alt"), (0x80, "GUI"),
]


# === スクリプトバイトコード定義 (fidian script.h より) ===
# 各命令のバイト長(code + operands)
SCRIPT_STEP_LEN = [1] * 256  # デフォルト1
# 命令名
SCRIPT_OP_NAMES = {}

def _define_script_ops():
    ops = {
        # 制御
        0x01: ("NOP", 1), 0x02: ("JMP", 3), 0x03: ("SJMP", 2), 0x04: ("AJMP", 2),
        0x05: ("SLEEP×256", 2), 0x06: ("SLEEP", 2), 0x07: ("SLEEP_RAND×256", 2),
        0x08: ("SLEEP_RAND", 2), 0x09: ("SLEEP×256_V", 2), 0x0a: ("SLEEP_V", 2),
        0x0b: ("SLEEP_RAND×8_V", 2), 0x0c: ("SLEEP_RAND_V", 2),
        # キー操作(通常=修飾キー押しっぱなし)
        0x10: ("SK", 2), 0x11: ("GK", 2), 0x12: ("MK", 2), 0x13: ("MU", 2),
        0x14: ("SK_V", 2), 0x15: ("GK_V", 2), 0x16: ("MK_V", 2), 0x17: ("MU_V", 2),
        # キー操作(ユニコード/長押し無し)
        0x18: ("USK", 2), 0x19: ("UGK", 2), 0x1a: ("UMK", 2), 0x1b: ("UMU", 2),
        0x1c: ("USK_V", 2), 0x1d: ("UGK_V", 2), 0x1e: ("UMK_V", 2), 0x1f: ("UMU_V", 2),
        0x20: ("UPDATE", 1),
        # マウス
        0x21: ("MO_XYZ", 3), 0x22: ("MO_XYZ_V", 3),
        0x23: ("GA_XYZ", 5), 0x24: ("GA_XYZ_V", 5),
        0x25: ("TB_XY", 5), 0x26: ("TB_XY_V", 3),
        0x2c: ("GAK", 2), 0x2d: ("GAK_V", 2), 0x2e: ("UGAK", 2), 0x2f: ("UGAK_V", 2),
        0x30: ("C2K", 1), 0x31: ("U2K", 1), 0x32: ("C2K_RAND", 1),
        # ジャンプ/分岐
        0x48: ("JFC", 2), 0x49: ("JFNC", 2), 0x4a: ("JFZ", 3), 0x4b: ("JFNZ", 3),
        0x4c: ("DJFNZ", 3), 0x4d: ("CJFNE", 3),
        0x4e: ("JC", 2), 0x4f: ("JNC", 2), 0x50: ("JZ", 2), 0x51: ("JNZ", 2),
        0x52: ("DJNZ", 3), 0x53: ("CJNE", 3), 0x54: ("CALL", 3), 0x55: ("RET", 1),
        # 演算
        0x56: ("ANL", 2), 0x57: ("ANLD", 2), 0x58: ("ADD", 2), 0x59: ("ADDD", 2),
        0x5a: ("SUB", 2), 0x5b: ("SUBD", 2), 0x5c: ("ORL", 2), 0x5d: ("ORLD", 2),
        0x5e: ("DEC", 1), 0x5f: ("INC", 1),
        0x60: ("MUL", 2), 0x61: ("DIV", 2), 0x62: ("XRL", 2), 0x63: ("XRLD", 2),
        0x64: ("RL", 1), 0x65: ("RLD", 2), 0x66: ("RR", 1), 0x67: ("RRD", 2),
        0x68: ("CLR", 1), 0x69: ("CPL", 1), 0x6a: ("XCH", 2),
        0x6c: ("PUSH", 1), 0x6d: ("POP", 1), 0x6e: ("MOV", 2), 0x6f: ("MOVD", 2),
        # LED
        0xe0: ("LED_CTRL", 2), 0xe1: ("LED_COL", 2),
        # 制御フロー
        0xf4: ("WHILE_UPDATE", 1), 0xf6: ("MOV_PC2REG", 1), 0xf7: ("VALUE_RELOAD", 1),
        0xf8: ("MODE_JOG", 1), 0xf9: ("WHILE_UP", 1), 0xfa: ("WHILE_DOWN", 1),
        0xfb: ("IF_UP_EXIT", 1), 0xfc: ("IF_DOWN_EXIT", 1), 0xfd: ("IF_KA_EXIT", 1),
        0xfe: ("RES", 1), 0xff: ("EXIT", 1),
    }
    for code, (name, length) in ops.items():
        SCRIPT_STEP_LEN[code] = length
        SCRIPT_OP_NAMES[code] = name

_define_script_ops()


def find_config_interface():
    """設定通信用の HID インターフェース(MI_01, vendor-defined)を探す。"""
    _hid = init_hid()
    devs = _hid.enumerate(VID, PID)
    for d in devs:
        iface = d.get('interface_number')
        up = d.get('usage_page')
        # MI_01 の vendor-defined collection を優先
        if iface == 1 and up == USAGE_PAGE_VENDOR:
            return d['path']
    # フォールバック: MI_01 の先頭
    for d in devs:
        if d.get('interface_number') == 1:
            return d['path']
    return None


def checksum(packet, data_len):
    """id, cmd, data_len, data[0..data_len-1] の和をとる(先頭 id を含む)。
    fidian 実装(test.c / o2_protocol.cpp)に準拠: ループ i=0..data_len+2。"""
    s = 0
    for i in range(0, data_len + 3):  # id(1) + cmd(1) + data_len(1) + data → idx 0..data_len+2
        s += packet[i]
    return s & 0xFF


def make_packet(cmd, data):
    """64 バイトのパケットを組み立てる。"""
    pkt = bytearray(64)
    pkt[0] = 2                       # id (固定)
    pkt[1] = cmd & 0xFF
    pkt[2] = len(data) & 0xFF
    for i, b in enumerate(data):
        pkt[3 + i] = b
    pkt[3 + len(data)] = checksum(pkt, len(data))
    return bytes(pkt)


def send_recv(dev, cmd, data, timeout=1500):
    """パケット送信 -> 応答受信。"""
    pkt = make_packet(cmd, data)
    dev.write(pkt)
    # 応答を待機(ブロッキング読み取り)
    resp = dev.read(64, timeout=timeout)
    return resp


def read_device_info(dev):
    """cmd=0 でデバイス情報(型番/バージョン/シリアル)を取得。"""
    # まず現在時刻を送ってデバイスを起こす(fidian 実装準拠)
    t = time.localtime()
    data = [t.tm_mday & 0xFF, t.tm_hour & 0xFF, t.tm_min & 0xFF, t.tm_sec & 0xFF]
    resp = send_recv(dev, CMD_INFO, data)
    return resp


def describe_key(mode, values):
    """mode と values から人間が読みやすい表現を返す。"""
    v0, v1, v2, v3 = values
    if mode in (0, 1):  # キーボード系: values=[modifier_mask, keycode1, keycode2, keycode3]
        mods = [name for bit, name in MOD_BITS if v0 & bit]
        codes = []
        for code in (v1, v2, v3):
            if code != 0:
                codes.append(HID_KEY_NAMES.get(code, f"0x{code:02x}"))
        mod_str = "+".join(mods) if mods else ""
        if codes:
            return (mod_str + "+" if mod_str else "") + "+".join(codes)
        return "(未割当)"
    if mode == 2:  # マウス: values=[button_mask, x, y, scroll]
        btns = []
        if v0 & 0x01:
            btns.append("左")
        if v0 & 0x02:
            btns.append("右")
        if v0 & 0x04:
            btns.append("中")
        parts = btns or ["(ボタン無し)"]
        if v1 or v2:
            parts.append(f"移動(x={v1 if v1<128 else v1-256},y={v2 if v2<128 else v2-256})")
        if v3:
            parts.append(f"スクロール={v3 if v3<128 else v3-256}")
        return " ".join(parts)
    if mode == 3:  # コンシューマ: values[0] = usage code
        return f"コンシューマ usage=0x{v0:02x}"
    if 64 <= mode <= 127:  # ユーザースクリプト1〜64
        return f"スクリプト{mode - 63}"
    if mode in (62, 63):  # カスタムスクリプト 点動/トグル
        return f"カスタムスクリプト step={v0}"
    if mode in (6, 7, 33):  # US1/US2/二段階キーボード: いずれも [mod1,key1,mod2,key2]
        m1 = [name for bit, name in MOD_BITS if v0 & bit]
        m2 = [name for bit, name in MOD_BITS if v2 & bit]
        k1 = HID_KEY_NAMES.get(v1, f"0x{v1:02x}") if v1 else ""
        k2 = HID_KEY_NAMES.get(v3, f"0x{v3:02x}") if v3 else ""
        step1 = ("+".join(m1 + [k1]) if (m1 or k1) else "—")
        step2 = ("+".join(m2 + [k2]) if (m2 or k2) else "—")
        return f"[{step1}]→[{step2}]"
    if mode == 8:
        return "パスワード入力"
    if mode == 48:
        return "定型文入力"
    if mode == 128:
        return f"FN レイヤー切替 → {v0}"
    if mode == 129:
        return f"Bluetooth デバイス切替 → {v0}"
    return f"raw values={values}"


def read_script_raw(dev, max_addr=8192):
    """cmd=0xF0 でスクリプト領域(最大8192B)を先頭から順に読み込む。
    戻り値: bytes(読み込めた分)。未サポート時は b''。"""
    buf = bytearray()
    addr = 0
    while addr < max_addr:
        data = [(addr >> 8) & 0xFF, addr & 0xFF]
        resp = send_recv(dev, CMD_SCRIPT, data, timeout=1000)
        if not resp or len(resp) < 4:
            break
        if resp[1] != 0:  # cmd != 0 → エラー/未サポート
            break
        data_len = resp[2]
        if data_len == 0:
            break
        chunk = bytes(resp[3:3 + data_len])
        buf.extend(chunk)
        addr += data_len
        time.sleep(0.01)
    return bytes(buf)


def disassemble_script(buf):
    """スクリプトバイト列を命令リストに変換。
    戻り値: [{"addr":int, "code":int, "name":str, "values":[...]}]。
    終端(EXIT=0xFF / 空領域=0x00 連続)に達したら打ち切る。"""
    ops = []
    i = 0
    n = len(buf)
    while i < n:
        code = buf[i]
        # 終端検出: EXIT(0xFF) または 0x00 が続き続く場合
        if code == 0xFF:
            ops.append({"addr": i, "code": code, "name": "EXIT", "values": []})
            break
        if code == 0x00 and i + 1 < n and buf[i + 1] == 0x00:
            # 0x00 連続 → 空領域の可能性。ただし NOP(0x01)でない0x00は実質終端
            break
        length = SCRIPT_STEP_LEN[code]
        name = SCRIPT_OP_NAMES.get(code, f"0x{code:02x}")
        values = list(buf[i + 1:i + length])
        # パディング不足対策
        while len(values) < length - 1:
            values.append(0)
        ops.append({"addr": i, "code": code, "name": name, "values": values})
        i += length
    return ops


def describe_script_op(op):
    """1命令を人間が読みやすい文字列に変換。"""
    code = op["code"]
    name = op["name"]
    v = op["values"]

    def _mod_keys(mask):
        return "+".join(nm for bit, nm in MOD_BITS if mask & bit)

    # キー操作系: values[0]=modifier_mask, values[1]=keycode
    if code in (0x10, 0x14, 0x18, 0x1c):  # SK系(単一キー)
        mods = _mod_keys(v[0]) if len(v) >= 1 else ""
        key = HID_KEY_NAMES.get(v[1], f"0x{v[1]:02x}") if len(v) >= 2 else ""
        return ("mods " if mods else "") + f"キー押下 {mods}+{key}".strip("+")
    if code in (0x11, 0x15, 0x19, 0x1d):  # GK系
        key = HID_KEY_NAMES.get(v[1], f"0x{v[1]:02x}") if len(v) >= 2 else ""
        return f"キー {key}"
    if code in (0x12, 0x16, 0x1a, 0x1e):  # MK系(修飾キー操作)
        mods = _mod_keys(v[0]) if len(v) >= 1 else ""
        return f"修飾キー {mods}".strip()
    if code == 0x0a:  # SLEEP_V
        return f"待機 {v[0]}ms" if v else "待機"
    if code == 0x06:
        return f"待機 {v[0] * 256 if v else 0}ms" if v else "待機"
    if code in (0x05, 0x09):
        return f"待機 {v[0] * 256 if v else 0}ms(粗)" if v else "待機"
    if code == 0xfe:
        return "リセット"
    if code == 0xff:
        return "終了"
    return f"{name} {v}"


def open_config_device():
    """設定通信用 HID デバイスを開き、(dev) を返す。
    見つからない/開けない場合は例外を送出する。呼び出し側で dev.close() すること。"""
    _hid = init_hid()
    path = find_config_interface()
    if not path:
        raise RuntimeError("設定通信用 HID インターフェースが見つかりません。デバイスが接続されているか確認してください。")
    dev = _hid.Device(path=path)
    # オープン直後にバッファに残ったデータがあれば破棄し、ブロッキングモードへ
    dev.nonblocking = True
    try:
        dev.read(64, timeout=50)
    except Exception:
        pass
    dev.nonblocking = False
    return dev


def read_all_config(num_keys=12):
    """デバイス情報 + キーマップ(Fn0のみ)をまとめて読み込む。GUI/CUI 共用。
    戻り値: {"device": {...}, "keys": [...]} (各 key に "human" も付与)
    例外: デバイスが見つからない/開けない場合は RuntimeError。"""
    dev = open_config_device()
    try:
        info_resp = read_device_info(dev)
        result = {"device": {}, "keys": []}

        if info_resp:
            data_len = info_resp[2]
            cmd = info_resp[1]
            if cmd == 0 and data_len >= 4:
                version = info_resp[3] * 256 + info_resp[4]
                model = info_resp[5] * 256 + info_resp[6]
                model_name = MODEL_NAMES.get(model, f"未知の型番(0x{model:04x})")
                serial_bytes = info_resp[11:11 + max(0, data_len - 8)]
                try:
                    serial_str = bytes(serial_bytes).decode('ascii', errors='replace').rstrip('\x00')
                except Exception:
                    serial_str = serial_bytes.hex()
                result["device"] = {
                    "vendor_id": f"0x{VID:04x}",
                    "product_id": f"0x{PID:04x}",
                    "firmware_version": f"0x{version:04x}",
                    "model_code": f"0x{model:04x}",
                    "model_name": model_name,
                    "serial_number": serial_str,
                }
            else:
                result["device"]["raw_info"] = list(info_resp)

        keys = read_keymap(dev, num_keys=num_keys)
        for k in keys:
            if "error" not in k and "error_code" not in k:
                k["human"] = describe_key(k["mode"], k["values"])
            else:
                k["human"] = "(読込失敗)"
        result["keys"] = keys
        return result
    finally:
        dev.close()


def read_keymap_layers(dev, num_keys=12):
    """cmd=22 で全レイヤー(Fn0〜Fn4)を読み込む。
    戻り値: [{number, layers:[{mode,mode_name,values,human}, ...]}, ...]
    各キーの layers[i] が Fn i に対応。"""
    keys = []
    for kn in range(num_keys):
        data = [0, kn]  # pattern=0(read)
        resp = send_recv(dev, 22, data, timeout=1000)
        if not resp or len(resp) < 20:
            keys.append({"number": kn, "error": "no response", "layers": []})
            continue
        resp_cmd = resp[1]
        if resp_cmd != 0:
            keys.append({"number": kn, "error_code": resp_cmd, "layers": []})
            continue
        data_len = resp[2]
        num_layers = (data_len - 16) // 6
        base = 3 + 16  # data 開始位置 + shape情報(16B)
        layers = []
        for lay in range(num_layers):
            off = base + lay * 6
            l_mode = resp[off]
            vals = [resp[off + 2], resp[off + 3], resp[off + 4], resp[off + 5]]
            layers.append({
                "mode": l_mode,
                "mode_name": KEY_MODE_NAMES.get(l_mode, f"0x{l_mode:02x}"),
                "values": vals,
                "human": describe_key(l_mode, vals),
            })
        keys.append({"number": kn, "layers": layers})
        time.sleep(0.02)
    return keys


def read_all_config_layers(num_keys=12):
    """デバイス情報 + 全レイヤー(Fn0〜Fn4)のキーマップをまとめて読み込む。
    戻り値: {"device": {...}, "keys": [{number, layers:[...]}], "num_layers": int}
    例外: デバイスが見つからない/開けない場合は RuntimeError。"""
    dev = open_config_device()
    try:
        info_resp = read_device_info(dev)
        result = {"device": {}, "keys": [], "num_layers": 0}

        if info_resp:
            data_len = info_resp[2]
            cmd = info_resp[1]
            if cmd == 0 and data_len >= 4:
                version = info_resp[3] * 256 + info_resp[4]
                model = info_resp[5] * 256 + info_resp[6]
                model_name = MODEL_NAMES.get(model, f"未知の型番(0x{model:04x})")
                serial_bytes = info_resp[11:11 + max(0, data_len - 8)]
                try:
                    serial_str = bytes(serial_bytes).decode('ascii', errors='replace').rstrip('\x00')
                except Exception:
                    serial_str = serial_bytes.hex()
                result["device"] = {
                    "vendor_id": f"0x{VID:04x}",
                    "product_id": f"0x{PID:04x}",
                    "firmware_version": f"0x{version:04x}",
                    "model_code": f"0x{model:04x}",
                    "model_name": model_name,
                    "serial_number": serial_str,
                }

        keys = read_keymap_layers(dev, num_keys=num_keys)
        result["keys"] = keys
        # レイヤー数を検出(最初の正常キーから)
        for k in keys:
            if k.get("layers"):
                result["num_layers"] = len(k["layers"])
                break
        return result
    finally:
        dev.close()


def read_keymap(dev, num_keys=12):
    """cmd=6, pattern=0 でキーごとの設定を読み込む。"""
    keys = []
    for kn in range(num_keys):
        # pattern=0 (read), number=kn
        data = [0, kn]
        resp = send_recv(dev, CMD_KEY_READ, data, timeout=1000)
        if not resp or len(resp) < 9:
            keys.append({"number": kn, "error": "no response"})
            continue
        resp_cmd = resp[1]
        if resp_cmd != 0:
            keys.append({"number": kn, "error_code": resp_cmd})
            continue
        # resp 構造: [id][cmd][data_len][pattern][number][type][_][plain0..3]...
        pattern = resp[3]
        number = resp[4]
        mode = resp[5]
        plain0 = resp[7]
        plain1 = resp[8]
        plain2 = resp[9]
        plain3 = resp[10]
        keys.append({
            "number": number,
            "pattern": pattern,
            "mode": mode,
            "mode_name": KEY_MODE_NAMES.get(mode, f"未知(0x{mode:02x})"),
            "values": [plain0, plain1, plain2, plain3],
        })
        time.sleep(0.02)
    return keys


def main():
    try:
        init_hid()
    except RuntimeError as e:
        print(e)
        sys.exit(1)
    print("=== SayoDevice 設定読み込み (read-only) ===\n")

    try:
        result = read_all_config(num_keys=12)
    except RuntimeError as e:
        print(f"読み込みに失敗しました: {e}")
        sys.exit(1)

    dev_info = result.get("device", {})
    if dev_info.get("model_name"):
        print(f"  型番コード : {dev_info.get('model_code')} ({dev_info['model_name']})")
        print(f"  ファーム   : {dev_info.get('firmware_version')}")
        print(f"  シリアル   : {dev_info.get('serial_number')}")
    elif "raw_info" in dev_info:
        print(f"  ※ デバイス情報応答が想定外です: {dev_info['raw_info']}")

    print("\n読み込んだキー設定:")
    for k in result["keys"]:
        if "error" in k:
            print(f"  Key{k.get('number','?')}: {k['error']}")
            continue
        if "error_code" in k:
            print(f"  Key{k['number']:2d}: エラーコード 0x{k['error_code']:02x}")
            continue
        vals = k["values"]
        hexs = ' '.join(f'{v:02x}' for v in vals)
        print(f"  Key{k['number']:2d}: [{k['mode_name']}] raw=[{hexs}]  => {k.get('human','')}")

    # --- JSON に保存 ---
    out = os.path.join(os.path.dirname(os.path.abspath(__file__)), "sayo_config_dump.json")
    with open(out, 'w', encoding='utf-8') as f:
        json.dump(result, f, ensure_ascii=False, indent=2)
    print(f"\n設定を保存しました: {out}")


if __name__ == "__main__":
    main()
