"""Generate the reviewed 2026-09-09 compatibility snapshot (stdlib only).

Refuses to overwrite an existing workbook. This is a reviewed snapshot, not
automatic discovery of physical compatibility from USB identifiers.
"""
from pathlib import Path
from zipfile import ZipFile, ZIP_DEFLATED
from xml.etree import ElementTree as ET
from xml.sax.saxutils import escape, quoteattr

ROOT = Path(__file__).resolve().parents[2]
OUT = Path(__file__).with_name('HallJoy_supported_keyboards.xlsx')
H = 'SUPPORTED_HARDWARE.md'
U = 'third_party/UniversalAnalogPluginFixed/overlay/Soup/soup/AnalogueKeyboard.cpp'
rows = []

def add(brand, model, status, route, identity, notes, source=H):
    rows.append([brand, model, status, route, identity, notes, source])

tested = 'Проверено на устройстве'
compat = 'Совместимый протокол'
uap = 'Поддержка через UAP'
add('AULA', 'WIN 60 HE MAX', tested, 'Native Aula', '1CA2:1902', 'Проверена прошивка App V1.1.6 / Feb 4 2026; не путать со Standard.')
add('AULA', 'WIN 60 HE Standard / W669', tested, 'Native W669', '2E3C:C365', 'Семейство SI2825; требуется точное совпадение профиля прошивки.')
add('AULA', 'WIN 68 HE Standard', compat, 'Native W669', 'Проверяется профиль прошивки', 'SI2828HEARGB / SI2828KZHEARGB; 68 клавиш; отдельная физическая проверка не зафиксирована.')
add('Не уточнён', 'KP-TE153 UK', compat, 'Native W669', 'Проверяется профиль прошивки', 'SI2851UKKZHEARGB; 69 клавиш; официальный профиль и автотесты.')
add('Redragon', 'K673RGB-M BR', tested, 'Native W669', '2E3C:C365', '7272BRHEXYXK673JCARGB V3.18.01; 81 клавиша.')
add('Redragon', 'K673RGB-M UK', compat, 'Native W669', 'Проверяется профиль прошивки', '7272UKHEXYXBJCARGB; 81 клавиша; официальный профиль и автотесты.')
add('Redragon', 'K673WB-RGB-M US', compat, 'Native W669', 'Проверяется профиль прошивки', '7272USHEXYXK673JCARGB; 80 клавиш; официальный профиль и автотесты.')
add('IROK', 'MG75 Max', tested, 'Native SparkLink/XD', '1CA6:0529', 'Проверены аналоговый ввод и переподключение.')
add('IROK', 'MG75 Pro', compat, 'Native SparkLink/XD', 'Проверка протокола', 'При успешной проверке протокола; не путать с неподдерживаемой MG75 v2.')
add('MADLIONS', 'MAD 68 Pro R', tested, 'Native asynchronous A0', '373B:1109; bcdDevice 0102', 'Проверен поток A0 и карта 68 позиций.')
add('SayoDevice', 'O3C', tested, 'Native depth 0x22', '8089:0009', 'Трёхклавишное устройство; используются текущие HID-назначения клавиш.')
add('Keychron', 'K4 HE ANSI', tested, 'UAP / A9 31', '3434:0E40', 'ТОЛЬКО специальная full-report прошивка. Стоковая не поддерживается для игр из-за задержек; прошивки других моделей не подходят.')
add('ATK', 'Hex80', 'Поддерживается', 'Native 0x96', '373B:1176 / 1177 / 1250', 'Известные PID; подключение только после проверки протокола. Другие PID не означают ту же модель.')
add('Не уточнён', 'QBZ75-совместимые устройства', compat, 'Native Addressed Analog', 'Проверка протокола 09/94/02', 'Рабочая поддержка включена. Требуется подходящая карта клавиш; неизвестная неполная карта отклоняется.')
for model, pid in [('Huntsman V2 Analog','0266'), ('Huntsman Mini Analog','0282'), ('Huntsman V3 Pro','02A6'), ('Huntsman V3 Pro Mini','02B0'), ('Huntsman V3 Pro Tenkeyless','02A7')]:
    add('Razer', model, uap, 'Embedded UAP', '1532:'+pid, 'Razer Synapse должен быть установлен и запущен; требуется аналоговый HID-отчёт.', U)
