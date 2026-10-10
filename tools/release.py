"""One-command HallJoy release: workspace -> publication clone -> GitHub release.

Usage (from the workspace root, HallJoy-main):
  python tools/release.py 1.6.10 "v1.6.10: short title"            # prepare and verify, no push
  python tools/release.py 1.6.10 "v1.6.10: short title" --publish  # also push, tag and release

Before running: write docs/releases/RELEASE_NOTES_v<version>.md (public text, the GitHub
release body). Each check runs once:
  1. version.h, release index and an empty RELEASE_NOTES_NEXT.md;
  2. fresh clone of main; stop if main changed files the workspace does not have;
  3. transfer under docs/legal/PUBLICATION_POLICY.json (denied paths, local folders and
     local-only files stay out), keeping main's line-ending style for modified files;
  4. private-data scan of added lines and check_publication_inputs.py in the clone;
  5. native suite once (test cache is valid), build_release.ps1 once, package_release.ps1;
  6. --publish: commit, tag, push, GitHub release with four assets, server-side SHA-256
     check, publication record committed with [skip ci]. CI on GitHub (Windows) is the
     clean-checkout run, so the suite is not repeated locally in the clone.
Nothing is pushed without --publish. Commits carry no attribution trailers.
"""
import argparse
import datetime
import hashlib
import json
import os
import re
import shutil
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
REPO = 'PashOK7/HallJoy'
REMOTE = f'https://github.com/{REPO}.git'
STATE = ROOT / '.local/release-state.json'  # last main commit the workspace is synced with
SKIP_TOP = {'.local', 'build', 'outputs', '.git', '.vs', '.cache', '.analysis'}
# Local-only files outside the redistribution policy (correspondence, local traces).
LOCAL_ONLY = {
    'src/HallJoyProject/tests/data/titan68_turbo_trace_valid.log',
    'docs/current/PWNAGE_SUPPORT_CORRESPONDENCE_2026-09-22.md',
}
ASSETS = ['HallJoy.exe', 'LICENSE', 'SHA256SUMS.txt', 'THIRD_PARTY_NOTICES.md']
PRIVATE = [
    re.compile(r'[A-Za-z]:[\\/](?:Users|Downloads|Desktop)[\\/]', re.I),
    re.compile(r'\b[A-Za-z]:\\(?:github|tools)\\', re.I),
    re.compile(r'[\w.+-]+@[\w-]+\.(?:com|ru|net|org)\b'),
]
LLVM = Path(r'C:\Program Files\LLVM\bin\clang++.exe')


def run(*args, cwd=ROOT, capture=False, env=None):
    print('+ ' + ' '.join(str(a) for a in args), flush=True)
    result = subprocess.run([str(a) for a in args], cwd=cwd, env=env, text=True,
                            capture_output=capture, encoding='utf8', errors='replace')
    if result.returncode != 0:
        if capture:
            print(result.stdout + result.stderr)
        raise SystemExit(f'FAILED ({result.returncode}): {args[0]} {args[1] if len(args) > 1 else ""}')
    return result.stdout if capture else ''


def git(clone, *args):
    return run('git', '-C', clone, *args, capture=True)


def eol(data):
    crlf, lf = data.count(b'\r\n'), data.count(b'\n')
    return 'crlf' if crlf and crlf == lf else 'lf' if not crlf else 'mixed'


def with_eol(data, style):
    data = data.replace(b'\r\n', b'\n')
    return data.replace(b'\n', b'\r\n') if style == 'crlf' else data


def bump_version(version):
    major, minor, patch = (int(x) for x in version.split('.'))
    path = ROOT / 'src/HallJoyProject/HallJoy/version.h'
    text = path.read_bytes().decode()
    text = re.sub(r'(HALLJOY_VERSION_MAJOR )\d+', rf'\g<1>{major}', text)
    text = re.sub(r'(HALLJOY_VERSION_MINOR )\d+', rf'\g<1>{minor}', text)
    text = re.sub(r'(HALLJOY_VERSION_PATCH )\d+', rf'\g<1>{patch}', text)
    text = re.sub(r'HALLJOY_VERSION_TUPLE \d+,\d+,\d+,0', f'HALLJOY_VERSION_TUPLE {major},{minor},{patch},0', text)
    text = re.sub(r'\d+\.\d+\.\d+(?=(?:\.0)?")', version, text)
    path.write_bytes(text.encode())
    index = ROOT / 'docs/releases/README.md'
    text = index.read_bytes().decode('utf8')
    entry = f'- [{version}](RELEASE_NOTES_v{version}.md)\n'
    if entry not in text:
        first = re.search(r'^- \[', text, re.M)
        text = text[:first.start()] + entry + text[first.start():]
        index.write_bytes(text.encode('utf8'))
    (ROOT / 'docs/releases/RELEASE_NOTES_NEXT.md').write_bytes(b'# Next release (unreleased)\n')


