#!/usr/bin/env python3
"""Reject stale source copies, version drift and unresolved TeX diagnostics."""
import json
import re
import sys
from pathlib import Path


def check(kind, name, *extra):
    path = Path(name)
    if kind == 'output':
        build = path.resolve()
        if (build == Path.cwd().resolve() or any(build.glob('*.tex'))
                or any(build.glob('*.sty')) or (build / 'img').exists()
                or (build / 'sections').exists()):
            raise ValueError('Output directory contains source files; choose a fresh .build directory.')
    elif kind == 'source':
        version = json.loads(Path('../../plugin.json').read_text())['version']
        if r'\newcommand{\manualversion}{' + version + '}' not in path.read_text():
            raise ValueError('Manual version does not match plugin.json')
    elif kind == 'log':
        text = path.read_text(errors='replace')
        # TeX wraps warnings at its print width; match across that whitespace.
        flat = re.sub(r'\s+', ' ', text)
        diagnostics = [r'Overfull \\[hv]box', r'undefined references',
                       r'Citation .{0,180}? unde\s*fined',
                       r'Reference .{0,180}? unde\s*fined',
                       r'Label\(s\) may have changed', r'multiply defined']
        failures = [pattern for pattern in diagnostics if re.search(pattern, flat)]
        if failures:
            raise ValueError('Unresolved manual diagnostics in ' + str(path) + ': ' + ', '.join(failures))
    else:
        raise ValueError('Unknown manual check: ' + kind)


if __name__ == '__main__':
    try:
        check(*sys.argv[1:])
    except (ValueError, OSError) as error:
        sys.exit(str(error))
