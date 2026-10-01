"""Validate the whole native batch, then losslessly crop light panels.

Copyright 2026 Arhythmetic Units. SPDX-License-Identifier: GPL-3.0-or-later
"""
import argparse
import hashlib
import io
import json
from pathlib import Path
from PIL import Image, ImageStat, ImageChops
from run import fingerprint, selected

VIEWS = ('Light', 'Dark', 'Preview-Light', 'Preview-Dark',
         'Restored', 'Preview-Restored')


def prepare(capture_dir, module=None):
    rows = selected(module)
    batch = json.loads((capture_dir / 'batch.json').read_text())
    if batch['inventory'] != rows or batch['source_sha256'] != fingerprint():
        raise ValueError('Stale or mismatched capture batch; run capture again')
    images = []
    for row in rows:
        name = row['manual'] or row['slug']
        report = json.loads((capture_dir / f'{name}.json').read_text())
        ratio = report['pixel_ratio']
        if not isinstance(ratio, int) or not 1 <= ratio <= 4:
            raise ValueError('Invalid pixel ratio')
        for view in VIEWS:
            with Image.open(capture_dir / f'{name}-{view}.ppm') as source:
                if (source.format != 'PPM' or source.mode != 'RGB' or
                        source.size != ((row['width'] + 20) * ratio, 420 * ratio)):
                    raise ValueError(f'Unexpected capture geometry: {name}-{view}')
                panel = source.crop(tuple(n * ratio for n in
                                         (10, 20, 10 + row['width'], 400)))
                if max(ImageStat.Stat(panel).stddev) < 5:
                    raise ValueError(f'Blank render: {name}-{view}')
                if view == 'Light' and row['manual']:
                    data = io.BytesIO()
                    panel.save(data, format='PNG')
                    images.append((row['manual'], data.getvalue()))
        for first, restored in [('Light', 'Restored'),
                                ('Preview-Light', 'Preview-Restored')]:
            with Image.open(capture_dir / f'{name}-{first}.ppm') as before, \
                    Image.open(capture_dir / f'{name}-{restored}.ppm') as after:
                if ImageChops.difference(before, after).getbbox():
                    raise ValueError(f'Context restoration changed pixels: {name}')
    return images


def export(capture_dir, manual_dir, module=None):
    images = prepare(capture_dir, module)
    # Preflight all destinations; retain original bytes for rollback on I/O error.
    destinations = [(manual_dir / name / 'img/Panel.png', data) for name, data in images]
    for path, _ in destinations:
        if not path.parent.is_dir():
            raise ValueError(f'Missing manual image directory: {path.parent}')
    originals = {p: p.read_bytes() if p.exists() else None for p, _ in destinations}
    try:
        for path, data in destinations:
            temporary = path.with_suffix('.png.tmp')
            temporary.write_bytes(data)
            temporary.replace(path)
    except BaseException:
        for path, data in originals.items():
            if data is None:
                path.unlink(missing_ok=True)
            else:
                path.write_bytes(data)
        raise
    finally:
        for path, _ in destinations:
            path.with_suffix('.png.tmp').unlink(missing_ok=True)
    for path, data in destinations:
        print(f'{path}: sha256={hashlib.sha256(data).hexdigest()}')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('capture_dir', type=Path)
    parser.add_argument('manual_dir', type=Path)
    parser.add_argument('--module')
    args = parser.parse_args()
    export(args.capture_dir, args.manual_dir, args.module)


if __name__ == '__main__':
    main()
