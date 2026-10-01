"""Native failure-path checks in disposable directories; needs a desktop.

Copyright 2026 Arhythmetic Units. SPDX-License-Identifier: GPL-3.0-or-later
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import subprocess
import tempfile
from run import ROOT, INVENTORY


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--rack', required=True, type=Path)
    parser.add_argument('--build', required=True, type=Path)
    args = parser.parse_args()
    rack, build = args.rack.resolve(), args.build.resolve()
    env = dict(os.environ, DYLD_LIBRARY_PATH=str(rack), LD_LIBRARY_PATH=str(rack))

    def assets():
        return {str(p): hashlib.sha256(p.read_bytes()).hexdigest()
                for p in ROOT.glob('manual/*/img/Panel.png')}

    before = assets()
    with tempfile.TemporaryDirectory(prefix='potato-native-failure-') as directory:
        output = Path(directory)
        (output / 'user').mkdir()
        inventory = json.loads(INVENTORY.read_text())
        inventory[0]['width'] += 15
        wrong = output / 'wrong.json'
        wrong.write_text(json.dumps(inventory))

        def render(plugin, inventory_path):
            return subprocess.run([str(build / 'capture'), str(rack), str(plugin),
                                   str(output), str(inventory_path), 'Blocks'],
                                  env=env, capture_output=True, text=True, timeout=30)

        result = render(ROOT, wrong)
        assert result.returncode and 'Geometry changed' in result.stderr, result.stderr
        assert assets() == before
        print('PASS: native wrong geometry preserves publication PNGs')
        plugin = output / 'plugin'
        (plugin / 'res').mkdir(parents=True)
        for path in (ROOT / 'res').iterdir():
            if path.name != 'Blocks.svg':
                (plugin / 'res' / path.name).symlink_to(path)
        result = render(plugin, INVENTORY)
        assert result.returncode, result.stdout
        assert assets() == before
        print('PASS: native missing panel preserves publication PNGs')
        failed = output / 'failed-renderer'
        failed.mkdir()
        (failed / 'capture').write_text('#!/bin/sh\nexit 23\n')
        (failed / 'capture').chmod(0o755)
        (failed / 'captures').mkdir()
        sentinel = failed / 'captures/sentinel'
        sentinel.write_text('previous complete batch')
        result = subprocess.run(['python3', str(Path(__file__).with_name('run.py')),
                                 '--rack', str(rack), '--build', str(failed)],
                                env=env, capture_output=True, text=True, timeout=30)
        assert result.returncode and sentinel.read_text() == 'previous complete batch'
        assert assets() == before
        print('PASS: failed renderer preserves prior batch and publication PNGs')


if __name__ == '__main__':
    main()
