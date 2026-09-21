"""Generate exact RY5088 identities and factory maps from reviewed client records."""
from pathlib import Path
import argparse
import json

ROOT = Path(__file__).resolve().parents[1]
SOURCE = ROOT / 'docs/research/attackshark-family-profiles-20260920.json'
TARGET = ROOT / 'src/HallJoyProject/HallJoy/attackshark_pro_native_model.h'
BEGIN = '// BEGIN GENERATED FAMILY PROFILES\n'
END = '// END GENERATED FAMILY PROFILES\n'


def decode(raw):
    assert len(raw) == 512 and all(0 <= n <= 255 for n in raw)
    result = []
    for i in range(0, 512, 4):
        record = raw[i:i+4]
        if record == [10, 1, 0, 0]:
            result.append(1033)
        elif record[0:2] == [0, 0] and record[3] == 0 and (
                4 <= record[2] <= 115 or 224 <= record[2] <= 231):
            result.append(record[2])
        else:
            result.append(0)  # Consumer, macro, lighting and encoder actions.
    return result


def generated():
    records = json.loads(SOURCE.read_text(encoding='utf8'))
    assert len(records) == 37 and len({r['id'] for r in records}) == 37
    # Preserve the historical default profile used by component tests.
    records.sort(key=lambda r: (r['id'] != 2308, r['id']))
    names = {2308: 'X65 Pro HE', 2938: 'X65 Pro HE',
             2370: 'X68 Pro HE', 2901: 'X68 Pro HE',
             2356: 'X82 Pro HE', 2935: 'X82 Pro HE'}
    lines = [BEGIN.rstrip(), '// Source: docs/research/attackshark-family-profiles-20260920.json',
             'inline constexpr Profile Profiles[]={']
    for r in records:
        assert r['vid'] == 0x3151 and r['pid'] in (0x5029, 0x502d, 0x502f, 0x5030)
        factory, fn = decode(r['factory']), decode(r['fn'])
        assert factory.count(1033) == 1 and factory[127] == 0
        assert [factory[i] for i in (14, 9, 15, 21)] == [26, 4, 22, 7]
        name = names.get(r['id'], r['display'])
        lines += [f"    // {r['chunk']} SHA256 {r['sha256']}",
                  f'    {{{r["id"]},0x{r["pid"]:04x},{factory.index(1033)},L"ATTACK SHARK {name}",',
                  '     {' + ','.join(map(str, factory)) + '},',
                  '     {' + ','.join(map(str, fn)) + '}},']
    return '\n'.join(lines + ['};', END.rstrip(), ''])


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--check', action='store_true')
    args = parser.parse_args()
    before = TARGET.read_bytes()
    text = before.decode('utf8').replace('\r', '')
    assert text.count(BEGIN) == text.count(END) == 1
    start, end = text.index(BEGIN), text.index(END) + len(END)
    updated = (text[:start] + generated() + text[end:]).encode('utf8')
    if args.check:
        assert before == updated, 'Generated profiles are stale'
    else:
        assert TARGET.read_bytes() == before, 'Concurrent edit'
        TARGET.write_bytes(updated)
    print('ATTACK SHARK family profiles: 37 exact identities/maps PASS')
