# HallJoy — クイックスタート

[![C++20](https://img.shields.io/badge/C%2B%2B-20-00599C?style=flat-square)](../../src/HallJoyProject/HallJoy/HallJoy.vcxproj)
[![Windows x64](https://img.shields.io/badge/Windows-x64-0078D4?style=flat-square)](../../README.md#quick-start)
[![PowerShell 5.1+](https://img.shields.io/badge/PowerShell-5.1%2B-5391FE?style=flat-square)](../PORTABLE.md)
[![NSIS 3](https://img.shields.io/badge/NSIS-3-4A7C59?style=flat-square)](../development/WINDOWS_INSTALLER.md)

[English](../../README.md) · [Español](README.es.md) · [Français](README.fr.md) · [Italiano](README.it.md) · [简体中文](README.zh-CN.md) · [한국어](README.ko.md) · [Русский](README.ru.md) · [Deutsch](README.de.md) · **日本語** · [हिन्दी](README.hi.md)

HallJoy は Windows 上で、対応するアナログキーボードの実際の押し込み量を
仮想 Xbox コントローラーの入力に変換します。通常のデジタルキーボードを
アナログ化するものではありません。ボタン・スティック・トリガーの割り当て、
応答曲線、デッドゾーン、プロファイル、Gamepad Tester を備えています。
作者：**PashOK7**。

これは短い入門ガイドであり、技術文書全体の翻訳ではありません。最新の対応機種と
制限は [Supported](../../README.md#supported) と
[Experimental support](../../README.md#experimental-support) を参照してください。

## 使い始める

1. Windows 10/11 x64 と対応するアナログキーボードが必要です。
2. [公式リリース](https://github.com/PashOK7/HallJoy/releases)から `HallJoy.exe` を取得します。
3. アプリを起動します。ViGEmBus がない場合は **Install ViGEmBus 1.22.0** を選び、
   別途表示される管理者権限の要求を承認します。
4. 競合するキーボードの Web 設定ツールを閉じます。デバイスと配列を確認し、
   必要なら配列を手動で選択します。
5. **Remap** で操作を割り当て、**Gamepad Tester** で押し込み量、入力の全範囲、
   キーを離した後に中立へ戻ることを確認します。

Wooting Analog SDK や Universal Analog Plugin の個別インストールは不要です。

## インストールと個人データ

アイコン、ショートカット、ユーザーデータを残すアンインストール機能を持つ
[任意のインストーラーをビルド](../development/WINDOWS_INSTALLER.md)できます。
公式リリースで既にインストーラーが配布されているという意味ではありません。
クローン内の `Update.cmd` は公式 EXE を `.local/portable` に取得し、`Start.cmd` は
そのコピーを起動します。このフォルダーは Git の対象外で、更新時も設定を保持します。
インストール版は通常 `%LOCALAPPDATA%\HallJoy` にデータを保存します。

英語のガイド：[データとバックアップ](../PORTABLE.md)、[プライバシー](../INPUT_AND_PRIVACY.md)、
[運転操作の例](../DRIVING_SETUP.md)。
[動画](https://youtu.be/MI_ZTS6UFhM) · [サポート](https://discord.gg/X5QsZJdN5a)
· [ライセンス](../../LICENSE) · [貢献ルール](../../.github/CONTRIBUTING.md)。
