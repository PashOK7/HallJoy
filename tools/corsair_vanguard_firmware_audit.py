"""Bounded offline review of the exact Vanguard 2.8.171 image; never accesses HID.

Vendor inputs and the resulting bundle remain local. Synthetic peripherals are
explicit boundaries, not claims of hardware execution or typing compatibility.
"""
import argparse
import hashlib
import importlib.metadata
import json
from pathlib import Path
import shutil
import struct

from unicorn import UC_HOOK_CODE
from unicorn.arm_const import (UC_ARM_REG_LR, UC_ARM_REG_PC, UC_ARM_REG_R0,
                               UC_ARM_REG_R1, UC_ARM_REG_R2, UC_ARM_REG_R3,
                               UC_ARM_REG_R6)
from firmware_replay import thumb_machine, run_until

BASE = 0x08020000
IMAGE_HASH = "72131be2bb9509a609067d5ceb76575fb023fdef0e9f968108064351b9ae87f7"
IMAGE_NAME = "fw-VANGUARDPRO96_App_v2.8.171.bin"
REPLY, REQUEST, KEY = 0x2008001C, 0x200E0000, 0x20051EA0
LAYOUT_ADDR, COUNT_ADDR, GATE = 0x20016E14, 0x20087DBE, 0x200002F1


