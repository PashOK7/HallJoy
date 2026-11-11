# HallJoy — शुरुआत करें

[![C++20](https://img.shields.io/badge/C%2B%2B-20-00599C?style=flat-square)](../../src/HallJoyProject/HallJoy/HallJoy.vcxproj)
[![Windows x64](https://img.shields.io/badge/Windows-x64-0078D4?style=flat-square)](../../README.md#quick-start)
[![PowerShell 5.1+](https://img.shields.io/badge/PowerShell-5.1%2B-5391FE?style=flat-square)](../PORTABLE.md)
[![NSIS 3](https://img.shields.io/badge/NSIS-3-4A7C59?style=flat-square)](../development/WINDOWS_INSTALLER.md)

[English](../../README.md) · [Español](README.es.md) · [Français](README.fr.md) · [Italiano](README.it.md) · [简体中文](README.zh-CN.md) · [한국어](README.ko.md) · [Русский](README.ru.md) · [Deutsch](README.de.md) · [日本語](README.ja.md) · **हिन्दी**

HallJoy, Windows पर समर्थित एनालॉग कीबोर्ड की कुंजी दबाने की वास्तविक गहराई को
वर्चुअल Xbox कंट्रोलर इनपुट में बदलता है। यह साधारण डिजिटल कीबोर्ड को एनालॉग नहीं
बनाता। इसमें बटन, स्टिक और ट्रिगर मैपिंग, प्रतिक्रिया वक्र, डेडज़ोन, प्रोफ़ाइल और
Gamepad Tester उपलब्ध हैं। लेखक: **PashOK7**।

यह संक्षिप्त शुरुआत मार्गदर्शिका है, सभी तकनीकी दस्तावेज़ों का पूरा अनुवाद नहीं।
नवीनतम मॉडल और सीमाएँ [Supported](../../README.md#supported) तथा
[Experimental support](../../README.md#experimental-support) में देखें।

## पहले कदम

1. Windows 10/11 x64 और समर्थित एनालॉग कीबोर्ड आवश्यक हैं।
2. [आधिकारिक रिलीज़](https://github.com/PashOK7/HallJoy/releases) से `HallJoy.exe` डाउनलोड करें।
3. ऐप खोलें। ViGEmBus न होने पर **Install ViGEmBus 1.22.0** चुनें और अलग से आने
   वाले व्यवस्थापक अनुमति अनुरोध को स्वीकार करें।
4. टकराव पैदा करने वाले कीबोर्ड वेब-कॉन्फ़िगरेटर बंद करें। डिवाइस और लेआउट जाँचें;
   आवश्यकता हो तो लेआउट स्वयं चुनें।
5. **Remap** में नियंत्रण निर्धारित करें। **Gamepad Tester** में दबाव की गहराई,
   पूरी इनपुट सीमा और कुंजी छोड़ने पर शून्य पर वापसी जाँचें।

Wooting Analog SDK या Universal Analog Plugin अलग से स्थापित करना आवश्यक नहीं है।

## स्थापना और निजी डेटा

आइकन, शॉर्टकट और निजी डेटा सुरक्षित रखने वाली अनइंस्टॉल सुविधा के साथ
[वैकल्पिक इंस्टॉलर बनाया जा सकता है](../development/WINDOWS_INSTALLER.md)। इसका अर्थ
यह नहीं है कि आधिकारिक रिलीज़ में इंस्टॉलर पहले से उपलब्ध है।
रिपॉज़िटरी के क्लोन में `Update.cmd` आधिकारिक EXE को `.local/portable` में डाउनलोड
करता है और `Start.cmd` उसे चलाता है। यह फ़ोल्डर Git से बाहर रहता है और अपडेट
प्रोफ़ाइल सुरक्षित रखते हैं। स्थापित कॉपी सामान्यतः `%LOCALAPPDATA%\HallJoy` में डेटा रखती है।

अंग्रेज़ी मार्गदर्शिकाएँ: [डेटा और बैकअप](../PORTABLE.md),
[गोपनीयता](../INPUT_AND_PRIVACY.md), [ड्राइविंग नियंत्रण](../DRIVING_SETUP.md)।
[वीडियो](https://youtu.be/MI_ZTS6UFhM) · [सहायता](https://discord.gg/X5QsZJdN5a)
· [लाइसेंस](../../LICENSE) · [योगदान के नियम](../../.github/CONTRIBUTING.md)।
