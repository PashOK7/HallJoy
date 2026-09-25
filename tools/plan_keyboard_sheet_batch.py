"""Offline native Sheet insertion plan; never writes to Google or existing files.
Requires a fresh native snapshot, reviewed intents and explicit optional renames.
The caller must back up/re-read live data and verify after applying requests.
"""
import argparse
import copy
import hashlib
import json
import re
from pathlib import Path
from check_keyboard_sheet_structure import audit, unpack
from check_keyboard_sheet_presentation import audit_presentation


def natural(text):
    return tuple((1, int(x)) if x.isdigit() else (0, x.casefold())
                 for x in re.split(r'(\d+)', text))


def plan(source, intents, renames=()):
    # Packed snapshots intern formats; detach each cell before mutating borders.
    native = json.loads(json.dumps(unpack(copy.deepcopy(source))))
    initial = copy.deepcopy(native)
    _, issues, _ = audit(native)
    if issues:
        raise ValueError('Repair existing structure before insertion: '+str(issues[:5]))
    sheet = next(s for s in native['sheets'] if s['properties']['title'] == 'Main')
    sid = sheet['properties']['sheetId']
    grid = sheet['data'][0]
    count = sheet['properties']['gridProperties']['rowCount']
    rows, dims = grid['rowData'], grid['rowMetadata']
    rows.extend({'values': [{}, {}, {}]} for _ in range(count-len(rows)))
    old_indexes = list(range(count))
    requests, changes = [], []
    def value(row, col):
        cells = row.get('values', [])
        return cells[col].get('formattedValue', '') if len(cells) > col else ''
    def pairs():
        return {(value(r, 0), value(r, 1)): i for i, r in enumerate(rows)
                if i and value(r, 1)}
    model_rows = [r for i, r in enumerate(rows) if i and value(r, 1)]
    if len(pairs()) != len(model_rows):
        raise ValueError('Duplicate brand/model pairs')
    exemplar = model_rows[0]
    blank = next(r for i, r in enumerate(rows) if i and not value(r, 0))
    def cells_from(row):
        cells = []
        for c in row.get('values', []):
            cells.append({k: copy.deepcopy(c[k]) for k in
                          ('userEnteredFormat', 'dataValidation') if k in c})
        return (cells + [{}, {}, {}])[:3]
    def rectangle(index, column=0, width=3):
        return dict(sheetId=sid, startRowIndex=index, endRowIndex=index+1,
                    startColumnIndex=column, endColumnIndex=column+width)
    def set_value(index, column, text):
        c = rows[index]['values'][column]
        c.update(userEnteredValue={'stringValue': text}, formattedValue=text,
                 effectiveValue={'stringValue': text})
        requests.append({'updateCells': {'range': rectangle(index, column, 1),
            'rows': [{'values': [{'userEnteredValue': {'stringValue': text}}]}],
            'fields': 'userEnteredValue'}})
    seen = set()
    for rename in renames:
        old = (rename['brand'], rename['from'])
        new = (rename['brand'], rename['to'])
        if old not in pairs() or new in pairs():
            raise ValueError('Rename is missing or collides: '+str(rename))
        index = pairs()[old]
        set_value(index, 1, new[1])
        changes.append(dict(kind='rename', old=old, new=new))
    for intent in sorted(intents, key=lambda x: (x['brand'].casefold(), natural(x['model']))):
        key = (intent['brand'], intent['model'])
        if key in seen:
            raise ValueError('Duplicate intent: '+str(key))
        seen.add(key)
        if key in pairs():
            index = pairs()[key]
            allowed = rows[index]['values'][2].get('dataValidation', {}).get('condition', {}).get('values', [])
            if intent['status'] not in [x['userEnteredValue'] for x in allowed]:
                raise ValueError('Status is not allowed: '+str(key))
            previous = value(rows[index], 2)
            if previous != intent['status']:
                set_value(index, 2, intent['status'])
                changes.append(dict(kind='status', brand=key[0], model=key[1], previous=previous))
            continue
        peers = [i for i, r in enumerate(rows) if value(r, 0) == key[0]]
        if peers:
            index = next((i for i in peers if natural(value(rows[i], 1)) > natural(key[1])), peers[-1]+1)
        else:
            index = next((i for i, r in enumerate(rows) if i and value(r, 0)
                          and value(r, 0).casefold() > key[0].casefold()),
                         max(i for i, r in enumerate(rows) if value(r, 0))+2)
        cells = cells_from(exemplar)
        allowed = cells[2].get('dataValidation', {}).get('condition', {}).get('values', [])
        if intent['status'] not in [x['userEnteredValue'] for x in allowed]:
            raise ValueError('New status is not allowed')
        for c, text in zip(cells, (*key, intent['status'])):
            c['userEnteredValue'] = {'stringValue': text}
        new_rows = [{'values': cells}]
        if not peers:
            new_rows.append({'values': cells_from(blank)})
        length = len(new_rows)
        requests.append({'insertDimension': {'range': dict(sheetId=sid, dimension='ROWS',
                         startIndex=index, endIndex=index+length), 'inheritFromBefore': False}})
        requests.append({'updateCells': {'start': dict(sheetId=sid, rowIndex=index, columnIndex=0),
                         'rows': copy.deepcopy(new_rows),
                         'fields': 'userEnteredValue,userEnteredFormat,dataValidation'}})
        for offset in range(length):
            height = 32 if offset == 0 else 16
            requests.append({'updateDimensionProperties': {'range': dict(sheetId=sid, dimension='ROWS',
                startIndex=index+offset, endIndex=index+offset+1),
                'properties': {'pixelSize': height}, 'fields': 'pixelSize'}})
        for r in new_rows:
            for c in r['values']:
                if 'userEnteredValue' in c:
                    c['formattedValue'] = c['userEnteredValue']['stringValue']
                    c['effectiveValue'] = copy.deepcopy(c['userEnteredValue'])
        rows[index:index] = new_rows
        dims[index:index] = [{'pixelSize': 32}] + ([{'pixelSize': 16}] if length == 2 else [])
        old_indexes[index:index] = [None]*length
        sheet['properties']['gridProperties']['rowCount'] += length
        changes.append(dict(kind='insert', brand=key[0], model=key[1], new_brand=not bool(peers)))
    repairs, _, blocks = audit(native)
    requests.extend(repairs)
    # Model/blank row heights and validations were explicit; only outlines need repair.
    if any('repeatCell' not in r for r in repairs):
        raise ValueError('Unexpected non-border repair after planning')
    for request in repairs:
        r = request['repeatCell']; area = r['range']
        assert r['fields'] == 'userEnteredFormat.borders'
        c = rows[area['startRowIndex']]['values'][area['startColumnIndex']]
        c.setdefault('userEnteredFormat', {})['borders'] = copy.deepcopy(r['cell'].get('userEnteredFormat', {}).get('borders', {}))
    assert not audit(native)[1]
    # Recompute base colors after insertions too: a new first model must reveal
    # its brand and hide the former first row even if client CF rendering lags.
    requests.extend(audit_presentation(native, effective=False)[0])
    return dict(schema=1, before_sha256=hashlib.sha256(json.dumps(initial, sort_keys=True).encode()).hexdigest(),
                requests=requests, changes=changes, original_row_indexes=old_indexes,
                expected_models=[dict(brand=value(r, 0), model=value(r, 1), status=value(r, 2))
                                 for i, r in enumerate(rows) if i and value(r, 1)],
                row_count=len(rows), brand_blocks=blocks)


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--snapshot', type=Path, required=True)
    p.add_argument('--package', type=Path, required=True)
    p.add_argument('--renames', type=Path)
    p.add_argument('--out', type=Path, required=True)
    a = p.parse_args()
    result = plan(json.loads(a.snapshot.read_bytes()), json.loads(a.package.read_bytes())['sheet_intent'],
                  json.loads(a.renames.read_bytes()) if a.renames else [])
    with a.out.open('x', encoding='utf8') as f:
        json.dump(result, f, ensure_ascii=False, indent=2)
    print(json.dumps(dict(requests=len(result['requests']), models=len(result['expected_models']),
                          brand_blocks=result['brand_blocks'], changes=len(result['changes']))))


if __name__ == '__main__':
    main()
