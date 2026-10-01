"""Run native captures in a fresh directory; publish a batch only on success.

Copyright 2026 Arhythmetic Units. SPDX-License-Identifier: GPL-3.0-or-later
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import platform
import shutil
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]
INVENTORY = Path(__file__).with_name('modules.json')


def selected(module=None):
    rows = json.loads(INVENTORY.read_text())
    manifest = json.loads((ROOT / 'plugin.json').read_text())
    enabled = {r['slug'] for r in manifest['modules'] if not r.get('disabled')}
    if len(rows) != len(enabled) or {r['slug'] for r in rows} != enabled:
        raise ValueError('Enabled manifest inventory changed; review modules.json')
    if module:
        rows = [r for r in rows if (r['manual'] or r['slug']) == module]
        if not rows:
            raise ValueError(f'Unknown capture module: {module}')
    return rows


def fingerprint():
    """Hash rendering inputs, including uncommitted edits, not just HEAD."""
    digest = hashlib.sha256()
    paths = [ROOT / 'plugin.json']
    for directory in ('src', 'res', 'tools/capture'):
        paths.extend(p for p in (ROOT / directory).rglob('*') if p.is_file()
                     and '.build' not in p.parts and '__pycache__' not in p.parts)
    for path in sorted(paths):
        digest.update(str(path.relative_to(ROOT)).encode())
        digest.update(path.read_bytes())
    return digest.hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--rack', required=True, type=Path)
    parser.add_argument('--build', type=Path, default=Path('.build'))
    parser.add_argument('--module')
    args = parser.parse_args()
    rows = selected(args.module)
    build = args.build.resolve()
    build.mkdir(parents=True, exist_ok=True)
    # Validate assets before constructors can silently substitute empty panels.
    for row in rows:
        for panel in (row['panel'], row['panel'].replace('.svg', '-dark.svg')):
            if not (ROOT / 'res' / panel).is_file():
                raise ValueError(f'Missing panel: {panel}')
    for index in range(8):
        if not (ROOT / 'res/BossFight_algorithms' / f'{index}.svg').is_file():
            raise ValueError('Missing Boss Fight algorithm frame')
    source_hash = fingerprint()
    with tempfile.TemporaryDirectory(prefix='capture-', dir=build) as temporary:
        output = Path(temporary)
        (output / 'user').mkdir()
        env = dict(os.environ, DYLD_LIBRARY_PATH=str(args.rack.resolve()),
                   LD_LIBRARY_PATH=str(args.rack.resolve()))
        subprocess.run([str(build / 'capture'), str(args.rack.resolve()),
                        str(ROOT), str(output), str(INVENTORY),
                        args.module or 'all'], env=env, check=True, timeout=120)
        if fingerprint() != source_hash:
            raise ValueError('Sources changed during capture; retry')
        report = dict(source_sha256=source_hash, inventory=rows,
                      revision=subprocess.check_output(
                          ['git', 'rev-parse', 'HEAD'], cwd=ROOT, text=True).strip(),
                      platform=platform.platform(), sample_rate=48000, samples=4800,
                      fixture='Constructor defaults; no patched signals; RNG seed 0x504f5441544f/0x4348495053',
                      rack_library_sha256={p.name: hashlib.sha256(p.read_bytes()).hexdigest()
                                           for p in args.rack.glob('libRack.*')},
                      themes='Native light/dark panels, ports and screws; state/history/geometry unchanged')
        (output / 'batch.json').write_text(json.dumps(report, indent=2) + '\n')
        destination = build / 'captures'
        old = build / 'captures-old'
        if old.exists():
            shutil.rmtree(old)
        if destination.exists():
            destination.rename(old)
        try:
            shutil.move(str(output), destination)
        except BaseException:
            if old.exists():
                old.rename(destination)
            raise
        if old.exists():
            shutil.rmtree(old)


if __name__ == '__main__':
    main()
