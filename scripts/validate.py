#!/usr/bin/env python3
"""Validate dependency, package and publication contracts without publishing."""
import argparse
import hashlib
import io
import json
from pathlib import Path
import re
import subprocess
import tarfile
from urllib.parse import urlparse

ROOT = Path(__file__).resolve().parents[1]
MANIFEST = json.loads((ROOT / 'plugin.json').read_text())


def check_tag(tag):
    if tag.removeprefix('v') != MANIFEST['version']:
        raise ValueError(f'Tag {tag} does not match manifest {MANIFEST["version"]}')


def dependencies():
    pins = json.loads((ROOT / 'dep/catch2-v3/provenance.json').read_text())
    for name, digest in pins['sha256'].items():
        data = (ROOT / 'dep/catch2-v3' / name).read_bytes()
        if hashlib.sha256(data).hexdigest() != digest:
            raise ValueError(f'Catch2 checksum mismatch: {name}')
    if (ROOT / 'docs/licenses/BSL-1.0.txt').read_bytes() != (ROOT / 'dep/catch2-v3/LICENSE_1_0.txt').read_bytes():
        raise ValueError('Packaged Catch2 license differs from vendored license')


def package(path):
    data = subprocess.check_output(['zstd', '-dc', str(path)])
    prefix = MANIFEST['slug'] + '/'
    with tarfile.open(fileobj=io.BytesIO(data)) as archive:
        names = archive.getnames()
        expected = [ROOT / 'LICENSE', ROOT / 'LICENSING.md', ROOT / 'plugin.json']
        for directory in ('docs/licenses', 'res', 'presets'):
            expected += [p for p in (ROOT / directory).rglob('*') if p.is_file()]
        for source in expected:
            name = prefix + source.relative_to(ROOT).as_posix()
            member = archive.getmember(name)
            if not member.isfile() or archive.extractfile(member).read() != source.read_bytes():
                raise ValueError(f'Missing or changed package file: {name}')
        binaries = [n for n in names if n in [prefix + 'plugin' + suffix for suffix in ('.so', '.dll', '.dylib')]]
        if len(binaries) != 1 or archive.getmember(binaries[0]).size == 0:
            raise ValueError('Expected one nonempty plugin binary')
        if any(n.startswith(prefix + d) for n in names for d in ('dep/', 'test/', '.build/')):
            raise ValueError('Development files in plugin package')


def manuals(directory):
    """Spec 004's strict publication gate; current legacy PDFs may fail."""
    expected = [m for m in MANIFEST['modules'] if not m.get('disabled') and m.get('manualUrl')]
    if len(expected) != 14:
        raise ValueError('Review manual inventory when enabled modules change')
    names = {Path(urlparse(m['manualUrl']).path).name for m in expected}
    if {p.name for p in directory.glob('*.pdf')} != names:
        raise ValueError('Missing or unexpected manual PDF; require all 14 sound modules, no blanks/disabled entries')
    for module in expected:
        path = directory / Path(urlparse(module['manualUrl']).path).name
        info = subprocess.check_output(['pdfinfo', str(path)], text=True)
        text = subprocess.check_output(['pdftotext', str(path), '-'], text=True)
        title = re.search(r'^Title:\s*(.+)', info, re.M)
        author = re.search(r'^Author:\s*(.+)', info, re.M)
        if not title or not author or len(text.split()) < 100:
            raise ValueError(f'Missing metadata or meaningful text: {path.name}')
        if module['name'].lower() not in (title[1] + ' ' + text).lower():
            raise ValueError(f'Module identity missing: {path.name}')
        if MANIFEST['version'] not in text or '??' in text:
            raise ValueError(f'Wrong/missing version or unresolved references: {path.name}')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('kind', choices=['dependencies', 'package', 'tag', 'manuals'])
    parser.add_argument('value', nargs='?')
    args = parser.parse_args()
    if args.kind == 'dependencies':
        dependencies()
    elif args.kind == 'tag':
        check_tag(args.value or '')
    elif not args.value:
        parser.error('package/manuals requires a path')
    else:
        {'package': package, 'manuals': manuals}[args.kind](Path(args.value))
    print(f'{args.kind}: passed')

if __name__ == '__main__':
    main()
