"""Exercise export failure safety using a real, previously rendered batch.

Copyright 2026 Arhythmetic Units. SPDX-License-Identifier: GPL-3.0-or-later
"""
import json
from pathlib import Path
import shutil
import tempfile
from unittest.mock import patch
from PIL import Image, ImageChops
from export_screenshots import export
from run import selected


def main():
    with tempfile.TemporaryDirectory(prefix='potato-capture-test-') as directory:
        root = Path(directory)
        captures = root / 'captures'
        shutil.copytree(Path(__file__).parent / '.build/captures', captures)
        manuals = root / 'manual'
        for row in selected():
            if row['manual']:
                path = manuals / row['manual'] / 'img'
                path.mkdir(parents=True)
                (path / 'Panel.png').write_bytes(b'existing publication asset')

        def snapshot():
            return {p: p.read_bytes() for p in manuals.rglob('Panel.png')}

        def must_fail(action):
            before = snapshot()
            try:
                action()
            except (ValueError, OSError):
                pass
            else:
                raise AssertionError('Expected rejection')
            assert snapshot() == before, 'Failed batch modified publication assets'

        source = captures / 'StepSaw-Light.ppm'
        original = source.read_bytes()
        with Image.open(source) as image:
            image.crop((0, 0, image.width - 1, image.height)).save(source)
        must_fail(lambda: export(captures, manuals))
        source.write_bytes(original)
        print('PASS: late-batch geometry mismatch preserves all existing PNGs')
        source.unlink()
        must_fail(lambda: export(captures, manuals))
        source.write_bytes(original)
        print('PASS: missing renderer output preserves all PNGs')
        with Image.open(source) as image:
            Image.new('RGB', image.size, 'gray').save(source)
        must_fail(lambda: export(captures, manuals))
        source.write_bytes(original)
        print('PASS: blank renderer output preserves all PNGs')
        dark = captures / 'StepSaw-Dark.ppm'
        dark_bytes = dark.read_bytes()
        dark.write_bytes(original)
        must_fail(lambda: export(captures, manuals))
        dark.write_bytes(dark_bytes)
        print('PASS: identical theme views rejected')
        batch_path = captures / 'batch.json'
        batch_text = batch_path.read_text()
        batch = json.loads(batch_text)
        batch['source_sha256'] = 'stale'
        batch_path.write_text(json.dumps(batch))
        must_fail(lambda: export(captures, manuals))
        batch_path.write_text(batch_text)
        print('PASS: stale capture batch rejected')
        replace = Path.replace
        replacements = 0

        def fail_second(path, target):
            nonlocal replacements
            replacements += 1
            if replacements == 2:
                raise OSError('Injected destination I/O failure')
            return replace(path, target)

        with patch.object(Path, 'replace', fail_second):
            must_fail(lambda: export(captures, manuals))
        print('PASS: destination I/O failure rolls back earlier updates')
        batch = json.loads(batch_text)
        batch['inventory'] = selected('PotKeys')
        batch_path.write_text(json.dumps(batch))
        before = snapshot()
        export(captures, manuals, 'PotKeys')
        changed = [p for p, content in before.items() if p.read_bytes() != content]
        assert changed == [manuals / 'PotKeys/img/Panel.png']
        print('PASS: single-module export changes only PotKeys')
        batch_path.write_text(batch_text)
        export(captures, manuals)
        for row in selected():
            if row['manual']:
                with Image.open(manuals / row['manual'] / 'img/Panel.png') as image:
                    ratio = json.loads((captures / f'{row["manual"]}.json').read_text())['pixel_ratio']
                    assert image.size == (row['width'] * ratio, 380 * ratio)
                    with Image.open(captures / f'{row["manual"]}-Dark.ppm') as dark:
                        crop = dark.crop(tuple(v * ratio for v in (10, 20, row['width'] + 10, 400)))
                        assert not ImageChops.difference(image, crop).getbbox()
        print('PASS: all publication images use the exact dark-theme crop')
        print('PASS: complete real batch crops at native pixel density')


if __name__ == '__main__':
    main()
