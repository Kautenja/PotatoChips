#!/usr/bin/env python3
"""Exercise real build rules and publication gates in disposable fixtures."""
import json
import os
from pathlib import Path
import shutil
import subprocess
import tempfile
import time
import unittest

ROOT = Path(__file__).resolve().parents[1]

class BuildTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix='potato-build-')
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.plugin = 'plugin.dll' if os.environ.get('OS') == 'Windows_NT' else 'plugin.so'
        shutil.copy(ROOT / 'Makefile', self.root)
        for name in ('mk', 'scripts'):
            shutil.copytree(ROOT / name, self.root / name)
        for name in ('src', 'test/dsp', 'dep/catch2-v3', 'sdk'):
            (self.root / name).mkdir(parents=True)
        (self.root / 'src/value.hpp').write_text('#define VALUE 0\n')
        (self.root / 'src/detail').mkdir()
        (self.root / 'src/detail/helper.cpp').write_text('int helper() { return 0; }\n')
        (self.root / 'test/dsp/test_one.cpp').write_text('''#include "value.hpp"
#include "detail/helper.cpp"
#ifdef RACK_FIXTURE
#error Rack flags leaked into standalone tests
#endif
int main() { return VALUE + helper(); }
''')
        (self.root / 'dep/catch2-v3/catch_amalgamated.cpp').write_text('int harness;\n')
        (self.root / 'src/plugin.cpp').write_text('#include "sdk_value.hpp"\nint main() { return SDK_VALUE; }\n')
        (self.root / 'sdk/sdk_value.hpp').write_text('#define SDK_VALUE 0\n')
        (self.root / 'sdk/plugin.mk').write_text('''CXXFLAGS += -std=c++11 -DRACK_FIXTURE -Isdk -MMD -MP
TARGET := plugin$(if $(filter Windows_NT,$(OS)),.dll,.so)
OBJECTS := $(patsubst %,build/%.o,$(SOURCES))
DEPENDENCIES := $(patsubst %,build/%.d,$(SOURCES))
all: $(TARGET)
$(TARGET): $(OBJECTS)
\t$(CXX) $(CXXFLAGS) -shared -o $@ $^
-include $(DEPENDENCIES)
clean:
\trm -rf build $(TARGET) dist
.DEFAULT_GOAL := all
''')

    def make(self, *args, success=True):
        env = os.environ.copy()
        for key in ('MAKEFLAGS', 'MFLAGS', 'MAKELEVEL', 'TEST_MODE'):
            env.pop(key, None)
        result = subprocess.run(['make', '-j2', 'RACK_DIR=missing-sdk', *args],
                                cwd=self.root, env=env, text=True,
                                stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
        self.assertEqual(result.returncode == 0, success, result.stdout)
        return result.stdout

    def test_standalone_incremental_flags_compiler_and_failure(self):
        self.assertIn(' -c ', self.make('test'))
        self.assertNotIn(' -c ', self.make('test'))
        time.sleep(1.05)
        (self.root / 'src/value.hpp').write_text('#define VALUE 0\n// changed header\n')
        self.assertIn(' -c ', self.make('test'))
        self.assertIn(' -c ', self.make('test', 'CXXFLAGS=-DCHANGED'))
        self.assertNotIn(' -c ', self.make('test', 'CXXFLAGS=-DCHANGED'))
        self.assertIn(' -c ', self.make('test', 'CXX=env c++'))
        time.sleep(1.05)
        (self.root / 'src/value.hpp').write_text('#define VALUE 1\n')
        self.make('test', success=False)
        self.make('test-build')  # Building alone must not execute a failed suite.
        self.make('clean')
        self.assertFalse((self.root / '.build').exists())

    def test_mixed_goals_sdk_changes_and_default(self):
        self.make('all', 'test', success=False)
        self.make('RACK_DIR=sdk')
        self.assertTrue((self.root / self.plugin).exists())
        self.make('all', 'test', 'RACK_DIR=sdk')
        self.assertNotIn(' -c ', self.make('all', 'test', 'RACK_DIR=sdk'))
        time.sleep(1.05)
        (self.root / 'sdk/sdk_value.hpp').write_text('#define SDK_VALUE 0\n// changed SDK header\n')
        self.assertIn(' -c ', self.make('all', 'RACK_DIR=sdk'))
        shutil.copytree(self.root / 'sdk', self.root / 'sdk2')
        self.assertIn(' -c ', self.make('all', 'RACK_DIR=sdk2'))
        self.assertIn(' -c ', self.make('all', 'RACK_DIR=sdk2', 'LDFLAGS=-g'))
        (self.root / 'sdk2/libRack.so').write_text('changed SDK library identity')
        self.assertIn(' -c ', self.make('all', 'RACK_DIR=sdk2', 'LDFLAGS=-g'))
        self.make('clean', 'RACK_DIR=missing-sdk')
        self.assertFalse((self.root / self.plugin).exists())

    def test_manual_completeness_metadata_version_and_tag_gate(self):
        shutil.copy(ROOT / 'plugin.json', self.root)
        manifest = json.loads((self.root / 'plugin.json').read_text())
        pdfs = self.root / 'pdfs'
        pdfs.mkdir()
        for module in manifest['modules']:
            if not module.get('disabled') and module.get('manualUrl'):
                name = module['manualUrl'].rsplit('/', 1)[1]
                (pdfs / name).write_text(module['name'] + '\n' + manifest['version'] + '\n' + 'musical controls ' * 100)
        tools = self.root / 'tools'
        tools.mkdir()
        for name, body in {
            'pdfinfo': 'echo "Title: Module manual"; echo "Author: Christian Kauten"',
            'pdftotext': 'cat "$1"',
        }.items():
            file = tools / name
            file.write_text('#!/bin/sh\n' + body + '\n')
            file.chmod(0o755)
        env = dict(os.environ, PATH=str(tools) + os.pathsep + os.environ['PATH'])
        def validate(kind, value, succeeds):
            result = subprocess.run(['python3', 'scripts/validate.py', kind, str(value)],
                                    cwd=self.root, env=env, capture_output=True, text=True)
            self.assertEqual(result.returncode == 0, succeeds, result.stderr)
        validate('manuals', pdfs, True)
        sample = pdfs / 'PotKeys.pdf'
        text = sample.read_text()
        sample.unlink()
        validate('manuals', pdfs, False)
        sample.write_text(text.replace(manifest['version'], '0.0.0'))
        validate('manuals', pdfs, False)
        sample.write_text(text.replace(manifest['version'], manifest['version'] + '0'))
        validate('manuals', pdfs, False)
        sample.write_text(text + ' ??')
        validate('manuals', pdfs, False)
        sample.write_text(text)
        (tools / 'pdfinfo').write_text('#!/bin/sh\nexit 1\n')
        validate('manuals', pdfs, False)
        validate('tag', 'v' + manifest['version'], True)
        validate('tag', 'v0.0.0', False)

if __name__ == '__main__':
    unittest.main()
