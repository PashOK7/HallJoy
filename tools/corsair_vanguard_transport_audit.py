"""Review file/session framing and exact layout maps offline, without HID access."""
import argparse
import json
from pathlib import Path
import shutil
import struct

from unicorn.arm_const import UC_ARM_REG_R0, UC_ARM_REG_R1, UC_ARM_REG_R2, UC_ARM_REG_R3
from corsair_vanguard_firmware_audit import (Replay, BASE, IMAGE_NAME, IMAGE_HASH,
                                           REQUEST, REPLY, KEY, digest, require)


class FileReplay(Replay):
    def __init__(self, image, layout):
        super().__init__(image, layout)
        self.files = {0x4B: image[0x08085A1C - BASE:0x08085A1C - BASE + 415]}
        self.next_heap = 0x200B0000
        self.open_id = None
        self.read_offset = 0
        self.blocked_write = False

    def peripheral(self, u, address, size, user):
        if address == 0x08020400:
            n = u.reg_read(UC_ARM_REG_R0)
            require(0 < n < 0x3000, "unexpected allocation")
            p = self.next_heap
            self.next_heap += (n + 31) & ~31
            require(self.next_heap < 0x200D0000, "modeled heap exhausted")
            self.calls.append(["alloc", n])
            self.ret(p)
        elif address == 0x0802045E:
            self.calls.append(["free"])
            self.ret()
        elif address == 0x08077934:
            name = bytes(u.mem_read(u.reg_read(UC_ARM_REG_R0), 12)).split(b"\0")[0]
            require(name == b"ID004B.hex", "unexpected exists query")
            self.calls.append(["exists", name.decode(), 0x4B in self.files])
            out_size = u.reg_read(UC_ARM_REG_R1)
            if out_size and 0x4B in self.files:
                u.mem_write(out_size, struct.pack("<I", len(self.files[0x4B])))
            self.ret(0 if 0x4B in self.files else 6)
        elif address == 0x08077510:
            # No fake successful persistent write. Stop at the actual write API.
            self.blocked_write = True
            raise RuntimeError("PERSISTENT_WRITE_BOUNDARY")
        elif address == 0x08077B74:
            name = bytes(u.mem_read(u.reg_read(UC_ARM_REG_R0), 12)).split(b"\0")[0]
            require(name == b"ID004B.hex", "unexpected file open")
            mode = u.reg_read(UC_ARM_REG_R1)
            require(mode == 1, "non-read-only file open")
            self.calls.append(["open", name.decode(), mode])
            self.open_id, self.read_offset = 0x4B, 0
            p, metadata = 0x200D1000, 0x200D1100
            u.mem_write(p, bytes(64))
            u.mem_write(p + 0x10, struct.pack("<I", metadata))
            u.mem_write(metadata + 4, struct.pack("<I", len(self.files[0x4B])))
            self.ret(p)
        elif address == 0x08077ED0:
            require(self.open_id in self.files, "read without file")
            n = u.reg_read(UC_ARM_REG_R2)
            data = self.files[self.open_id][self.read_offset:self.read_offset + n]
            u.mem_write(u.reg_read(UC_ARM_REG_R1), data)
            u.mem_write(u.reg_read(UC_ARM_REG_R3), struct.pack("<I", len(data)))
            self.read_offset += len(data)
            self.calls.append(["read", n, len(data)])
            self.ret()
        elif address == 0x08077340:
            self.calls.append(["close"])
            self.open_id = None
            self.ret()

    def frame(self, command, payload=b""):
        self.calls.clear()
        req = bytes([0, 1, 1, command]) + payload
        self.u.mem_write(REQUEST, req.ljust(1024, b"\0"))
        self.u.reg_write(UC_ARM_REG_R0, REQUEST)
        self.execute(0x08066FA0)
        reply = bytes(self.u.mem_read(REPLY, 1024))
        require(reply[:4] == bytes([0, 0, 1, command]), "unexpected reply framing")
        return reply


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("inputs", type=Path)
    parser.add_argument("output", type=Path)
    a = parser.parse_args()
    require(not a.output.exists(), "output must be new")
    require(digest(a.inputs / IMAGE_NAME) == IMAGE_HASH, "wrong image")
    image = (a.inputs / IMAGE_NAME).read_bytes()
    literals = json.loads((a.inputs / "webhub-layout-literals.json").read_text())
    result = dict(layouts=[], file_transport=[], limitations=[
        "Synthetic session, heap and filesystem. Real USB is not modeled.",
        "Creation guard is exercised before any persistent-write body.",
        "Directory checking cannot prevent concurrent external deletion; close configurators.",
        "No runtime implementation, hardware timing or supported-status claim."])
    for layout, (name, definition) in enumerate(literals.items(), 1):
        r = Replay(image, layout)
        keys, grouped = [], {}
        for key in definition["keys"]:
            code = key["code"]
            if 130 <= code <= 135:  # official non-actuation Game Mode/G1..G5
                continue
            sensor = r.call(0x0805A5A4, code)
            factory = r.call(0x08070474, sensor)
            r.call(0x08065A28, code, 5)
            explicit = bytes(r.u.mem_read(KEY + 64 * sensor + 9, 1))[0]
            hid = code if code < 105 else ({105:224, 106:225, 107:226,
                108:227, 109:228, 110:229, 111:230}.get(code))
            # Vendor 115..119 are Intl1..5; Fn/StreamDeck are physical special
            # keys, not invented USB keyboard usages.
            if 115 <= code <= 119:
                hid = 135 + code - 115
            keys.append(dict(code=code, name=key.get("name"), sensor=sensor,
                             hid=hid, factory_curve=factory, explicit_factory_curve=explicit))
            grouped.setdefault(sensor, []).append(code)
        collisions = {str(k):v for k,v in grouped.items() if len(v) != 1}
        result["layouts"].append(dict(layout=layout, name=name, count=r.count,
            keys=keys, collisions=collisions,
            unused_sensor_slots=sorted(set(range(r.count)) - set(grouped)),
            result="hold" if collisions else "unambiguous"))
    r = FileReplay(image, 1)
    # File listing index contains actual IDs; execute the real snapshot builder
    # and host open/describe/read routing, with no directory builder stub.
    ids = [0x4B, 0xCA, 0x6D91]
    for i, file_id in enumerate(ids):
        r.u.mem_write(0x20084A84 + 0x410 + i * 8, struct.pack("<HHI", file_id, 1, 415))
    reply = r.frame(13, bytes([0, 0x11, 0]))
    require(reply[4] == 0, "directory open failed")
    result["file_transport"].append(dict(stage="directory_open", calls=list(r.calls)))
    reply = r.frame(9, b"\0")
    require(reply[4] == 0, "directory describe failed")
    result["file_transport"].append(dict(stage="directory_describe", prefix=reply[:15].hex()))
    reply = r.frame(8, b"\0")
    require(reply[4] == 0, "directory read failed")
    header, count = struct.unpack_from("<HH", reply, 5)
    found = list(struct.unpack_from(f"<{count}H", reply, 9))
    require(header == 0xAB5B and found == ids, f"directory format: {header:x}/{found}")
    result["file_transport"].append(dict(stage="directory_read", ids=found))
    # Different handle: avoid replacing the listing allocation/handle.
    reply = r.frame(13, bytes([1, 0x4B, 0]))
    require(reply[4] == 0, "existing switch file open failed")
    result["file_transport"].append(dict(stage="switch_open_existing", calls=list(r.calls)))
    reply = r.frame(9, b"\1")
    require(reply[4] == 0, "switch describe failed")
    result["file_transport"].append(dict(stage="switch_describe", prefix=reply[:15].hex()))
    reply = r.frame(8, b"\1")
    require(reply[4] == 0 and reply[5:420] == r.files[0x4B], "switch file read mismatch")
    result["file_transport"].append(dict(stage="switch_read_existing", calls=list(r.calls), bytes=415))
    r = FileReplay(image, 1)
    r.files.clear()
    try:
        r.frame(13, bytes([1, 0x4B, 0]))
    except RuntimeError as error:
        require(str(error) == "PERSISTENT_WRITE_BOUNDARY" and r.blocked_write,
                "unexpected missing-file failure")
    else:
        raise RuntimeError("missing switch file did not expose its write boundary")
    result["file_transport"].append(dict(stage="switch_open_missing",
        outcome="persistent_write_boundary_confirmed_and_blocked", calls=list(r.calls)))
    a.output.mkdir(parents=True, exist_ok=False)
    sources = [Path(__file__), Path(__file__).with_name("corsair_vanguard_firmware_audit.py"),
        a.inputs / IMAGE_NAME, a.inputs / "hub.js", a.inputs / "literal_layout_probe.py",
        a.inputs / "webhub-layout-literals.json"]
    for source in sources:
        old_hash = digest(source)
        shutil.copyfile(source, a.output / source.name)
        require(digest(source) == old_hash == digest(a.output / source.name), "source changed")
    with (a.output / "raw.json").open("x", encoding="utf-8") as out:
        json.dump(result, out, indent=2)
    manifest = {p.name:dict(sha256=digest(p),bytes=p.stat().st_size)
                for p in a.output.iterdir() if p.is_file()}
    with (a.output / "manifest.json").open("x", encoding="utf-8") as out:
        json.dump(manifest, out, indent=2)
    print(json.dumps(dict(layouts=[(r["name"],r["result"]) for r in result["layouts"]],
                          file_transport_cases=len(result["file_transport"]))))


if __name__ == "__main__":
    main()
