#!/usr/bin/env python3
"""Run every DSP suite with diagnostics; report coverage by ownership."""
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys

mode = sys.argv[1]
if mode not in ('coverage', 'asan-ubsan'):
    raise SystemExit('Expected coverage or asan-ubsan')
root = Path(__file__).resolve().parents[1]
os.chdir(root)
report = root / '.build' / mode / 'reports'
report.mkdir(parents=True, exist_ok=True)
env = os.environ.copy()
for key in ('MAKEFLAGS', 'MFLAGS', 'MAKELEVEL'):
    env.pop(key, None)
env['ASAN_OPTIONS'] = 'halt_on_error=1'
env['UBSAN_OPTIONS'] = 'halt_on_error=1:print_stacktrace=1'
raw = report / 'raw'
if raw.exists():
    shutil.rmtree(raw)
raw.mkdir()
env['LLVM_PROFILE_FILE'] = str(raw / '%m-%p.profraw')
compiler = env.get('INSTRUMENT_CXX', 'clang++')

def run(command, output):
    with output.open('w') as log:
        result = subprocess.run(command, stdout=log, stderr=subprocess.STDOUT, env=env)
    print(output.read_text(), end='')
    return result.returncode

status = run(['make', '-j2', 'test-build', f'TEST_MODE={mode}',
              f'CXX={compiler}', 'RACK_DIR=/nonexistent-sdk'], report / 'build.log')
if status:
    raise SystemExit(status)
binaries = sorted(p for p in (root / '.build' / mode / 'dsp/dsp').rglob('test_*')
                  if p.suffix in ('', '.exe'))
if len(binaries) != len(list((root / 'test/dsp').rglob('*.cpp'))):
    raise SystemExit('Missing or stale DSP test binaries; clean this configuration')
for binary in binaries:
    status |= run([str(binary)], report / (binary.stem + '.log'))

if mode == 'coverage':
    def llvm_tool(variable, name):
        if variable in env:
            return env[variable]
        if shutil.which(name):
            return name
        return subprocess.check_output(['xcrun', '--find', name], text=True).strip()
    profdata = llvm_tool('LLVM_PROFDATA', 'llvm-profdata')
    cov = llvm_tool('LLVM_COV', 'llvm-cov')
    profile = report / 'coverage.profdata'
    subprocess.run([profdata, 'merge', '-sparse', *map(str, raw.glob('*.profraw')),
                    '-o', str(profile)], check=True)
    args = [str(binaries[0])]
    for binary in binaries[1:]:
        args += ['-object', str(binary)]
    args += [f'-instr-profile={profile}']
    exported = json.loads(subprocess.check_output([cov, 'export', *args]))
    (report / 'coverage.json').write_text(json.dumps(exported))
    groups = {'first-party': [], 'imported-dsp': [], 'test-harness': []}
    for file in exported['data'][0]['files']:
        path = Path(file['filename']).resolve()
        try:
            relative = path.relative_to(root).as_posix()
        except ValueError:
            continue
        if relative.startswith(('src/dsp/trigger/', 'src/dsp/math/')) or relative in (
                'src/dsp/pcm.hpp', 'src/dsp/exceptions.hpp', 'src/dsp/sony_s_dsp/common.hpp'):
            group = 'first-party'
        elif relative.startswith('src/dsp/'):
            group = 'imported-dsp'
        else:
            group = 'test-harness'
        groups[group].append(str(path))
    for group, sources in groups.items():
        if not sources:
            continue
        output = subprocess.check_output([cov, 'report', *args, '-sources', *sources])
        (report / f'{group}.txt').write_bytes(output)
    subprocess.run([cov, 'show', *args, '-format=html',
                    f'-output-dir={report / "html"}'], check=True)
    print('Coverage includes only code mapped by these suites, not all plugin code.')
    print((report / 'first-party.txt').read_text())
print(f'Diagnostics: {report.relative_to(root)}')
raise SystemExit(status)
