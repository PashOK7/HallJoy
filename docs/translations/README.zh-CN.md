# HallJoy — 快速入门

[![C++20](https://img.shields.io/badge/C%2B%2B-20-00599C?style=flat-square)](../../src/HallJoyProject/HallJoy/HallJoy.vcxproj)
[![Windows x64](https://img.shields.io/badge/Windows-x64-0078D4?style=flat-square)](../../README.md#quick-start)
[![PowerShell 5.1+](https://img.shields.io/badge/PowerShell-5.1%2B-5391FE?style=flat-square)](../PORTABLE.md)
[![NSIS 3](https://img.shields.io/badge/NSIS-3-4A7C59?style=flat-square)](../development/WINDOWS_INSTALLER.md)

[English](../../README.md) · [Español](README.es.md) · [Français](README.fr.md) · [Italiano](README.it.md) · **简体中文** · [한국어](README.ko.md) · [Русский](README.ru.md) · [Deutsch](README.de.md) · [日本語](README.ja.md) · [हिन्दी](README.hi.md)

HallJoy 在 Windows 上将受支持的模拟键盘的真实按键行程转换为虚拟 Xbox
手柄输入。它不会把普通数字键盘变成模拟键盘。功能包括按键、摇杆和扳机映射、
响应曲线、死区、配置文件及 Gamepad Tester。作者：**PashOK7**。

本文是简明入门指南，并非完整技术文档的翻译。最新型号和限制请查看
[Supported](../../README.md#supported) 和
[Experimental support](../../README.md#experimental-support)。

## 开始使用

1. 需要 Windows 10/11 x64 和受支持的模拟键盘。
2. 从[官方发布页面](https://github.com/PashOK7/HallJoy/releases)下载 `HallJoy.exe`。
3. 启动程序。如果缺少 ViGEmBus，点击 **Install ViGEmBus 1.22.0**，
   并确认单独出现的管理员权限请求。
4. 关闭可能冲突的键盘网页配置工具。检查设备和布局，必要时手动选择布局。
5. 在 **Remap** 中分配控制，并使用 **Gamepad Tester** 检查按键行程、
   完整输入范围以及松开后是否归零。

不需要单独安装 Wooting Analog SDK 或 Universal Analog Plugin。

## 安装与个人数据

可[构建可选的 Windows 安装程序](../development/WINDOWS_INSTALLER.md)，提供图标、
快捷方式和保留用户数据的卸载功能。这并不表示官方发布中已经包含安装程序。
在仓库克隆中，`Update.cmd` 将官方 EXE 下载到 `.local/portable`，
`Start.cmd` 启动该副本。Git 会忽略此文件夹，更新会保留设置和配置文件。
安装版通常将数据保存在 `%LOCALAPPDATA%\HallJoy`。

英文指南：[数据与备份](../PORTABLE.md)、[输入与隐私](../INPUT_AND_PRIVACY.md)、
[驾驶控制示例](../DRIVING_SETUP.md)。
[视频](https://youtu.be/MI_ZTS6UFhM) · [支持](https://discord.gg/X5QsZJdN5a)
· [许可证](../../LICENSE) · [贡献规则](../../.github/CONTRIBUTING.md)。
