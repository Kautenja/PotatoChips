"""Validate the whole native batch, then losslessly crop dark panels.

Copyright 2026 Arhythmetic Units. SPDX-License-Identifier: GPL-3.0-or-later
"""
import argparse
import hashlib
import io
import json
from pathlib import Path
from PIL import Image, ImageStat, ImageChops
from run import ROOT, fingerprint, selected

VIEWS = ('Light', 'Dark', 'Preview-Light', 'Preview-Dark',
         'Restored', 'Preview-Restored')


def prepare(capture_dir, module=None, theme="Dark"):
    if theme not in ("Light", "Dark"):
        raise ValueError("Unknown publication theme")
    rows = selected(module)
    batch = json.loads((capture_dir / 'batch.json').read_text())
    if batch['inventory'] != rows or batch['source_sha256'] != fingerprint():
        raise ValueError('Stale or mismatched capture batch; run capture again')
    images = []
    baseline = json.loads((ROOT / 'specs/assets/012/layout.json').read_text())['modules']
    for row in rows:
        name = row['manual'] or row['slug']
        report = json.loads((capture_dir / f'{name}.json').read_text())
        expected = next(m for m in baseline if m['slug'] == row['slug'])['controls']
        actual = report['controls']
        if len(actual) != len(expected) or any(
                any(before[k] != value for k, value in after.items())
                for before, after in zip(expected, actual)):
            raise ValueError(f'Control geometry differs from approved layout: {name}')
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
                if view == theme and row['manual']:
                    data = io.BytesIO()
                    panel.save(data, format='PNG')
                    images.append((row['manual'], data.getvalue()))
        for prefix in ('', 'Preview-'):
            with Image.open(capture_dir / f'{name}-{prefix}Light.ppm') as light, \
                    Image.open(capture_dir / f'{name}-{prefix}Dark.ppm') as dark:
                if not ImageChops.difference(light, dark).getbbox():
                    raise ValueError(f'Theme did not change pixels: {name}')
        for first, restored in [('Light', 'Restored'),
                                ('Preview-Light', 'Preview-Restored')]:
            with Image.open(capture_dir / f'{name}-{first}.ppm') as before, \
                    Image.open(capture_dir / f'{name}-{restored}.ppm') as after:
                if ImageChops.difference(before, after).getbbox():
                    raise ValueError(f'Context restoration changed pixels: {name}')
    return images


def export(capture_dir, manual_dir, module=None, theme="Dark"):
    images = prepare(capture_dir, module, theme)
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
    parser.add_argument('--theme', choices=['Light', 'Dark'], default='Dark')
    args = parser.parse_args()
    export(args.capture_dir, args.manual_dir, args.module, args.theme)


if __name__ == '__main__':
    main()
