"""Check materialized and effective colors, not only conditional-rule definitions."""
import argparse
import json
from pathlib import Path
from check_keyboard_sheet_structure import unpack

WHITE = dict(red=1, green=1, blue=1)
DARK = dict(red=0.14117648, green=0.19607843, blue=0.2784314)
PALETTES = {
    'green': ((.65882355, .8666667, .70980394), (.09019608, .29803923, .16470589)),
    'yellow': ((1, .9019608, .6392157), (.3882353, .2784314, 0)),
    'gray': ((.92156863, .93333334, .9490196), (.27450982, .30980393, .35686275)),
    'red': ((.95686275, .7176471, .7176471), (.47843137, .1254902, .1254902)),
    'lime': ((.8666667, .9529412, .6509804), (.21960784, .34509805, .08235294)),
}


def palette(status):
    if status in ('Supported; awaiting tester', 'Supported; Block Bound Keys retest pending'):
        key = 'lime'
    elif status.startswith('Supported'): key = 'green'
    elif status.startswith(('Implemented;', 'Known protocol;')): key = 'yellow'
    elif status in ('Not investigated', 'Research incomplete', 'Research frozen; tester needed'): key = 'gray'
    elif status.startswith(('Support impossible', 'No usable analog found', 'Research blocked:')): key = 'red'
    else: raise ValueError('Unknown support status: '+status)
    return [dict(zip(('red', 'green', 'blue'), c)) for c in PALETTES[key]]


def color(fmt, foreground=False):
    if foreground: fmt = fmt.get('textFormat', {})
    name = 'foregroundColor' if foreground else 'backgroundColor'
    return fmt.get(name+'Style', {}).get('rgbColor', fmt.get(name, {}))


def same(a, b):
    return all(abs(a.get(k, 0)-b.get(k, 0)) < 0.00001 for k in ('red', 'green', 'blue'))


def audit_presentation(source, effective=True):
    sheet = next(s for s in unpack(source)['sheets'] if s['properties']['title'] == 'Main')
    rows = sheet['data'][0]['rowData']; requests, issues = [], []
    previous = ''
    for i, row in enumerate(rows):
        if not i: continue
        cells = row.get('values', [])
        vals = [c.get('formattedValue', '') for c in cells]
        if not any(vals): previous = ''; continue
        if len(vals) < 3 or not all(vals[:3]): raise ValueError('Incomplete model row')
        bg, fg = palette(vals[2])
        expected = [(WHITE, WHITE if vals[0] == previous else DARK), (bg, fg), (bg, fg)]
        previous = vals[0]
        for j, (background, foreground) in enumerate(expected):
            bad_base = False
            for field in ('userEnteredFormat', 'effectiveFormat') if effective else ('userEnteredFormat',):
                fmt = cells[j].get(field, {})
                if not same(color(fmt), background) or not same(color(fmt, True), foreground):
                    issues.append(f'{field}:{i+1}:{j+1}')
                    bad_base |= field == 'userEnteredFormat'
            if bad_base:
                requests.append({'repeatCell': {'range': dict(sheetId=sheet['properties']['sheetId'],
                    startRowIndex=i, endRowIndex=i+1, startColumnIndex=j, endColumnIndex=j+1),
                    'cell': {'userEnteredFormat': {'backgroundColor': background,
                        'backgroundColorStyle': {'rgbColor': background}, 'textFormat': {
                            'foregroundColor': foreground, 'foregroundColorStyle': {'rgbColor': foreground}}}},
                    'fields': 'userEnteredFormat.backgroundColor,userEnteredFormat.backgroundColorStyle,'
                              'userEnteredFormat.textFormat.foregroundColor,userEnteredFormat.textFormat.foregroundColorStyle'}})
    return requests, issues


if __name__ == '__main__':
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('snapshot', type=Path); p.add_argument('--plan', type=Path)
    a = p.parse_args(); requests, issues = audit_presentation(json.loads(a.snapshot.read_bytes()))
    if a.plan:
        with a.plan.open('x', encoding='utf8') as f: json.dump(requests, f)
    print(f'SHEET_PRESENTATION={"FAIL" if issues else "PASS"} issues={len(issues)} repairs={len(requests)}')
    raise SystemExit(bool(issues) and not a.plan)
