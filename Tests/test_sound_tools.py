"""Lightweight build-tool checks; no sample library or audio rendering required."""
import contextlib
import importlib.util
import io
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import textwrap
import unittest
from unittest import mock
import zipfile

ROOT = Path(__file__).resolve().parent.parent
TOOLS = ROOT / 'Tools'


def load_tool(name):
    spec = importlib.util.spec_from_file_location(name, TOOLS / (name + '.py'))
    module = importlib.util.module_from_spec(spec)
    with mock.patch.object(sys, 'path', [str(TOOLS)] + sys.path):
        spec.loader.exec_module(module)
    return module


class SelectionTests(unittest.TestCase):
    def test_selection_from_another_directory(self):
        with tempfile.TemporaryDirectory(prefix='grain field ') as directory:
            work = Path(directory)
            vcsl = work / 'sample library'
            sample = vcsl / 'Percussion' / 'Ocean Drum' / 'waves.wav'
            sample.parent.mkdir(parents=True)
            sample.touch()
            output = work / 'generated' / 'selection.json'
            subprocess.run([sys.executable, str(TOOLS / 'select_samples.py'),
                            '--vcsl', str(vcsl), '--output', str(output)],
                           cwd=work, check=True, capture_output=True, text=True)
            selected = json.loads(output.read_text())
            self.assertEqual(selected['ocean'], ['Percussion/Ocean Drum/waves.wav'])
            self.assertTrue(all(not Path(p).is_absolute() for group in selected.values() for p in group))

    def test_missing_input_does_not_overwrite_selection(self):
        with tempfile.TemporaryDirectory() as directory:
            output = Path(directory) / 'selection.json'
            output.write_text('keep me')
            result = subprocess.run([sys.executable, str(TOOLS / 'select_samples.py'),
                                     '--vcsl', str(Path(directory) / 'missing'),
                                     '--output', str(output)], capture_output=True, text=True)
            self.assertEqual(result.returncode, 2)
            self.assertIn('VCSL checkout not found', result.stderr)
            self.assertEqual(output.read_text(), 'keep me')

    def test_selector_import_is_side_effect_free(self):
        module = load_tool('select_samples')
        self.assertEqual(module.files, [])
        self.assertEqual(module.ROOT, ROOT)


@unittest.skipUnless(importlib.util.find_spec('numpy') and importlib.util.find_spec('scipy'),
                     'Install NumPy and SciPy to test the audio build tools')
class BuilderTests(unittest.TestCase):
    def test_default_paths_are_repository_relative(self):
        builder = load_tool('build_sounds')
        self.assertEqual(builder.VCSL, ROOT / '.tools' / 'VCSL')
        self.assertEqual(builder.OUT, ROOT / 'Resources' / 'Sounds')
        self.assertEqual(builder.INC, ROOT / 'Source' / 'BuiltinSounds.inc')
        self.assertEqual(builder.WORK, ROOT / 'build' / 'soundbuild')
        self.assertEqual(builder.sel, {})

    def test_path_overrides_and_selection(self):
        builder = load_tool('build_sounds')
        with tempfile.TemporaryDirectory(prefix='grain field ') as directory:
            work = Path(directory).resolve()
            (work / 'library').mkdir()
            (work / 'selection.json').write_text('{"ocean": ["Ocean Drum/waves.wav"]}')
            previous = Path.cwd()
            try:
                os.chdir(work)
                builder.configure_paths(['--vcsl', 'library', '--selection', 'selection.json',
                                         '--output', 'output', '--include', 'source/catalogue.inc',
                                         '--work-dir', 'scratch'])
            finally:
                os.chdir(previous)
            self.assertEqual(builder.VCSL, work / 'library')
            self.assertEqual(builder.OUT, work / 'output')
            self.assertEqual(builder.INC, work / 'source' / 'catalogue.inc')
            self.assertEqual(builder.WORK, work / 'scratch')
            self.assertEqual(builder.sel['ocean'], ['Ocean Drum/waves.wav'])
            self.assertTrue(builder.OUT.is_dir())
            self.assertTrue(builder.INC.parent.is_dir())
            self.assertTrue(builder.WORK.is_dir())

    def test_invalid_selection_creates_no_outputs(self):
        builder = load_tool('build_sounds')
        with tempfile.TemporaryDirectory() as directory:
            work = Path(directory)
            (work / 'library').mkdir()
            with contextlib.redirect_stderr(io.StringIO()), self.assertRaises(SystemExit) as error:
                builder.configure_paths(['--vcsl', str(work / 'library'),
                                         '--selection', str(work / 'missing.json'),
                                         '--output', str(work / 'output')])
            self.assertEqual(error.exception.code, 2)
            self.assertFalse((work / 'output').exists())

    def test_texture_import_does_not_build_or_delete_audio(self):
        with mock.patch.object(Path, 'mkdir', side_effect=AssertionError('unexpected mkdir')), \
             mock.patch('os.remove', side_effect=AssertionError('unexpected deletion')), \
             mock.patch('subprocess.run', side_effect=AssertionError('unexpected rendering')):
            textures = load_tool('build_textures')
        self.assertEqual(textures.catalogue, [])
        self.assertEqual(textures.sel, {})

    def test_builders_help_from_another_directory(self):
        with tempfile.TemporaryDirectory() as directory:
            for name in ('build_sounds.py', 'build_textures.py'):
                with self.subTest(name=name):
                    result = subprocess.run([sys.executable, str(TOOLS / name), '--help'],
                                            cwd=directory, check=True, capture_output=True, text=True)
                    for flag in ('--vcsl', '--selection', '--output', '--include', '--work-dir'):
                        self.assertIn(flag, result.stdout)