polling = 'Стоковая прошивка использует опрос: возможны задержки и пропуски. Full-report прошивка улучшает ввод; нужна версия именно для своей модели.'
for model, ids in [('Q1 HE (ANSI / ISO / JIS)','0B10 / 0B11 / 0B12'), ('Q3 HE ANSI','0B30'), ('Q5 HE ANSI','0B50'), ('K2 HE (ANSI / ISO / JIS)','0E20 / 0E21 / 0E22')]:
    add('Keychron', model, uap, 'Embedded UAP', '3434: '+ids, polling, U)
add('Lemokey', 'P1 HE (ANSI / ISO)', uap, 'Embedded UAP', '362D:0610 / 0611', polling, U)
for model, pid in [('A75','2383'), ('G60','2384'), ('G65','2382'), ('G75 (ANSI / JP)','2386 / 2391')]:
    add('DrunkDeer', model, uap, 'Embedded UAP', '352D: '+pid, 'Поддержка объявлена встроенным UAP; это не отдельное подтверждение физического теста HallJoy.', U)
for model in ['Air60 HE', 'Air75 HE']:
    add('NuPhy', model, uap, 'Embedded UAP', 'VID 19F5; интерфейс 0001:0000', 'Модель указана в документации HallJoy. Семейный reader не гарантирует совместимость всех NuPhy или будущих прошивок.')
for model, ids in [('MAD60HE','1053 / 1054 / 1055 / 1056 / 105D'), ('MAD68HE','1058 / 1059 / 105A / 105C'), ('MAD68R','10A7')]:
    add('MADLIONS', model, uap, 'Embedded UAP', '373B: '+ids, 'Опрос аналоговых значений; возможны задержки и пропуски. Не путать MAD68R с отдельным Native MAD 68 Pro R.', U)
