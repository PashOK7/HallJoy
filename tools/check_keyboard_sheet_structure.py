"""Audit native Google Sheets structure; emit narrowly scoped repair requests.
Input: updatedSpreadsheet from a full A1:C<rowCount> native read (grid data on).
No network writes. --plan writes a reviewed batchUpdate request list exclusively.
"""
import argparse
import json
from pathlib import Path


def unpack(source):
    if 'dict' in source:
        sheet = source['sheet']
        sheet['data'] = [{**source['grid'], 'rowData': [
            {'values': [{k: source['dict'][v] for k, v in c.items()} for c in r]}
            for r in source['rows']]}]
        return {'sheets': [sheet]}
    return source.get('updatedSpreadsheet', source)


def audit(source):
    sheet = next(s for s in unpack(source)['sheets'] if s['properties']['title'] == 'Main')
    assert not sheet.get('merges'), 'Unexpected merged cells: inspect before editing'
    assert len(sheet['data']) == 1
    data = sheet['data'][0]
    assert data.get('startRow', 0) == data.get('startColumn', 0) == 0
    rows, dims = data['rowData'], data['rowMetadata']
    count = sheet['properties']['gridProperties']['rowCount']
    assert len(dims) == count, 'Full bounded grid read required'
    rows = rows + [{'values': [{}, {}, {}]} for _ in range(count-len(rows))]
    # Sheets omits trailing empty cells, including entire separator rows.
    rows = [{**r, 'values': r.get('values', []) + [{} for _ in range(max(0, 3-len(r.get('values', []))))]} for r in rows]
    sid = sheet['properties']['sheetId']
    def value(i, j):
        return rows[i].get('values', [])[j].get('formattedValue', '') if 0 <= i < count else ''
    def rect(i, j=0, width=3):
        return dict(sheetId=sid, startRowIndex=i, endRowIndex=i+1,
                    startColumnIndex=j, endColumnIndex=j+width)
    color = dict(red=0.48235294, green=0.5372549, blue=0.60784316)
    border = dict(style='SOLID_MEDIUM', colorStyle=dict(rgbColor=color))
    def norm(b):
        return {k: (v.get('style'), v.get('colorStyle', {}).get('rgbColor', v.get('color', {})))
                for k, v in b.items() if v.get('style') not in (None, 'NONE')}
    requests, issues, blocks = [], [], []
    last_used = max(i for i in range(count) if any(value(i,j) for j in range(3)))
    for i in range(1, count):
        vals = [value(i, j) for j in range(3)]
        cells = rows[i].get('values', [])
        if any(vals):
            assert all(vals), ('Incomplete model row', i+1, vals)
            start = value(i-1, 0) != vals[0]
            end = value(i+1, 0) != vals[0]
            if start:
                assert not value(i-1, 1) or i == 1, ('Missing brand separator', i+1)
                blocks.append(vals[0])
            for j, c in enumerate(cells[:3]):
                expected = {}
                if start: expected['top'] = border
                if end: expected['bottom'] = border
                if j == 0: expected['left'] = border
                if j == 2: expected['right'] = border
                if norm(c.get('userEnteredFormat', {}).get('borders', {})) != norm(expected):
                    issues.append(f'border:{i+1}:{j+1}')
                    requests.append({'repeatCell': {'range': rect(i,j,1),
                        'cell': {'userEnteredFormat': {'borders': expected}},
                        'fields': 'userEnteredFormat.borders'}})
            if dims[i].get('pixelSize', 0) < 32:
                issues.append(f'height:{i+1}')
                requests.append({'updateDimensionProperties': {
                    'range': dict(sheetId=sid, dimension='ROWS', startIndex=i, endIndex=i+1),
                    'properties': {'pixelSize': 32}, 'fields': 'pixelSize'}})
            validation = cells[2].get('dataValidation', {})
            assert validation.get('strict') and vals[2] in [v.get('userEnteredValue') for v in validation.get('condition', {}).get('values', [])], ('Invalid status', i+1)
        else:
            if i < last_used and dims[i].get('pixelSize') != 16:
                issues.append(f'separator-height:{i+1}')
                requests.append({'updateDimensionProperties': {
                    'range': dict(sheetId=sid, dimension='ROWS', startIndex=i, endIndex=i+1),
                    'properties': {'pixelSize': 16}, 'fields': 'pixelSize'}})
            for j, c in enumerate(cells[:3]):
                if norm(c.get('userEnteredFormat', {}).get('borders', {})):
                    issues.append(f'separator-border:{i+1}:{j+1}')
                    requests.append({'repeatCell': {'range': rect(i,j,1), 'cell': {},
                                                    'fields': 'userEnteredFormat.borders'}})
                if c.get('dataValidation'):
                    issues.append(f'empty-dropdown:{i+1}:{j+1}')
                    requests.append({'setDataValidation': {'range': rect(i,j,1)}})
    assert len(blocks) == len(set(blocks)), 'Split brand blocks'
    return requests, issues, len(blocks)


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('snapshot', type=Path)
    parser.add_argument('--plan', type=Path)
    args = parser.parse_args()
    source = json.loads(args.snapshot.read_bytes())
    requests, issues, blocks = audit(source)
    from check_keyboard_sheet_presentation import audit_presentation
    color_requests, color_issues = audit_presentation(source)
    requests.extend(color_requests)
    issues.extend(color_issues)
    if args.plan:
        with args.plan.open('x', encoding='utf8') as f:
            json.dump(requests, f, indent=2)
    print(f'SHEET_STRUCTURE={"FAIL" if issues else "PASS"} blocks={blocks} issues={len(issues)}')
    if issues: print(' '.join(issues[:80]))
    raise SystemExit(bool(issues) and not args.plan)
