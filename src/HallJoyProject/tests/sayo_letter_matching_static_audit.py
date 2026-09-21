#!/usr/bin/env python3
"""The config reader supersedes historical digital-event binding inference."""
from pathlib import Path
hall=Path(__file__).resolve().parents[1]/'HallJoy'
s=(hall/'backend_sayo.inc').read_text(encoding='utf-8-sig')
b=(hall/'backend.cpp').read_text(encoding='utf-8-sig')
assert 'SayoLetterMatcher' not in s and 'SayoParseKeyboardReport' not in s
assert 'SayoSetIndexState' not in s and 'SayoParseAnalogReport' not in s
assert 'so3c::Binding(frame,i,hid)' in s and 'so3c::Identity(frame)' in s
assert 'so3c::Parse(data,len,frame)' in s and 'so3c::Depth(frame,raw)' in s
assert 'so3c::Normalize(raw[i])' in s
assert 'UsesRemapping' in s and 'so3c::Factory[index]' in s
assert 'g_sayoLayoutToken.exchange(0)' in s
assert 'SaturatingAgeMs(GetTickCount64(), last) > kSayoDepthFreshMs' in b
assert 'value=std::max(value,g_sayoPhysicalMilli[i]' in b
print('SAYO_CONFIG_MAPPING_STATIC_AUDIT=PASS')
