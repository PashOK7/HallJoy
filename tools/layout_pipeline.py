"""Offline layout workbench. Short stdout, full evidence in an explicit new stage.

summary/check never write. stage never edits runtime source or user settings.
Existing brand adapters retain their byte-identical outputs and safety rules.
"""
import argparse
import hashlib
import importlib
import json
import re
import urllib.request
from pathlib import Path
import layout_import as common

ROOT = Path(__file__).resolve().parents[1]
CATALOG = ROOT / 'tools/layout_catalog.json'
ADAPTERS = {'keychron': 'build_keychron_layouts', 'lemokey': 'build_lemokey_layouts',
            'drunkdeer': 'build_drunkdeer_layouts'}
MANIFEST_ADAPTERS = {'aula-w669': 'layout_aula_w669', 'aula-max': 'layout_aula_max',
                     'redragon-w669': 'layout_redragon_w669', 'reviewed-report': 'layout_reviewed_report'}


def digest(data):
    return hashlib.sha256(data).hexdigest()


def local_path(root, relative):
    root = Path(root).resolve()
    path = (root / relative).resolve()
    common.require(path != root and path.is_relative_to(root), f'Unsafe path: {relative}')
    return path


def read_catalog(path=CATALOG):
    data = common.read_json(path)[0]
    common.require(data.get('schema') == 1 and isinstance(data.get('brands'), dict), 'Unknown catalog schema')
    for brand, spec in data['brands'].items():
        common.label_text(brand)
        common.require(spec['adapter'] in {*ADAPTERS, *MANIFEST_ADAPTERS}, 'Unknown adapter')
        ids, names, identities = set(), set(), set()
        for model in spec.get('models', []):
            common.require(model.get('adapter',spec['adapter']) in MANIFEST_ADAPTERS,'Unknown model adapter')
            common.require(re.fullmatch('[a-z][a-z0-9_]*', model['id']), 'Unsafe model symbol')
            name = (model['model'], model['variant'])
            common.label_text(model['model'])
            common.require(model['variant'] in ('ANSI', 'ISO', 'JIS'), 'Unknown physical variant')
            common.require(model['id'] not in ids and name not in names, 'Duplicate model/variant')
            ids.add(model['id']); names.add(name)
            common.require(re.fullmatch('[0-9a-f]{64}', model['sha256']), 'Missing source lock')
            local_path(ROOT, model['source'])
            if 'css' in model:
                local_path(ROOT,model['css'])
                common.require(re.fullmatch('[0-9a-f]{64}',model['cssSha256']), 'Missing CSS lock')
            for product in model['products']:
                common.require(re.fullmatch('[A-Z0-9-]+', product) and product not in identities,
                               'Ambiguous or invalid firmware product')
                identities.add(product)
    return data['brands']


def fetch_sources(spec):
    """Explicit network action, never an implicit part of check or summary."""
    models=spec.get('models',[])
    common.require(models, 'Legacy adapter: use the source acquisition guide listed by list')
    sources=[]
    for model in models:
        sources.append(model)
        if 'css' in model:
            sources.append(dict(source=model['css'],url=model['cssUrl'],sha256=model['cssSha256']))
    for model in sources:
        path=local_path(ROOT,model['source'])
        if path.exists():
            common.require(digest(path.read_bytes())==model['sha256'],'Cached source changed; refusing overwrite')
            continue
        common.require(model['url'].startswith('https://'),'HTTPS source required')
        with urllib.request.urlopen(model['url'],timeout=25) as response:
            data=response.read(8*1024*1024+1)
        common.require(len(data)<=8*1024*1024 and digest(data)==model['sha256'],
                       'Source differs from reviewed lock; review before changing the manifest')
        path.parent.mkdir(parents=True,exist_ok=True)
        with path.open('xb') as stream:
            stream.write(data)


def rectangles(key):
    x, y, w, h = (key[n] for n in ('x', 'y', 'w', 'h'))
    nw, ny = key.get('notchW', 0), key.get('notchY', 0)
    if nw:
        return [(x, y, w, ny), (x + nw, y + ny, w - nw, h - ny)]
    return [(x, y, w, h)]


def validate_report(report):
    common.require(report['status'] == 'ready' and not report['unresolved'], 'Unresolved actions')
    common.label_text(report['name'])
    keys = report['keys']
    common.require(0 < len(keys) <= 512, 'Invalid key count')
    seen = set()
    for key in keys:
        hid = common.integer(key['hid'], 1, 65535, 'hid')
        common.require(hid not in seen, 'Duplicate HID')
        seen.add(hid)
        common.label_text(key['label'])
        for field, lo, hi in [('x', 0, 4000), ('y', 0, 4000), ('w', 10, 600), ('h', 10, 600)]:
            common.integer(key[field], lo, hi, field)
        nw = common.integer(key.get('notchW', 0), 0, key['w'] - 1, 'notchW')
        ny = common.integer(key.get('notchY', 0), 0, key['h'] - 1, 'notchY')
        common.require(bool(nw) == bool(ny), 'Incomplete compound contour')
    for i, a in enumerate(keys):
        for b in keys[i + 1:]:
            common.require(not any(max(ax,bx) < min(ax+aw,bx+bw) and max(ay,by) < min(ay+ah,by+bh)
                for ax,ay,aw,ah in rectangles(a) for bx,by,bw,bh in rectangles(b)),
                f'Overlapping contours: {a["hid"]}/{b["hid"]}')


