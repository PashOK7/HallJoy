"""Replay receiver mailbox handoff. Sibling0801, not exact Keydous0802 hardware."""
from pathlib import Path
import hashlib
import json
import sys

from firmware_replay import thumb_machine, run_until
from unicorn.arm_const import UC_ARM_REG_R4

ROOT = Path(__file__).resolve().parents[1]
IMAGE = ROOT / '.local/research/keydous-nj98v4s-firmware-20261114/alternate-01/receiver-family/firmwares__DONGLE_RY6108_RF_KB_V903__dfu_dumps__dongle_working_256k.bin'
STATE, USB, SPI = 0x20000330, 0x20018000, 0x20000834


def main():
    output = Path(sys.argv[1])
    assert not output.exists(), output
    image = IMAGE.read_bytes()
    sha = hashlib.sha256(image).hexdigest()
    assert sha == 'ad3ab42828cf31ed9a7a1e7646462621c556d27ece5dfce590a11152e4ba2f55'
    machine = thumb_machine(image, 0x08000000)

    def run(start, stop):
        run_until(machine, start, {stop}, budget=10000)

    def stage(payload):
        machine.mem_write(USB + 12, payload)
        machine.reg_write(UC_ARM_REG_R4, USB)
        run(0x08007084, 0x0800709A)
        assert bytes(machine.mem_read(STATE + 0x41, 65)) == b'\1' + payload

    rows = []
    for opcode in (0x8F, 0xE5, 0x1B):
        for page in range(4) if opcode == 0xE5 else (0,):
            request = bytearray(64)
            request[0] = opcode
            if opcode == 0xE5:
                request[1:4] = bytes((0xFE, 1, page))
            if opcode == 0x1B:
                request[1] = 1
            request[7] = (255 - sum(request[:7])) & 255
            stage(bytes(request))
            # Actual SET_REPORT staging overwrites the same single mailbox.
            status = b'\xf7' + bytes(63)
            stage(status)
            assert bytes(machine.mem_read(STATE + 0x42, 64)) == status
            # Re-stage original and execute the ACTUAL generic forwarding branch.
            marker = bytes((0xF0, 1, 8, 0, 0, 0, 0, 129)) + bytes(56)
            machine.mem_write(STATE + 1, marker)
            machine.mem_write(STATE + 0x8A, b'\xa5' * 64)
            machine.mem_write(0x20000004, b'\0')
            stage(bytes(request))
            assert bytes(machine.mem_read(STATE + 1, 64)) == marker
            run(0x080066F2, 0x08006796)
            assert bytes(machine.mem_read(SPI + 0x50, 64)) == request
            assert bytes(machine.mem_read(SPI + 1, 1)) == b'\1'
            # The final memset clears the RF reply cache, NOT the USB reply.
            # Thus a passive GET cannot acknowledge this outbound command.
            assert bytes(machine.mem_read(STATE + 1, 64)) == marker
            assert bytes(machine.mem_read(STATE + 0xD8, 2)) == b'\0\0'
            assert bytes(machine.mem_read(STATE + 0x8A, 64)) == bytes(64)
            rows.append(dict(opcode=hex(opcode), page=page,
                staged_command_overwritten_by_F7=True, usb_reply_unchanged_until_worker=True,
                worker_forwards_exact_request=True, usb_reply_unchanged_after_forward=True,
                passive_usb_handoff_ack_absent=True, previous_rf_cache_cleared=True))
    result = dict(schema=1, image_sha256=sha, source_sha256=hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),
        exact_keydous=False, boundaries=dict(set_report=['08007084', '0800709a'], forward=['080066f2', '08006796']),
        scope='Actual sibling branch bytes; synthetic SRAM. No full worker/boot/USB/SPI/RF scheduling, target0802 or ADC execution.', rows=rows)
    with output.open('x', encoding='utf-8') as stream:
        json.dump(result, stream, indent=2)
    print('KEYDOUS_COMMAND_HANDOFF_REPLAY=PASS cases=6 mailbox_overwrite=1 passive_forwarding_ack=absent exact_keydous=0')


if __name__ == '__main__':
    main()
