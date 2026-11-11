# HallJoy — 빠른 시작

[![C++20](https://img.shields.io/badge/C%2B%2B-20-00599C?style=flat-square)](../../src/HallJoyProject/HallJoy/HallJoy.vcxproj)
[![Windows x64](https://img.shields.io/badge/Windows-x64-0078D4?style=flat-square)](../../README.md#quick-start)
[![PowerShell 5.1+](https://img.shields.io/badge/PowerShell-5.1%2B-5391FE?style=flat-square)](../PORTABLE.md)
[![NSIS 3](https://img.shields.io/badge/NSIS-3-4A7C59?style=flat-square)](../development/WINDOWS_INSTALLER.md)

[English](../../README.md) · [Español](README.es.md) · [Français](README.fr.md) · [Italiano](README.it.md) · [简体中文](README.zh-CN.md) · **한국어** · [Русский](README.ru.md) · [Deutsch](README.de.md) · [日本語](README.ja.md) · [हिन्दी](README.hi.md)

HallJoy는 Windows에서 지원되는 아날로그 키보드의 실제 키 이동 깊이를 가상 Xbox
컨트롤러 입력으로 변환합니다. 일반 디지털 키보드를 아날로그 키보드로 바꾸지는
않습니다. 버튼·스틱·트리거 매핑, 반응 곡선, 데드존, 프로필과 Gamepad Tester를
제공합니다. 개발자: **PashOK7**.

이 문서는 간단한 시작 안내이며 전체 기술 문서의 번역은 아닙니다. 최신 모델과
제약 사항은 [Supported](../../README.md#supported) 및
[Experimental support](../../README.md#experimental-support)를 확인하세요.

## 시작하기

1. Windows 10/11 x64와 지원되는 아날로그 키보드가 필요합니다.
2. [공식 릴리스](https://github.com/PashOK7/HallJoy/releases)에서 `HallJoy.exe`를 받으세요.
3. 앱을 실행하세요. ViGEmBus가 없다면 **Install ViGEmBus 1.22.0**을 누르고
   별도로 표시되는 관리자 권한 요청을 승인하세요.
4. 충돌하는 키보드 웹 설정 도구를 닫으세요. 장치와 배열을 확인하고 필요하면
   배열을 직접 선택하세요.
5. **Remap**에서 입력을 배정한 뒤 **Gamepad Tester**에서 키 깊이, 전체 범위,
   키를 놓았을 때 중립으로 돌아오는지 확인하세요.

Wooting Analog SDK나 Universal Analog Plugin을 따로 설치할 필요는 없습니다.

## 설치 및 개인 데이터

아이콘과 바로 가기, 사용자 데이터를 보존하는 제거 기능이 있는
[선택적 설치 프로그램을 빌드](../development/WINDOWS_INSTALLER.md)할 수 있습니다.
공식 릴리스에 설치 프로그램이 이미 배포되었다는 뜻은 아닙니다.
저장소 복제본에서 `Update.cmd`는 공식 EXE를 `.local/portable`에 다운로드하고,
`Start.cmd`는 해당 복사본을 실행합니다. 이 폴더는 Git에서 제외되며 업데이트 시
프로필이 보존됩니다. 설치형은 보통 `%LOCALAPPDATA%\HallJoy`에 데이터를 저장합니다.

영문 안내: [데이터 및 백업](../PORTABLE.md), [개인정보](../INPUT_AND_PRIVACY.md),
[운전 조작 예시](../DRIVING_SETUP.md).
[영상](https://youtu.be/MI_ZTS6UFhM) · [지원](https://discord.gg/X5QsZJdN5a)
· [라이선스](../../LICENSE) · [기여 규칙](../../.github/CONTRIBUTING.md).