def digest(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def require(condition, detail):
    if not condition:
        raise RuntimeError(detail)


class Replay:
    def __init__(self, image, layout):
        self.image, self.layout = image, layout
        self.count = image[0x08092D58 - BASE + layout * 4]
        self.u = thumb_machine(image, BASE, ram_size=0x100000)
        self.u.mem_write(LAYOUT_ADDR, bytes([layout]))
        self.u.mem_write(COUNT_ADDR, bytes([self.count]))
        self.u.mem_write(0x20017734 + 28 + 4, struct.pack("<I", 0x200D0000))
        self.u.mem_write(0x20051234 + 4, struct.pack("<I", 120))
        self.file_mode = "ok"
        self.blob = bytearray(719)
        for i in range(self.count):
            struct.pack_into("<H", self.blob, i * 2, 6200 + i)
            struct.pack_into("<H", self.blob, 256 + i * 2, 2100 + i)
            self.blob[512 + i] = (i * 3) % 64
            self.blob[512 + self.count + i] = (i * 7) % 128
        self.calls = []
        self.u.hook_add(UC_HOOK_CODE, self.peripheral)

    def ret(self, value=0):
        self.u.reg_write(UC_ARM_REG_R0, value)
        self.u.reg_write(UC_ARM_REG_PC, self.u.reg_read(UC_ARM_REG_LR))

    def peripheral(self, u, address, size, user):
        if address == 0x08068738:  # EEPROM read, no EEPROM writes modeled
            require(u.reg_read(UC_ARM_REG_R0) == 0, "unexpected EEPROM address")
            n = u.reg_read(UC_ARM_REG_R2)
            require(n in (719, 768), "unexpected EEPROM read length")
            u.mem_write(u.reg_read(UC_ARM_REG_R1), bytes(self.blob).ljust(n, b"\0"))
            self.calls.append(["eeprom_read", n])
            self.ret()
        elif address == 0x08077B74:  # saved calibration open
            name = bytes(u.mem_read(u.reg_read(UC_ARM_REG_R0), 12)).split(b"\0")[0]
            mode = u.reg_read(UC_ARM_REG_R1)
            require(name == b"ID00CA.hex" and mode == 1, "unexpected file open")
            self.calls.append(["open", name.decode(), mode])
            self.ret(0 if self.file_mode == "missing" else 0x200D0100)
        elif address == 0x08077ED0:
            n = u.reg_read(UC_ARM_REG_R2)
            require(n == 719, "unexpected saved calibration read size")
            failed = self.file_mode == "read_error"
            u.mem_write(u.reg_read(UC_ARM_REG_R1), bytes(self.blob))
            u.mem_write(u.reg_read(UC_ARM_REG_R3), struct.pack("<I", 0 if failed else n))
            self.calls.append(["file_read", n, "error" if failed else "ok"])
            self.ret(1 if failed else 0)
        elif address == 0x08077340:
            self.calls.append(["close", u.reg_read(UC_ARM_REG_R0)])
            self.ret()

    def execute(self, entry, stops=None, budget=150000):
        self.u.reg_write(UC_ARM_REG_LR, BASE | 1)
        return run_until(self.u, entry, stops or {BASE}, budget=budget)

    def command(self, prop, sub=None, session=1):
        self.calls.clear()
        req = bytes([0, 1, session, 2, prop & 255, prop >> 8] + ([] if sub is None else [sub]))
        self.u.mem_write(REQUEST, req.ljust(1024, b"\0"))
        self.u.reg_write(UC_ARM_REG_R0, REQUEST)
        self.execute(0x08066FA0)
        require(bytes(self.u.mem_read(GATE, 1)) == b"\0", "scan gate left set")
        return bytes(self.u.mem_read(REPLY, 1024))

    def call(self, entry, *args):
        for reg, value in zip((UC_ARM_REG_R0, UC_ARM_REG_R1, UC_ARM_REG_R2), args):
            self.u.reg_write(reg, value)
        self.execute(entry)
        return self.u.reg_read(UC_ARM_REG_R0)

    def depth(self, index, rest, bottom, raw, curve):
        address = KEY + 64 * index
        self.u.mem_write(address + 4, struct.pack("<HHBB", rest, bottom,
                                                 (rest - bottom) // 100, curve))
        self.u.mem_write(address + 14, struct.pack("<H", raw))
        self.u.reg_write(UC_ARM_REG_R6, address)
        self.execute(0x08067A4E, {0x08067A92, 0x08067B00})
        # The zero branch reaches B00 before its MOVS R0,#0.
        if self.u.reg_read(UC_ARM_REG_PC) == 0x08067B00:
            return 0
        return self.u.reg_read(UC_ARM_REG_R0)


def host_depth(rest, bottom, raw, curve):
    if raw >= rest:
        return 0
    scale = (rest - bottom) // 100
    return next((n for n in range(40, 0, -1)
                 if rest - raw > curve[n] * scale // 10), 0)


def run(image):
    result = {"calibration": [], "layouts": [], "normalization": {}, "limits": [
        "Synthetic RAM/calibration/current ADC; no real boot, USB or ADC.",
        "EEPROM/file functions are explicit mocks. Real filesystem timing is not executed.",
        "Normalized depth fragment runs before later firmware filtering/rapid-trigger logic.",
        "Mapping is protocol-index to sensor-index, not a completed USB HID/physical-key review.",
        "No runtime integration, hardware feedback, throughput or support-status claim."]}
    samples = 0
    curves = [list(struct.unpack_from("<41H", image, 0x08092F7A - BASE + i * 82))
              for i in range(18)]
    result["curves"] = curves
    for layout in range(1, 5):
        r = Replay(image, layout)
        for sub in range(1, 7):
            reply = r.command(0xC6, sub)
            source = sub if sub <= 3 else sub - 3
            if source == 1:
                expected = bytes(r.blob[512:512 + r.count * 2])
            else:
                offset = 256 if source == 2 else 0
                expected = bytes(r.blob[offset:offset + r.count * 2])
            require(reply[3:3 + len(expected)] == expected, f"C6/{sub} layout {layout}")
            require(reply[:3] == bytes([0, 0, 1]), "C6 session prefix")
            result["calibration"].append(dict(layout=layout, sub=sub, outcome="pass",
                bytes=len(expected), offset=3, calls=list(r.calls)))
        for mode in ("missing", "read_error"):
            r.file_mode = mode
            for sub in (4, 5, 6):
                reply = r.command(0xC6, sub)
                require(reply[3:3 + 2 * r.count] == bytes(2 * r.count), "failed C6 must return zero data")
                result["calibration"].append(dict(layout=layout, sub=sub,
                    file=mode, outcome="pass", all_zero=True, calls=list(r.calls)))
        r.file_mode = "ok"
        # Every current value distinct, including rest, shallow and bottom at once.
        currents = [6200 + i - (i * 43) % 4101 for i in range(r.count)]
        for i, current in enumerate(currents):
            r.u.mem_write(KEY + i * 64 + 14, struct.pack("<H", current))
        reply = r.command(0xC7)
        require(list(struct.unpack_from(f"<{r.count}H", reply, 5)) == currents, "all-key C7")
        require(not r.calls, "C7 unexpectedly uses calibration peripherals")
        r.u.mem_write(0x20017734 + 56 + 4, struct.pack("<I", 0x200D0200))
        bad = r.command(0xC7, session=2)
        # Session 2 has a seeded context but no valid session timeout.
        require((bad[1] & 15) != 0, "expired session accepted")
        mapping = [r.call(0x0805A5A4, key) for key in range(139)]
        factory_curves = [r.call(0x08070474, index) for index in range(r.count)]
        switch_curves = {}
        for switch in range(6):
            selected = []
            for key in range(4, 139):
                sensor = mapping[key]
                r.call(0x08065A28, key, switch)
                selected.append(bytes(r.u.mem_read(KEY + 64 * sensor + 9, 1))[0])
            require(all(c < len(curves) for c in selected), "unreviewed curve number")
            switch_curves[str(switch)] = selected
        result["layouts"].append(dict(id=layout, count=r.count, protocol_to_sensor=mapping,
            factory_curves=factory_curves, switch_curves=switch_curves,
            all_key_snapshot="pass", invalid_session="pass"))
        # The actual firmware fragment and host formula independently consume
        # the firmware's table. Boundaries on either side of every knot, every
        # transfer class and two calibration scales are compared.
        # This fragment takes R6 directly and never reads a key index. Replay
        # each distinct transfer class once, rather than repeating identical
        # boundary sweeps hundreds of times through Unicorn's timeout threads.
        for curve_id in (range(len(curves)) if layout == 1 else ()):
            index = curve_id
            curve = curves[curve_id]
            for rest, bottom in ((6200, 2100), (7831, 3529)):
                scale = (rest - bottom) // 100
                deltas = {0, 1, rest - bottom, rest - bottom + 1}
                for knot in curve:
                    threshold = knot * scale // 10
                    deltas.update((max(0, threshold - 1), threshold, threshold + 1))
                for delta in sorted(deltas):
                    raw = max(0, rest - delta)
                    actual = r.depth(index, rest, bottom, raw, curve_id)
                    expected = host_depth(rest, bottom, raw, curve)
                    require(actual == expected, f"depth mismatch {layout}/{index}/{raw}: {actual}!={expected}")
                    samples += 1
    result["normalization"] = dict(outcome="pass", comparisons=samples,
        boundary="0x08067A4E to 0x08067A92 or 0x08067B00; before later filtering",
        nominal_depth_steps=41)
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("inputs", type=Path)
    parser.add_argument("output", type=Path)
    args = parser.parse_args()
    require(not args.output.exists(), "output must be a new directory")
    image_path = args.inputs / IMAGE_NAME
    require(digest(image_path) == IMAGE_HASH, "wrong firmware image")
    source = Path(__file__)
    source_hash = digest(source)
    args.output.mkdir(parents=True, exist_ok=False)
    shutil.copyfile(image_path, args.output / IMAGE_NAME)
    shutil.copyfile(source, args.output / source.name)
    raw = run(image_path.read_bytes())
    require(digest(source) == source_hash, "audit source changed during execution")
    with (args.output / "raw.json").open("x", encoding="utf-8") as out:
        json.dump(raw, out, indent=2)
    files = [p for p in args.output.iterdir() if p.is_file()]
    manifest = dict(schema=1, image=IMAGE_HASH, source=source_hash,
        dependencies={name: importlib.metadata.version(name) for name in ("unicorn", "capstone")},
        assets={p.name: dict(sha256=digest(p), bytes=p.stat().st_size) for p in files})
    with (args.output / "manifest.json").open("x", encoding="utf-8") as out:
        json.dump(manifest, out, indent=2)
    print(json.dumps(dict(calibration_cases=len(raw["calibration"]),
                         layouts=len(raw["layouts"]), **raw["normalization"])))


if __name__ == "__main__":
    main()
