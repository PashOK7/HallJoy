#!/usr/bin/env python
# -*- coding: utf-8 -*-
"""
SayoDevice 設定表示 デスクトップアプリ (Windows / tkinter)

SayoDevice 2x6V RGB の現在のキー設定を読み込み、
2x6 のキートップ風レイアウトで表示するコンパクトなウィンドウ。

機能:
  - 横2 × 縦6 のキー一覧(キートップ風)
  - 「最前面」チェックボックスで常に手前に固定(切替可)
  - 「リフレッシュ」ボタンで設定を再読込(同期待ち)
  - 起動時に自動で読み込む

読込ロジックは sayo_dump モジュールを再利用(読み込み専用・書き込みなし)。
"""

import os
import sys
import tkinter as tk
from tkinter import ttk, messagebox

# スクリプトと同じフォルダをモジュール検索パスに含める(direct 実行対応)
_HERE = os.path.dirname(os.path.abspath(__file__))
if _HERE not in sys.path:
    sys.path.insert(0, _HERE)

import sayo_dump as sayo


# === レイアウト定数 ===
ROWS = 6
COLS = 2
NUM_KEYS = ROWS * COLS

KEY_W = 96       # キートップ幅
KEY_H = 50       # キートップ高さ
KEY_PAD = 4      # キー間隔

# 表示順マッピング: 画面上の位置(0..11) → 表示するデバイス上のキー番号
# デバイスの物理キー番号と異なる順序で表示したい場合に使用。
# 添付画像の配置に合わせるためのカスタム並び。
DISPLAY_ORDER = [5, 11, 4, 10, 3, 9, 2, 8, 1, 7, 0, 6]


class KeyTop(tk.Canvas):
    """1個のキートップ風表示ウィジェット。"""

    def __init__(self, parent, index, bg="#f0f0f0", **kw):
        super().__init__(parent, width=KEY_W, height=KEY_H,
                         bg=bg, highlightthickness=0, **kw)
        self.index = index
        self.label_var = tk.StringVar(value="")
        self.human_var = tk.StringVar(value="")

        # キートップの台形(上辺が短い立体風)
        x0, y0 = 4, 4
        x1, y1 = KEY_W - 4, KEY_H - 4
        inset = 6
        # 影(底面)
        self.create_polygon(
            x0 + 1, y1, x1 + 1, y1, x1 - inset, y0 + inset,
            smooth=False, fill="#9aa0a6", outline=""
        )
        # 上面
        self._face = self.create_polygon(
            x0, y1, x1, y1, x1 - inset, y0 + inset, x0 + inset, y0 + inset,
            smooth=False, fill="#f1f3f4", outline="#c1c4c7"
        )
        # 割当内容(中央に大きく表示)
        self._assign = self.create_text(
            (x0 + x1) / 2, (y0 + y1) / 2 - 3,
            text="", fill="#202124", font=("Segoe UI", 9)
        )
        # モード名(下部)
        self._mode = self.create_text(
            (x0 + x1) / 2, y1 - 7,
            text="", fill="#80868b", font=("Segoe UI", 7)
        )

    def set_data(self, key_info):
        """key_info: sayo_dump.read_keymap の1要素。None なら未読込表示。"""
        if key_info is None:
            self.itemconfig(self._assign, text="—")
            self.itemconfig(self._mode, text="")
            self.itemconfig(self._face, fill="#e8eaed")
            return

        if "error" in key_info or "error_code" in key_info:
            self.itemconfig(self._assign, text="(読込失敗)")
            self.itemconfig(self._mode, text=key_info.get("error", ""))
            self.itemconfig(self._face, fill="#fce8e6")
            return

        human = key_info.get("human", "")
        # 長すぎる場合はフォント調整
        text = human
        self.itemconfig(self._assign, text=text)
        self.itemconfig(self._mode, text=key_info.get("mode_name", ""))
        # 未割当は淡色
        face = "#f1f3f4" if human and "未割当" not in human else "#e8eaed"
        self.itemconfig(self._face, fill=face)


