#!/usr/bin/env python3
"""Read-only K4 HE STM32F401 flash backup; never erase, download or unprotect.

Run only after confirming that the sole STM32 DFU device is the owner's K4 HE.
DFU's ROM serial differs from QMK's serial, so explicit --serial is required.
Two independent uploads must match, including plausible Cortex-M vectors.
External EEPROM is NOT part of this MCU flash backup.
"""
from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path
import re
import struct
import subprocess
import tempfile


def run(argv: list[str]) -> str:
    result = subprocess.run(argv, capture_output=True, text=True, timeout=90)
    text = result.stdout + result.stderr
    if result.returncode:
        raise RuntimeError(text.strip())
    return text


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--dfu-util', default='C:/msys64/ucrt64/bin/dfu-util.exe')
    parser.add_argument('--serial', required=True, help='exact ROM DFU serial, from --list')
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    output = args.output.resolve()
    metadata = output.with_suffix(output.suffix + '.json')
    if output.exists() or metadata.exists() or not output.parent.is_dir():
        raise RuntimeError('Output and metadata must be new files in an existing directory')
    listing = run([args.dfu_util, '--list'])
    lines = [line for line in listing.splitlines() if 'Found DFU: [0483:df11]' in line]
    serials = {re.search(r'serial="([^"]+)"', line).group(1) for line in lines
               if re.search(r'serial="([^"]+)"', line)}
    if serials != {args.serial}:
        raise RuntimeError('Expected exactly one confirmed STM32 DFU device/serial; got ' + repr(serials))
    flash = [line for line in lines if 'alt=0,' in line and '@Internal Flash' in line]
    if len(flash) != 1 or '0x08000000' not in flash[0]:
        raise RuntimeError('Internal flash alternate setting did not match K4 target')
    with tempfile.TemporaryDirectory(prefix='halljoy-k4-readback-') as temporary:
        copies = []
        for n in range(2):
            target = Path(temporary) / f'read-{n}.bin'
            run([args.dfu_util, '-d', '0483:df11', '-S', args.serial,
                 '-a', '0', '-s', '0x08000000:262144', '-U', str(target)])
            copies.append(target.read_bytes())
        if len(copies[0]) != 262144 or copies[0] != copies[1]:
            raise RuntimeError('Two 256 KiB flash reads did not match')
        stack, reset = struct.unpack_from('<II', copies[0])
        if not (0x20000000 < stack <= 0x20010000 and stack % 4 == 0 and
                reset & 1 and 0x08000000 <= (reset & ~1) < 0x08040000):
            raise RuntimeError('Unexpected Cortex-M4 vectors; not publishing a rollback image')
        digest = hashlib.sha256(copies[0]).hexdigest()
        # Exclusive creates prevent overwriting an existing owner backup.
        with output.open('xb') as stream:
            stream.write(copies[0])
        if output.read_bytes() != copies[0]:
            raise RuntimeError('Backup file verification failed')
        record = {'serial': args.serial, 'bytes': len(copies[0]), 'sha256': digest,
                  'matching_reads': 2, 'scope': 'Internal MCU flash only, excludes external EEPROM',
                  'dfu_listing': listing}
        with metadata.open('x', encoding='utf-8') as stream:
            json.dump(record, stream, indent=2)
            stream.write('\n')
        print(f'Verified read-only backup: {output}\nSHA256 {digest}')


if __name__ == '__main__':
    main()