add('Wooting', 'One — старые прошивки', uap, 'Embedded UAP / Wooting v1', '03EB:FF01', 'Аналоговый интерфейс FF54.', U)
add('Wooting', 'Two — старые прошивки', uap, 'Embedded UAP / Wooting v1', '03EB:FF02', 'Аналоговый интерфейс FF54.', U)
add('Wooting', 'Аналоговые модели — семейный маршрут', uap, 'Embedded UAP / Wooting', 'VID 31E3; FF54 или FF53', 'Код принимает семейство, а не фиксированный список моделей. Не является подтверждением каждого поколения.', U)
rows.sort(key=lambda r: (r[0].casefold(), r[1].casefold()))
headers = ['Бренд', 'Модель / вариант', 'Статус', 'Реализация', 'USB VID:PID / идентификация', 'Условия и ограничения', 'Источник в проекте']
pending = [
 ['ASUS ROG', 'Azoth 96 HE', 'Заморожено, отключено', 'Диагностическая заготовка', '0B05:1C10', 'Ждём владельца для физического тестирования. В обычной сборке не поддерживается.', H],
 ['AULA', 'HERO84 HE', 'Заморожено, отключено', 'Экспериментальная заготовка', '372E:103E', 'Ждём владельца для физического тестирования. В обычной сборке не поддерживается.', H],
 ['IROK', 'ND75', 'Экспериментально, отключено', 'Native M484 candidate', '0416:7372', 'Только отдельная тестовая сборка; физическая проверка не завершена.', H],
 ['IROK', 'MG75 v2', 'Не поддерживается', 'Другая MCU / протокол', 'Не указан', 'Проверена физически; не путать с MG75 Max / Pro.', H],
 ['Attack Shark', 'X68 HE', 'Исследование, не поддерживается', 'Реализации нет', '3151:502D', 'Ждём владельца. Прошивка X68 HE не получена; изучена референсная X65 HE, это не поддержка X68 HE.', 'docs/research/ATTACK_SHARK_X68_HE_RECON_2026-09-09.md'],
]
notes = [
 ['Дата снимка', '2026-09-09. Список основан на локальном коде и документации HallJoy, а не на каталоге брендов.'],
 ['Проверено на устройстве', 'Есть записанное подтверждение работы на физическом устройстве; условия прошивки и варианта обязательны.'],
 ['Поддержка через UAP', 'Реализация входит в HallJoy. Устанавливать системный UAP или Wooting Analog SDK не требуется. Не означает физический тест каждой модели автором HallJoy.'],
 ['Совместимый протокол', 'Маршрут включён; устройство допускается после проверки протокола и карты. Это не замороженная поддержка.'],
 ['Поддерживается', 'Названная модель поддерживается по документации; отдельное утверждение о физической проверке здесь не добавлено.'],
 ['Семейные маршруты', 'Дополнительно возможны совместимые Aula MAX, Addressed Analog, SparkLink, SayoDevice, DrunkDeer и NuPhy. Нельзя обещать все модели бренда.'],
 ['Что не является поддержкой', 'Наличие раскладки, общий VID/PID, сходное название и исследованная прошивка другой клавиатуры не доказывают совместимость.'],
 ['Подключение', 'USB-идентификаторы описывают известные маршруты; таблица не обещает работу аналога по Bluetooth или каждому беспроводному приёмнику.'],
 ['Безопасность прошивок', 'Не прошивать устройство образом другой модели. Для K4 HE обязательна специальная прошивка; стоковая не является игровой поддержкой.'],
 ['Discord', 'https://discord.gg/5FQ297yZh'],
 ['Исходники', H+'; '+U+'; third_party/UniversalAnalogPluginFixed/README.md'],
]

NS = 'http://schemas.openxmlformats.org/spreadsheetml/2006/main'
REL = 'http://schemas.openxmlformats.org/officeDocument/2006/relationships'
PKG = 'http://schemas.openxmlformats.org/package/2006/relationships'
def sheet(data, widths):
    result = [f'<worksheet xmlns="{NS}"><sheetViews><sheetView workbookViewId="0"><pane ySplit="1" topLeftCell="A2" activePane="bottomLeft" state="frozen"/></sheetView></sheetViews><cols>']
    result += [f'<col min="{i}" max="{i}" width="{w}" customWidth="1"/>' for i,w in enumerate(widths,1)]
    result.append('</cols><sheetData>')
    for n,row in enumerate(data,1):
        result.append(f'<row r="{n}" ht="{32 if n == 1 else 76}" customHeight="1">')
        for c,value in enumerate(row):
            result.append(f'<c r="{chr(65+c)}{n}" s="{1 if n == 1 else 2+n%2}" t="inlineStr"><is><t>{escape(value)}</t></is></c>')
        result.append('</row>')
    result.append(f'</sheetData><autoFilter ref="A1:{chr(64+len(widths))}{len(data)}"/><pageMargins left="0.3" right="0.3" top="0.5" bottom="0.5" header="0.2" footer="0.2"/></worksheet>')
    return ''.join(result)

