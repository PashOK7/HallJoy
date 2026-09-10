"""Reject invalid UTF-8 and common double-decoded UI text; pin MSVC decoding."""
from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[3]
HALL = ROOT / 'src/HallJoyProject/HallJoy'
project = (HALL / 'HallJoy.vcxproj').read_text(encoding='utf-8-sig')
assert '/utf-8 /we4828' in project, 'MSVC must decode UTF-8 and reject invalid source bytes'
assert 'charset = utf-8' in (ROOT / '.editorconfig').read_text(encoding='utf-8')
# UTF-8 punctuation decoded as CP1251 / CP1252, replacement glyphs, and
# repeated CP1251-decoded Cyrillic. Escape patterns so this audit audits itself.
broken = re.compile(r'\ufffd|\u00e2[\u20ac\u0080]|\u0432[\u0402\u20ac]|(?:[\u0420\u0421][\u0400-\u04ff\u0080-\u00bf]){3,}')
count = 0
failures = []
for base in (ROOT / 'src', ROOT / 'tools'):
    for path in base.rglob('*'):
        if path.suffix.lower() not in {'.cpp', '.h', '.hpp', '.py', '.rc', '.vcxproj', '.props'}:
            continue
        if 'third_party' in path.parts:
            continue
        try:
            # Visual Studio's resource editor writes BOM-marked UTF-16 LE.
            encoding = 'utf-16' if path.suffix == '.rc' and path.read_bytes().startswith(b'\xff\xfe') else 'utf-8-sig'
            text = path.read_text(encoding=encoding, errors='strict')
        except UnicodeDecodeError as error:
            failures.append(f'Invalid UTF-8: {path.relative_to(ROOT)} byte {error.start}')
            continue
        for line_no, line in enumerate(text.splitlines(), 1):
            if broken.search(line):
                failures.append(f'Mojibake: {path.relative_to(ROOT)}:{line_no}')
        count += 1
assert not failures, '\n' + '\n'.join(failures)
print(f'SOURCE_ENCODING_STATIC_AUDIT=PASS files={count}')