def fresh_clone(version):
    clone = ROOT / f'.local/release-{version}'
    if clone.exists():  # scratch from an earlier attempt of this version; never reused
        def writable(func, path, _):
            os.chmod(path, 0o700)
            func(path)
        shutil.rmtree(clone, onerror=writable)
    run('git', '-c', 'core.autocrlf=false', 'clone', '-q', REMOTE, clone)
    git(clone, 'config', 'core.autocrlf', 'false')
    head = git(clone, 'rev-parse', 'HEAD').strip()
    if STATE.exists():
        base = json.loads(STATE.read_text())['main']
        changed = [n for n in git(clone, 'diff', '--name-only', base, head).splitlines() if n]
        conflicts = [n for n in changed if not (ROOT / n).is_file() or (ROOT / n).read_bytes() != (clone / n).read_bytes()]
        if conflicts:
            raise SystemExit('main changed files since the last sync that differ in the workspace; merge them first:\n  '
                             + '\n  '.join(conflicts))
    return clone, head


def transfer(clone):
    policy = set(json.loads((ROOT / 'docs/legal/PUBLICATION_POLICY.json').read_text(encoding='utf8'))['denied_paths'])
    tracked = set(n for n in git(clone, 'ls-files', '-z').split('\0') if n)
    normalized = []
    for folder, dirs, files in os.walk(ROOT):
        rel = Path(folder).relative_to(ROOT)
        if rel.parts and rel.parts[0] in SKIP_TOP:
            dirs[:] = []
            continue
        dirs[:] = [d for d in dirs if d != '__pycache__' and (rel.parts or d not in SKIP_TOP)]
        for name in files:
            path = (rel / name).as_posix()
            if name.endswith('.pyc') or path in policy or path in LOCAL_ONLY:
                continue
            data = (ROOT / path).read_bytes()
            target = clone / path
            if path in tracked:
                old = target.read_bytes()
                style = eol(old)
                if style != 'mixed' and eol(data) != style and b'\0' not in data:
                    data = with_eol(data, style)          # keep main's style, no whole-file churn
                    (ROOT / path).write_bytes(data)       # workspace = what gets published
                    normalized.append(path)
                if old == data:
                    continue
            target.parent.mkdir(parents=True, exist_ok=True)
            target.write_bytes(data)
    git(clone, 'clean', '-fdXq')                          # ignored copies never reach the index
    git(clone, 'add', '-A')
    staged = [n for n in git(clone, 'diff', '--cached', '--name-only').splitlines() if n]
    return staged, normalized


def scan_private(clone):
    diff = git(clone, 'diff', '--cached', '-U0')
    hits, current = [], None
    for line in diff.splitlines():
        if line.startswith('+++ '):
            current = line[6:]
        elif line.startswith('+') and any(p.search(line) for p in PRIVATE):
            hits.append(f'{current}: {line[:160]}')
    if hits:
        raise SystemExit('Private data in added lines (fix in the workspace, rerun):\n  ' + '\n  '.join(hits))


def same_as_clone(clone):
    names = [n for n in git(clone, 'ls-files', '-z').split('\0') if n]
    diff = [n for n in names if not (ROOT / n).is_file() or (ROOT / n).read_bytes() != (clone / n).read_bytes()]
    if diff:
        raise SystemExit('Workspace differs from the publication clone: ' + ', '.join(diff[:20]))


def build_and_package(version):
    env = dict(os.environ)
    if LLVM.is_file():
        env['CXX'] = str(LLVM)  # MSVC runtime: no MinGW libstdc++ mix-up from PATH (as CI)
    run(sys.executable, 'tools/run_native_backend_checks.py', '--require-compiler', env=env)
    run('powershell', '-NoProfile', '-ExecutionPolicy', 'Bypass', '-File', 'tools/build_release.ps1', env=env)
    out = ROOT / f'build/packages/HallJoy-{version}-Windows-x64'
    if out.exists():
        shutil.rmtree(out)  # a failed earlier attempt; the package is rebuilt from this exact EXE
    run('powershell', '-NoProfile', '-ExecutionPolicy', 'Bypass', '-File', 'tools/package_release.ps1')
    package = out / 'HallJoy'
    exe = hashlib.sha256((package / 'HallJoy.exe').read_bytes()).hexdigest()
    return package, exe