class PackagingTests(unittest.TestCase):
    def test_workflow_zip_contains_original_license_notices(self):
        workflow = (ROOT / '.github' / 'workflows' / 'build.yml').read_text()
        package = workflow.split('      - name: Package\n        run: |\n', 1)[1]
        script = textwrap.dedent(package.split('\n      - uses:', 1)[0])
        notices = ['LICENSE', 'Resources/Fonts/Silkscreen-OFL.txt',
                   'Resources/Fonts/VT323-OFL.txt', 'Resources/Sounds/CREDITS.txt']
        with tempfile.TemporaryDirectory(prefix='grain field package ') as directory:
            work = Path(directory)
            for relative in notices:
                target = work / relative
                target.parent.mkdir(parents=True, exist_ok=True)
                shutil.copyfile(ROOT / relative, target)
            for format_, name in [('AU', 'Grain Field.component'), ('VST3', 'Grain Field.vst3'),
                                  ('Standalone', 'Grain Field.app')]:
                bundle = work / 'build' / 'GrainField_artefacts' / 'Release' / format_ / name
                bundle.mkdir(parents=True)
                (bundle / 'fixture').write_text('packaging fixture, not a binary')
            subprocess.run(['bash', '-e', '-c', script], cwd=work, check=True, capture_output=True)
            with zipfile.ZipFile(work / 'Grain Field macOS.zip') as archive:
                for source, packaged in [
                    ('LICENSE', 'LICENSE'),
                    ('Resources/Fonts/Silkscreen-OFL.txt', 'Notices/Fonts/Silkscreen-OFL.txt'),
                    ('Resources/Fonts/VT323-OFL.txt', 'Notices/Fonts/VT323-OFL.txt'),
                    ('Resources/Sounds/CREDITS.txt', 'Notices/Sound-Credits.txt'),
                ]:
                    self.assertEqual(archive.read(packaged), (ROOT / source).read_bytes())
                self.assertNotIn('Notices/Fonts/DMMono-OFL.txt', archive.namelist())
                self.assertNotIn('Notices/Fonts/InstrumentSerif-OFL.txt', archive.namelist())

    def test_embedded_fonts_have_matching_oled_notices(self):
        cmake = (ROOT / 'CMakeLists.txt').read_text()
        fonts = cmake.split('juce_add_binary_data(GrainFieldFonts SOURCES', 1)[1].split(')', 1)[0]
        for name in ('VT323-Regular.ttf', 'Silkscreen-Regular.ttf', 'Silkscreen-Bold.ttf'):
            self.assertIn('Resources/Fonts/' + name, fonts)
            self.assertTrue((ROOT / 'Resources' / 'Fonts' / name).is_file())
        for name in ('VT323-OFL.txt', 'Silkscreen-OFL.txt'):
            self.assertIn('SIL OPEN FONT LICENSE', (ROOT / 'Resources' / 'Fonts' / name).read_text())


if __name__ == '__main__':
    unittest.main()