class SayoViewerApp:
    def __init__(self, root):
        self.root = root
        self.keys = []  # KeyTop ウィジェット
        self.last_result = None

        root.title("SayoDevice 設定ビューア")
        root.configure(bg="#f0f0f0")
        # コンパクトな固定サイズ(キー領域 + ヘッダ + レイヤー + フッタ)
        win_w = COLS * KEY_W + (COLS + 1) * KEY_PAD + 16
        win_h = ROWS * KEY_H + (ROWS + 1) * KEY_PAD + 120
        root.geometry(f"{win_w}x{win_h}")
        root.resizable(False, False)

        self._build_ui()
        # 起動時に自動読込
        root.after(100, self.refresh)

    # ---------- UI 構築 ----------
    def _build_ui(self):
        # ヘッダ(デバイス情報)
        header = ttk.Frame(self.root, padding=(8, 4))
        header.pack(fill=tk.X)
        self.title_var = tk.StringVar(value="SayoDevice (未接続)")
        ttk.Label(header, textvariable=self.title_var,
                  font=("Segoe UI", 10, "bold")).pack(side=tk.LEFT)
        self.fw_var = tk.StringVar(value="")
        ttk.Label(header, textvariable=self.fw_var,
                  font=("Segoe UI", 8), foreground="#5f6368").pack(side=tk.LEFT, padx=(8, 0))

        # レイヤー切り替え(Fn0〜Fn4)
        layer_frame = ttk.Frame(self.root, padding=(8, 0))
        layer_frame.pack(fill=tk.X)
        self.current_layer = tk.IntVar(value=0)
        self.layer_radios = []
        for li in range(5):
            rb = ttk.Radiobutton(layer_frame, text=f"Fn{li}",
                                 value=li, variable=self.current_layer,
                                 command=self._on_layer_change)
            rb.pack(side=tk.LEFT, padx=(0, 4))
            self.layer_radios.append(rb)

        # キー領域 (2x6 格子)
        grid = ttk.Frame(self.root, padding=(8, 0))
        grid.pack(fill=tk.BOTH, expand=True)
        for r in range(ROWS):
            for c in range(COLS):
                idx = r * COLS + c
                kt = KeyTop(grid, index=idx)
                kt.grid(row=r, column=c, padx=KEY_PAD // 2, pady=KEY_PAD // 2)
                self.keys.append(kt)
        grid.columnconfigure(0, weight=1)
        grid.columnconfigure(1, weight=1)

        # フッタ(操作パネル)
        footer = ttk.Frame(self.root, padding=(8, 4))
        footer.pack(fill=tk.X, side=tk.BOTTOM)

        self.topmost_var = tk.BooleanVar(value=True)
        ttk.Checkbutton(footer, text="最前面", variable=self.topmost_var,
                        command=self._on_topmost).pack(side=tk.LEFT)
        # 初期状態で最前面ON
        self.root.attributes("-topmost", True)

        self.refresh_btn = ttk.Button(footer, text="リフレッシュ", command=self.refresh)
        self.refresh_btn.pack(side=tk.RIGHT)

        self.status_var = tk.StringVar(value="")
        ttk.Label(footer, textvariable=self.status_var,
                  font=("Segoe UI", 7), foreground="#80868b").pack(side=tk.RIGHT, padx=(0, 8))

    # ---------- イベント ----------
    def _on_topmost(self):
        self.root.attributes("-topmost", bool(self.topmost_var.get()))

    def _on_layer_change(self):
        """レイヤー切り替え時に再読込せず、保持しているデータから再表示。"""
        if self.last_result:
            self._apply_layer(int(self.current_layer.get()))

    def refresh(self):
        """設定を再読込して表示を更新する(同期待ち)。"""
        # 読込中の表示
        self.refresh_btn.config(state=tk.DISABLED)
        self.status_var.set("読込中...")
        self.root.config(cursor="watch")
        self.root.update_idletasks()

        try:
            result = sayo.read_all_config_layers(num_keys=NUM_KEYS)
            self.last_result = result
            self._apply_result(result)
            self.status_var.set("読込完了")
        except RuntimeError as e:
            # デバイス未接続・オープン失敗
            self.status_var.set("読込失敗")
            messagebox.showwarning("SayoDevice", f"デバイスから設定を読み込めませんでした。\n\n{e}")
        except Exception as e:
            self.status_var.set("エラー")
            messagebox.showerror("SayoDevice", f"予期しないエラーが発生しました。\n\n{e}")
        finally:
            self.root.config(cursor="")
            self.refresh_btn.config(state=tk.NORMAL)

    def _apply_result(self, result):
        dev = result.get("device", {})
        model = dev.get("model_name", "")
        fw = dev.get("firmware_version", "")
        if model:
            self.title_var.set(f"SayoDevice {model}")
        else:
            self.title_var.set("SayoDevice (情報取得不可)")
        self.fw_var.set(f"FW {fw}" if fw else "")

        # 実際のレイヤー数に合わせてラジオボタンの有効/無効を切替
        num_layers = result.get("num_layers", 0)
        for li, rb in enumerate(self.layer_radios):
            rb.config(state=(tk.NORMAL if li < num_layers else tk.DISABLED))

        # 現在選択中レイヤーを表示
        self._apply_layer(int(self.current_layer.get()))

    def _apply_layer(self, layer_idx):
        """指定レイヤーのキーマップを表示。"""
        if not self.last_result:
            return
        keys = self.last_result.get("keys", [])
        for i, kt in enumerate(self.keys):
            src = DISPLAY_ORDER[i] if i < len(DISPLAY_ORDER) else i
            key_data = keys[src] if src < len(keys) else None
            if key_data is None:
                kt.set_data(None)
                continue
            layers = key_data.get("layers", [])
            if layer_idx < len(layers):
                # レイヤーデータを KeyTop.set_data が期待する形式に変換
                lay = layers[layer_idx]
                kt.set_data({
                    "mode": lay["mode"],
                    "mode_name": lay["mode_name"],
                    "values": lay["values"],
                    "human": lay["human"],
                })
            else:
                kt.set_data(None)


def main():
    try:
        sayo.init_hid()
    except RuntimeError as e:
        # hidapi が読めなくてもウィンドウは出し、リフレッシュ時に再試行
        root = tk.Tk()
        messagebox.showwarning("SayoDevice", str(e))
        root.destroy()
        return

    root = tk.Tk()
    # DPI 対策(Windows): 高DPIでも見切れないように
    try:
        import ctypes
        ctypes.windll.shcore.SetProcessDpiAwareness(1)
    except Exception:
        pass

    SayoViewerApp(root)
    root.mainloop()


if __name__ == "__main__":
    main()