def cpp_string(value):
    common.label_text(value)
    return json.dumps(value, ensure_ascii=True)


def normalized_products(brand, reports):
    """One reusable emitter for future adapters: geometry, registration, identity.

    Identity rows deliberately require a verified protocol/product. No VID-only
    fallback. Consumers must supply session-proven identity, never a HID caption.
    """
    out, arrays, registry, identities = {}, ['#pragma once'], [], []
    tokens, selectors = set(), set()
    for report in reports:
        validate_report(report)
        symbol = report['id']
        common.require(not any(c in report['name'] for c in '<>:"/\\|?*') and
                       report['name'].rstrip(' .')==report['name'], 'Unsafe layout filename')
        token = int(digest(symbol.encode())[:16],16)
        common.require(token and token not in tokens, 'Duplicate layout identity token')
        tokens.add(token)
        common.require(re.fullmatch('[a-z][a-z0-9_]*', symbol), 'Unsafe C++ symbol')
        out[f'exports/{report["name"]}.ini'] = common.ini_text(report).encode('utf-16')
        out[f'exports/{symbol}-review.json'] = (json.dumps(report, indent=2)+'\n').encode()
        arrays.append(f'static const KeyDef g_{symbol}[] = {{')
        for key in report['keys']:
            values = [key['hid'], min(20,key['y']//46), key['x'],key['w'],key['h'],key['y'],
                      key.get('notchW',0),key.get('notchY',0)]
            arrays.append('    {L%s, %s},' % (cpp_string(key['label']), ', '.join(map(str,values))))
        arrays.append('};')
        registry.append('    {L%s, g_%s, (int)std::size(g_%s), L%s},' %
                        (cpp_string(report['name']),symbol,symbol,cpp_string(report.get('brand',brand))))
        for product in report['identity']['products']:
            selector=(report['identity']['protocol'],product)
            common.require(selector not in selectors, 'Ambiguous verified identity')
            selectors.add(selector)
            identities.append('    {%s, %s, 0x%016Xull, L%s},' % (cpp_string(selector[0]),
                              cpp_string(product),token,cpp_string(report['name'])))
    out['generated/layouts.h'] = ('// Generated by layout_pipeline.py; review before integration.\n'+'\n'.join(arrays)+'\n').encode()
    out['generated/presets.inc'] = ('\n'.join(registry)+'\n').encode()
    out['generated/identities.inc'] = ('// Fields: verified protocol, exact firmware product, preset name.\n'+'\n'.join(identities)+'\n').encode()
    out['generated/identities.h'] = ('#pragma once\n#include <cstdint>\n#include <string_view>\n'
        'namespace halljoy::layout_identity {\n'
        'struct Entry { std::string_view protocol, product; std::uint64_t token; const wchar_t* preset; };\n'
        'inline constexpr Entry entries[] = {\n'+'\n'.join(identities)+'\n};\n'
        'inline constexpr std::uint64_t Token(std::string_view protocol, std::string_view product) noexcept {\n'
        '    for (const auto& e : entries) if (e.protocol==protocol && e.product==product) return e.token;\n'
        '    return 0;\n}\n'
        'inline constexpr const wchar_t* Match(std::uint64_t token) noexcept {\n'
        '    for (const auto& e : entries) if (e.token==token) return e.preset;\n'
        '    return nullptr;\n}\n}\n').encode()
    return out


def reports_for(spec):
    return [importlib.import_module(MANIFEST_ADAPTERS[model.get('adapter',spec['adapter'])]).prepare(model)
            for model in spec['models']]


def runtime_products():
    reports=[]
    for spec in read_catalog().values():
        if spec.get('runtime'):
            reports.extend(reports_for(spec))
    result=normalized_products('',reports)
    return {('src/HallJoyProject/HallJoy/generated/layout_pipeline/'+name[len('generated/'):] if name.startswith('generated/')
             else 'docs/exports/layout-pipeline/'+name[len('exports/'):]):data for name,data in result.items()}


def integrate():
    outputs=runtime_products()
    previous={name:local_path(ROOT,name).read_bytes() if local_path(ROOT,name).exists() else None for name in outputs}
    changed=[name for name in outputs if previous[name]!=outputs[name]]
    if not changed: return 0
    import tempfile
    backup=Path(tempfile.mkdtemp(prefix='layout-integrate-',dir=ROOT/'.local/backups'))
    for name in changed:
        if previous[name] is not None:
            target=local_path(backup,name);target.parent.mkdir(parents=True,exist_ok=True)
            with target.open('xb') as stream: stream.write(previous[name])
    for name in changed:
        target=local_path(ROOT,name)
        common.require((target.read_bytes() if target.exists() else None)==previous[name], 'Concurrent change: '+name)
        target.parent.mkdir(parents=True,exist_ok=True)
        with target.open('wb' if previous[name] is not None else 'xb') as stream: stream.write(outputs[name])
    print(f'GENERATED updated={len(changed)} backup={backup.relative_to(ROOT)}')
    return len(changed)


def products(brand, spec):
    if spec['adapter'] in MANIFEST_ADAPTERS:
        reports = reports_for(spec)
        return (runtime_products() if spec.get('runtime') else normalized_products(brand,reports)), reports, bool(spec.get('runtime'))
    module = importlib.import_module(ADAPTERS[spec['adapter']])
    output = {path.relative_to(ROOT).as_posix(): data for path,data in module.products().items()}
    reports = [json.loads(data) for path,data in output.items() if path.endswith('-review.json')]
    for report in reports:
        validate_report(report)
    return output, reports, True


def check_stage(directory):
    directory = Path(directory)
    manifest = common.read_json(directory/'stage.json')[0]
    common.require(manifest.get('schema') == 1, 'Unknown stage schema')
    actual = {p.relative_to(directory).as_posix() for p in directory.rglob('*') if p.is_file()}
    common.require(actual == set(manifest['files']) | {'stage.json'}, 'Stage file set changed')
    for name, expected in manifest['files'].items():
        common.require(digest(local_path(directory,name).read_bytes()) == expected, f'Stage changed: {name}')
    return manifest


def stage(directory, brand, outputs, reports, spec):
    directory = Path(directory)
    # Validate every target before creating anything. Never overwrite a prior run.
    for name in outputs:
        local_path(directory, name)
    directory.mkdir(parents=True, exist_ok=False)
    manifest = dict(schema=1, brand=brand, files={}, models=[r['name'] for r in reports],
                    guide=spec['guide'], pending=spec.get('pending', []),
                    runtimeInstalled=False, identityIntegrationRequired=not spec.get('runtime',False))
    for name, data in outputs.items():
        target = local_path(directory, name)
        target.parent.mkdir(parents=True, exist_ok=True)
        with target.open('xb') as stream:
            stream.write(data)
        manifest['files'][name] = digest(data)
    # Completion marker is last; a partial failed stage cannot pass check-stage.
    with (directory/'stage.json').open('x',encoding='utf-8') as stream:
        json.dump(manifest,stream,indent=2); stream.write('\n')
    check_stage(directory)


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('command', choices=('list','summary','check','fetch','stage','check-stage','integrate'))
    parser.add_argument('brand', nargs='?')
    parser.add_argument('--output-dir',type=Path)
    args=parser.parse_args()
    try:
        if args.command=='check-stage':
            common.require(args.output_dir is not None, '--output-dir required')
            result=check_stage(args.output_dir)
            print(f'STAGE=PASS brand={result["brand"]} files={len(result["files"])}'); return 0
        catalog=read_catalog()
        if args.command=='integrate':
            integrate(); return 0
        if args.command=='list':
            for brand,spec in catalog.items():
                print(f'{brand}: adapter={spec["adapter"]} guide={spec["guide"]}')
            return 0
        common.require(args.brand in catalog, 'Choose a brand with list')
        spec=catalog[args.brand]
        if args.command=='fetch':
            fetch_sources(spec)
            print(f'SOURCES=PASS brand={args.brand} cached={len(spec["models"])}'); return 0
        outputs,reports,integrated=products(args.brand,spec)
        changed=[name for name,data in outputs.items() if not local_path(ROOT,name).is_file() or local_path(ROOT,name).read_bytes()!=data] if integrated else []
        if args.command=='check':
            common.require(not changed, 'Generated outputs differ: '+', '.join(changed))
        if args.command=='stage':
            common.require(args.output_dir is not None, '--output-dir required')
            stage(args.output_dir,args.brand,outputs,reports,spec)
        print(f'LAYOUT_PIPELINE=PASS brand={args.brand} models={len(reports)} keys={sum(len(r["keys"]) for r in reports)} files={len(outputs)} changed={len(changed)} integrated={str(integrated).lower()}')
        if args.command=='summary':
            for report in reports:
                print(f'  {report["name"]}: {len(report["keys"])} keys')
        for pending in spec.get('pending',[]):
            print(f'  PENDING {pending["model"]}: {pending["reason"]}')
        if not integrated:
            print('  Runtime untouched. Stage includes registration and identity rows; verified-session wiring/review required before shipping.')
        return 0
    except (ValueError,OSError,KeyError,TypeError,ArithmeticError) as error:
        print(f'LAYOUT_PIPELINE=FAIL {error}'); return 2


if __name__=='__main__':
    raise SystemExit(main())