def main():
    assert not OUT.exists(), f'Review existing file before replacing: {OUT}'
    assert len({tuple(r[:2]) for r in rows}) == len(rows)
    for r in rows + pending:
        assert (ROOT / r[6]).is_file(), r[6]
    sheets = [('Поддерживаемые', [headers]+rows, [17,33,29,27,36,85,55]), ('Не поддерживаются', [headers]+pending, [17,33,29,27,36,85,55]), ('Как читать', [['Раздел','Описание']]+notes, [30,120])]
    files = {
        '_rels/.rels': f'<Relationships xmlns="{PKG}"><Relationship Id="rId1" Type="{REL}/officeDocument" Target="xl/workbook.xml"/></Relationships>',
        'xl/workbook.xml': f'<workbook xmlns="{NS}" xmlns:r="{REL}"><sheets>'+''.join(f'<sheet name={quoteattr(name)} sheetId="{i}" r:id="rId{i}"/>' for i,(name,_,_) in enumerate(sheets,1))+'</sheets></workbook>',
        'xl/_rels/workbook.xml.rels': f'<Relationships xmlns="{PKG}">'+''.join(f'<Relationship Id="rId{i}" Type="{REL}/worksheet" Target="worksheets/sheet{i}.xml"/>' for i in range(1,4))+f'<Relationship Id="rId4" Type="{REL}/styles" Target="styles.xml"/></Relationships>',
        '[Content_Types].xml': '<Types xmlns="http://schemas.openxmlformats.org/package/2006/content-types"><Default Extension="rels" ContentType="application/vnd.openxmlformats-package.relationships+xml"/><Default Extension="xml" ContentType="application/xml"/><Override PartName="/xl/workbook.xml" ContentType="application/vnd.openxmlformats-officedocument.spreadsheetml.sheet.main+xml"/><Override PartName="/xl/styles.xml" ContentType="application/vnd.openxmlformats-officedocument.spreadsheetml.styles+xml"/>'+''.join(f'<Override PartName="/xl/worksheets/sheet{i}.xml" ContentType="application/vnd.openxmlformats-officedocument.spreadsheetml.worksheet+xml"/>' for i in range(1,4))+'</Types>',
        'xl/styles.xml': f'<styleSheet xmlns="{NS}"><fonts count="2"><font><sz val="11"/><name val="Calibri"/><color rgb="FF243447"/></font><font><b/><sz val="11"/><name val="Calibri"/><color rgb="FFFFFFFF"/></font></fonts><fills count="4"><fill><patternFill patternType="none"/></fill><fill><patternFill patternType="gray125"/></fill><fill><patternFill patternType="solid"><fgColor rgb="FF243B53"/><bgColor indexed="64"/></patternFill></fill><fill><patternFill patternType="solid"><fgColor rgb="FFEDF3F8"/><bgColor indexed="64"/></patternFill></fill></fills><borders count="1"><border/></borders><cellStyleXfs count="1"><xf numFmtId="0" fontId="0" fillId="0" borderId="0"/></cellStyleXfs><cellXfs count="4"><xf numFmtId="0" fontId="0" fillId="0" borderId="0" xfId="0"/>'+''.join(f'<xf numFmtId="0" fontId="{font}" fillId="{fill}" borderId="0" xfId="0" applyFont="1" applyFill="1" applyAlignment="1"><alignment vertical="center" wrapText="1"/></xf>' for font,fill in [(1,2),(0,0),(0,3)])+'</cellXfs><cellStyles count="1"><cellStyle name="Normal" xfId="0" builtinId="0"/></cellStyles></styleSheet>',
    }
    for i,(_,data,widths) in enumerate(sheets,1):
        files[f'xl/worksheets/sheet{i}.xml'] = sheet(data,widths)
    for value in files.values():
        ET.fromstring(value)
    with ZipFile(OUT, 'x', ZIP_DEFLATED) as archive:
        for path,value in files.items():
            archive.writestr(path, '<?xml version="1.0" encoding="UTF-8" standalone="yes"?>'+value)
    with ZipFile(OUT) as archive:
        assert archive.testzip() is None
        for path in archive.namelist():
            ET.fromstring(archive.read(path))
    print(f'{OUT}: {len(rows)} supported model/variant/family rows, {len(pending)} excluded rows; ZIP/XML validation PASS')

if __name__ == '__main__':
    main()
