#!/usr/bin/env python3
"""Exercise the real shared recipes in a disposable source tree (requires TeX)."""
import shutil
import subprocess
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
with tempfile.TemporaryDirectory(prefix='potato-manual-test-') as directory:
    root = Path(directory)
    shutil.copy(ROOT / 'plugin.json', root)
    source = root / 'manual'
    source.mkdir()
    shutil.copy(ROOT / 'manual/makefile', source)
    for name in ('latex', 'SuperEcho'):
        shutil.copytree(ROOT / 'manual' / name, source / name,
                        ignore=shutil.ignore_patterns('.build', 'build', '__pycache__'))
    manual = source / 'SuperEcho'
    original = (manual / 'manual.tex').read_text()
    pdf = manual / '.build/manual.pdf'
    collection = source / '.build/SuperEcho.pdf'

    def run(*args, success=True):
        result = subprocess.run(['make', '-C', str(source), 'MODULES=SuperEcho', *args],
                                text=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
        if (result.returncode == 0) != success:
            raise AssertionError(result.stdout)
        return result.stdout

    run()
    assert pdf.is_file() and collection.read_bytes() == pdf.read_bytes()
    assert {p.name for p in pdf.parent.iterdir()} == {'manual.pdf'}
    print('PASS: fresh source-relative build, collection copy and auxiliary cleanup')

    for name, addition in [('missing input', r'\input{does-not-exist}'),
                           ('undefined reference', r'\ref{does-not-exist}'),
                           ('overflow', r'\noindent\rule{50cm}{1pt}\par')]:
        (manual / 'manual.tex').write_text(original.replace(r'\manualcolophon', addition + '\n' + r'\manualcolophon'))
        output = run(success=False)
        assert not pdf.exists() and not collection.exists(), output
        assert (manual / '.build/manual.log').exists()
        print('PASS: ' + name + ' fails, preserves log, removes failed/stale PDFs')

    (manual / 'manual.tex').write_text(original.replace(r'\manualversion}{', r'\manualversion}{9'))
    assert 'version does not match' in run(success=False)
    run('clean')
    print('PASS: version drift rejected; clean works after a version change')
    (manual / 'manual.tex').write_text(original)
    stale = manual / 'build'
    stale.mkdir()
    (stale / 'manual.tex').write_text('legacy source copy')
    for target in ('manual', 'clean'):
        result = subprocess.run(['make', '-C', str(manual), 'BUILD=build', target],
                                text=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
        assert result.returncode and 'contains source files' in result.stdout
        assert (stale / 'manual.tex').read_text() == 'legacy source copy'
    print('PASS: legacy source-copy directories rejected by build and clean')
    run()
    run('clean')
    assert not pdf.exists() and not collection.exists()
    print('PASS: rebuild recovers and clean removes both PDF outputs')
