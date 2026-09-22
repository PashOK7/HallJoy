#!/usr/bin/env python3
"""Bounded hardware integration probe for HJO1, never probes legacy firmware.

cycle uploads an all-unbound neutral profile, keeps it alive briefly, then
verifies watchdog removal. It does not synthesize key presses or game actions.
bootloader is an explicit maintenance operation on this custom firmware only.
"""
from __future__ import annotations
import argparse
import ctypes
import json
import os
from pathlib import Path
import struct
import time
import zlib


def neutral_profile() -> bytes:
    profile = bytearray(5044)
    profile[:4] = b'HJP1'
    struct.pack_into('<H', profile, 4, len(profile))
    struct.pack_into('<f', profile, 8, .02)
    profile[12:22] = bytes([255]) * 10
    for slot in range(114):
        struct.pack_into('<10fBB', profile, 252 + slot * 42,
                         0, .3, .7, 1, 0, .3, .7, 1, 1, 1, 1, 0)
    struct.pack_into('<I', profile, 5040, zlib.crc32(profile[:5040]))
    return bytes(profile)


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('operation', choices=['status', 'cycle', 'bootloader', 'depth'])
    parser.add_argument('--hid-dll-dir', required=True)
    parser.add_argument('--record', type=Path, required=True)
    parser.add_argument('--seconds', type=float, default=10)
    parser.add_argument('--strict-winmm', action='store_true')
    args = parser.parse_args()
    if args.record.exists():
        raise RuntimeError('Record already exists')
    dll_directory = os.add_dll_directory(args.hid_dll_dir)
    import hid
    device = None
    token = 0
    events = []
    start = time.monotonic()
    def log(kind, **fields):
        events.append({'seconds': time.monotonic()-start, 'event': kind, **fields})
        print(kind, json.dumps(fields), flush=True)
    def connect(revision=None, timeout=15):
        until = time.monotonic() + timeout
        while time.monotonic() < until:
            items = [d for d in hid.enumerate(0x3434,0x0e40)
                     if d['usage_page']==0xff60 and d['usage']==0x61
                     and d['release_number'] in (0x1212,0x1213)
                     and (revision is None or d['release_number']==revision)]
            if len(items)>1:
                raise RuntimeError('More than one onboard K4')
            if items:
                try: return hid.Device(path=items[0]['path'])
                except OSError: pass
            time.sleep(.05)
        raise TimeoutError('Custom K4 did not enumerate')
    def query(command, customize=None):
        packet=bytearray(32); packet[0:2]=bytes([0xa9,command])
        struct.pack_into('<I',packet,4,token)
        if customize: customize(packet)
        if device.write(bytes([0])+packet)!=33: raise IOError('Incomplete HID write')
        reply=bytes(device.read(64,400))
        if len(reply)!=32 or reply[:2]!=packet[:2]: raise IOError('Missing/mismatched response')
        if reply[2]: raise RuntimeError(f'Command {command:02x} status {reply[2]}')
        if command!=0x78 and reply[8:12]!=b'HJO1': raise RuntimeError('Unknown firmware')
        return reply
    xinput=ctypes.WinDLL('xinput1_4.dll')
    xinput.XInputGetState.argtypes=[ctypes.c_uint,ctypes.c_void_p]
    xinput.XInputGetState.restype=ctypes.c_uint
    def controllers():
        result={}
        for index in range(4):
            state=ctypes.create_string_buffer(16)
            if xinput.XInputGetState(index,state)==0: result[index]=state.raw[4:].hex()
        return result
    def legacy_k4_present():
        from ctypes import wintypes as w
        class Caps(ctypes.Structure):
            _fields_=[('mid',w.WORD),('pid',w.WORD),('name',w.WCHAR*32)]+[(n,w.UINT) for n in
                ('xmin','xmax','ymin','ymax','zmin','zmax','buttons','periodmin','periodmax',
                 'rmin','rmax','umin','umax','vmin','vmax','caps','maxaxes','axes','maxbuttons')]+[
                ('regkey',w.WCHAR*32),('oem',w.WCHAR*260)]
        winmm=ctypes.WinDLL('winmm.dll')
        result=[]
        for index in range(16):
            position=ctypes.create_string_buffer(52)
            struct.pack_into('<II',position,0,52,255)
            if winmm.joyGetPosEx(index,position)==0:
                c=Caps()
                if winmm.joyGetDevCapsW(index,ctypes.byref(c),ctypes.sizeof(c))==0 and (c.mid,c.pid)==(0x3434,0x0e40):
                    result.append(index)
        return result
    try:
        device=connect()
        reply=query(0x70)
        log('status',phase=reply[3],native=reply[16],profile=reply[17],
            scan_us=struct.unpack_from('<I',reply,24)[0],controllers=controllers(),winmm_k4=legacy_k4_present())
        if args.operation=='bootloader':
            query(0x79,lambda p:p.__setitem__(slice(4,19),b'HallJoyDFU-K4-v1'))
            log('bootloader_requested')
        elif args.operation=='depth':
            if not 0 < args.seconds <= 300: raise ValueError('seconds must be 0..300')
            if reply[3] or reply[16]: raise RuntimeError('Depth-only test requires idle keyboard')
            until=time.monotonic()+args.seconds
            frames=[]
            while time.monotonic()<until:
                frame=bytearray()
                for page in range(12):
                    reply=query(0x78,lambda p:p.__setitem__(2,page))
                    count=min(22,256-page*22)
                    if reply[8]!=page or reply[9]!=count: raise IOError('Invalid telemetry page')
                    frame.extend(reply[10:10+count])
                if len(frame)!=256 or frame[:6]!=b'HJK4\x01\x01' or struct.unpack_from('<H',frame,6)[0]!=256:
                    raise IOError('Invalid depth frame')
                if struct.unpack_from('<I',frame,252)[0]!=zlib.crc32(frame[:252]):
                    raise IOError('Depth CRC mismatch')
                if not frame[23]&1: raise RuntimeError('Calibration is not ready')
                frames.append({'seconds':time.monotonic()-start,
                    'sequence':struct.unpack_from('<I',frame,12)[0],
                    'scan_us':struct.unpack_from('<H',frame,20)[0],
                    'depth':list(struct.unpack_from('<114H',frame,24))})
            events.append({'event':'depth_frames','frames':frames})
            active=[i for i in range(114) if any(f['depth'][i] for f in frames)]
            log('depth_complete',frames=len(frames),active_slots=active,
                controllers=controllers(),scan_us_min=min(f['scan_us'] for f in frames),
                scan_us_max=max(f['scan_us'] for f in frames))
        elif args.operation=='cycle':
            baseline=controllers()
            reply=query(0x71,lambda p:p.__setitem__(slice(8,12),b'HJO1'))
            token=struct.unpack_from('<I',reply,4)[0]
            device.close(); device=None
            device=connect(0x1213)
            reply=query(0x70)
            if struct.unpack_from('<I',reply,4)[0]!=token or reply[3]!=1:
                raise RuntimeError('Session lost during native enumeration')
            log('native_enumerated',controllers=controllers())
            wire=neutral_profile(); query(0x72)
            for offset in range(0,len(wire),21):
                chunk=wire[offset:offset+21]
                def fill(p):
                    struct.pack_into('<HB',p,8,offset,len(chunk))
                    p[11:11+len(chunk)]=chunk
                query(0x73,fill)
            reply=query(0x74)
            assert struct.unpack_from('<I',reply,12)[0]==zlib.crc32(wire[:5040])
            query(0x75,lambda p:struct.pack_into('<I',p,8,1))
            log('started_neutral')
            for seq in range(2,32):
                time.sleep(.1)
                query(0x76,lambda p:struct.pack_into('<I',p,8,seq))
            active=controllers()
            log('active',controllers=active)
            added=set(active)-set(baseline)
            if len(added)!=1: raise RuntimeError('Expected exactly one added XInput controller')
            if any(active[i]!='00'*12 for i in added): raise RuntimeError('Neutral profile produced input')
            # Intentionally withhold heartbeats, including STOP. Firmware must
            # revoke ownership and remove its controller even with an open HID.
            time.sleep(1)
            device.close(); device=None
            device=connect(0x1212)
            reply=query(0x70)
            after=controllers()
            if reply[3]!=0 or reply[16] or any(i in after for i in added):
                raise RuntimeError('Watchdog did not remove controller')
            remaining=legacy_k4_present()
            log('legacy_winmm_observation',indices=remaining,strict=args.strict_winmm)
            if remaining and args.strict_winmm: raise RuntimeError('K4 remains visible in WinMM: '+repr(remaining))
            log('watchdog_pass',controllers=after,winmm_k4=remaining)
    finally:
        if device is not None:
            if token:
                try: query(0x77)
                except Exception: pass
            device.close()
        with args.record.open('x',encoding='utf-8') as stream:
            json.dump(events,stream,indent=2)
            stream.write('\n')
        dll_directory.close()


if __name__=='__main__': main()
