#!/usr/bin/env python3
"""Explicit bounded pre-gate ADC capture on HJO1 r5; RAM only, no calibration writes."""
import argparse,json,os,struct,time,statistics
from pathlib import Path
p=argparse.ArgumentParser(description=__doc__)
p.add_argument('--hid-dll-dir',required=True);p.add_argument('--slot',required=True,type=int)
p.add_argument('--record',required=True,type=Path)
a=p.parse_args()
if a.record.exists() or not 0<=a.slot<114:raise ValueError('New output and valid slot required')
dll=os.add_dll_directory(a.hid_dll_dir)
import hid
items=[d for d in hid.enumerate(0x3434,0x0e40) if d['usage_page']==0xff60 and d['usage']==0x61 and d['release_number']==0x1212 and (d.get('serial_number') or '').endswith('HJO1')]
if len(items)!=1:raise RuntimeError('Exactly one idle custom K4 required; close HallJoy first')
with hid.Device(path=items[0]['path']) as device:
 def query(command,index=0):
  packet=bytearray(32);packet[0:2]=bytes([0xa9,command])
  if command==0x7c:packet[2]=a.slot;packet[8:12]=b'RAW1'
  if command==0x7d:struct.pack_into('<H',packet,12,index)
  if device.write(bytes([0])+packet)!=33:raise IOError('Incomplete write')
  r=bytes(device.read(64,400))
  if len(r)!=32 or r[:2]!=packet[:2] or r[2] or r[8:12]!=b'HJO1':raise IOError('Invalid response')
  return r
 status=query(0x70)
 if status[3] or status[16] or not status[17]&8:raise RuntimeError('Idle precision-capable firmware required')
 query(0x7c);deadline=time.monotonic()+8
 while True:
  r=query(0x7d)
  if struct.unpack_from('<H',r,12)[0]==512:break
  if time.monotonic()>deadline:raise TimeoutError('Capture did not finish')
  time.sleep(.05)
 rows=[];first=None
 for i in range(512):
  r=query(0x7d,i);count,index,seq=struct.unpack_from('<HHI',r,12)
  if count!=512 or index!=i:raise IOError('Capture changed')
  if first is None:first=seq
  if seq!=(first+i)&0xffffffff:raise IOError('Sequence changed')
  raw,depth,scan=struct.unpack_from('<HHH',r,20);zero,full=struct.unpack_from('<HH',r,28)
  rows.append(dict(sequence=seq,raw=raw,depth16=depth,scan_us=scan,legacy_travel=r[26],valid=r[27],zero=zero,full=full))
raw=[r['raw'] for r in rows];depth=[r['depth16'] for r in rows]
summary=dict(raw_min=min(raw),raw_max=max(raw),raw_stdev=statistics.pstdev(raw),raw_unique=len(set(raw)),depth_min=min(depth),depth_max=max(depth),depth_unique=len(set(depth)),legacy_unique=len(set(r['legacy_travel'] for r in rows)),scan_min=min(r['scan_us'] for r in rows),scan_max=max(r['scan_us'] for r in rows),invalid=sum(not r['valid'] for r in rows),calibrations=len(set((r['zero'],r['full']) for r in rows)))
with a.record.open('x',encoding='utf-8') as f:json.dump(dict(slot=a.slot,summary=summary,frames=rows),f,indent=2)
print(json.dumps(summary),flush=True)