def publish(clone, version, title, package, exe, transfer_counts):
    git(clone, 'commit', '-q', '-m', f'Release {version}: {title.split(": ", 1)[-1]}')
    commit = git(clone, 'rev-parse', '--short', 'HEAD').strip()
    git(clone, 'tag', '-a', f'v{version}', '-m', f'HallJoy {version}')
    git(clone, 'push', '-q', 'origin', 'main')
    git(clone, 'push', '-q', 'origin', f'v{version}')
    notes = ROOT / f'docs/releases/RELEASE_NOTES_v{version}.md'
    run('gh', 'release', 'create', f'v{version}', '--repo', REPO, '--title', title,
        '--notes-file', notes, '--latest', *[package / a for a in ASSETS])
    remote = json.loads(run('gh', 'api', f'repos/{REPO}/releases/tags/v{version}', capture=True))
    digests = {a['name']: a.get('digest', '') for a in remote['assets']}
    for name in ASSETS:
        local = 'sha256:' + hashlib.sha256((package / name).read_bytes()).hexdigest()
        if digests.get(name) != local:
            raise SystemExit(f'Uploaded {name} differs from the package ({digests.get(name)} != {local})')
    today = datetime.date.today().isoformat()
    record = ROOT / f'docs/current/RELEASE_{version}_PUBLICATION_{today}.md'
    record.write_bytes(f'''# HallJoy {version} publication — {today}

Title: {title}. Published with `tools/release.py`.

- Release commit {commit}, tag `v{version}`, https://github.com/{REPO}/releases/tag/v{version} (latest).
- Transfer: {transfer_counts[0]} files written, {transfer_counts[1]} normalised to main's line endings;
  denied, local and local-only paths skipped. Private-data scan and check_publication_inputs: PASS.
- Native suite and build_release.ps1: PASS. Workspace = clone for every tracked file.
- EXE SHA-256 `{exe}`, version {version}.0. Server-side SHA-256 of {", ".join(ASSETS)} equals the package.
- Clean-checkout tests: GitHub CI (Windows) on {commit}.
'''.encode('utf8'))
    context = ROOT / 'docs/current/OWNER_CONTEXT.md'
    entry = (f'> {today} PUBLISHED stable/latest {version}: https://github.com/{REPO}/releases/tag/v{version} '
             f'(commit {commit}, EXE {exe[:8]}). See {record.name}.\n')
    context.write_bytes(entry.encode('utf8') + context.read_bytes())
    for path in (record, context):
        rel = path.relative_to(ROOT)
        data = path.read_bytes()
        if (clone / rel).exists():
            data = with_eol(data, eol((clone / rel).read_bytes()))
            path.write_bytes(data)
        (clone / rel).write_bytes(data)
    git(clone, 'add', '-A')
    git(clone, 'commit', '-q', '-m', f'Record {version} publication [skip ci]')
    git(clone, 'push', '-q', 'origin', 'main')
    head = git(clone, 'rev-parse', 'HEAD').strip()
    STATE.write_text(json.dumps({'main': head, 'version': version}))
    return commit


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument('version')
    parser.add_argument('title')
    parser.add_argument('--publish', action='store_true')
    args = parser.parse_args()
    if not re.fullmatch(r'\d+\.\d+\.\d+', args.version):
        raise SystemExit('version must look like 1.6.10')
    notes = ROOT / f'docs/releases/RELEASE_NOTES_v{args.version}.md'
    if not notes.is_file():
        raise SystemExit(f'Write {notes.relative_to(ROOT)} first (public patch notes).')
    bump_version(args.version)
    clone, head = fresh_clone(args.version)
    written, normalized = transfer(clone)
    scan_private(clone)
    run(sys.executable, 'tools/check_publication_inputs.py', '--root', clone)
    same_as_clone(clone)
    package, exe = build_and_package(args.version)
    same_as_clone(clone)  # the build must not have changed a published file
    print(f'READY {args.version}: {len(written)} files, EXE {exe}, clone {clone.relative_to(ROOT)}')
    if not args.publish:
        print('Not published (no --publish). Review `git -C <clone> diff --cached --stat`, then rerun with --publish.')
        return 0
    commit = publish(clone, args.version, args.title, package, exe, (len(written), len(normalized)))
    print(f'PUBLISHED {args.version}: commit {commit}, EXE {exe}')
    return 0


if __name__ == '__main__':
    sys.exit(main())
