"""lang_tools.py add and the extract/update guard (python3 -m unittest discover tools -p 'test_lang_tools.py')"""
import argparse
import contextlib
import io
import os
import sys
import tempfile
import unittest
from pathlib import Path

sys.path.insert(0, os.path.dirname(__file__))
import lang_tools as lt  # noqa: E402

HEADER = '# AutoBleem {} Translation\n# Format: English Text=Translated Text\n# Untranslated: {} / Total: {}\n\n'


def lang_file(name, lines, untranslated=0):
    return HEADER.format(name, untranslated, len(lines)) + ''.join(l + '\n' for l in lines)


class Tree(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.dir = Path(self.tmp.name)
        self.en = ['Apple=Apple', 'Banana=Banana', 'banana=banana', 'Cherry=Cherry']
        self.de = ['Apple=Apfel', 'Banana=', 'banana=Banane', 'Cherry=Kirsche']
        (self.dir / 'English.txt').write_text(lang_file('English', self.en), encoding='utf-8')
        (self.dir / 'Deutsch.txt').write_text(lang_file('Deutsch', self.de, 1), encoding='utf-8')
        (self.dir / 'Polski.txt').write_text(lang_file('Polski', ['Apple=', 'Banana=', 'banana=', 'Cherry=Wisnia'], 3),
                                             encoding='utf-8')

    def tearDown(self):
        self.tmp.cleanup()

    def read(self, name):
        return (self.dir / f'{name}.txt').read_bytes().decode('utf-8')

    def add(self, *items, from_file=None):
        out = io.StringIO()
        with contextlib.redirect_stdout(out), contextlib.redirect_stderr(out):
            code = lt.cmd_add(argparse.Namespace(lang_dir=str(self.dir), items=list(items), from_file=from_file))
        return code, out.getvalue()


class Add(Tree):
    def test_inserts_in_the_sorted_place_and_touches_nothing_else(self):
        before = {n: self.read(n) for n in ('English', 'Deutsch', 'Polski')}
        code, out = self.add('Apricot', 'Deutsch=Aprikose')
        self.assertEqual(code, 0)
        for name, value in (('English', 'Apricot'), ('Deutsch', 'Aprikose'), ('Polski', '')):
            old, new = before[name].split('\n'), self.read(name).split('\n')
            self.assertEqual(len(new), len(old) + 1)
            at = new.index(f'Apricot={value}')
            self.assertEqual(new[at - 1], 'Apple=' + ('Apple' if name == 'English' else old[old.index(
                [l for l in old if l.startswith('Apple=')][0])].split('=', 1)[1]))
            rest = new[:at] + new[at + 1:]
            self.assertEqual([l for l in rest if not l.startswith('# Untranslated')],
                             [l for l in old if not l.startswith('# Untranslated')])
        self.assertIn('Polski', out)
        self.assertNotIn('Deutsch,', out)

    def test_case_insensitive_then_by_case(self):
        self.add('BANANA')  # same letters: by case, 'BANANA' < 'Banana' < 'banana'
        self.assertEqual(self.read('English').split('\n')[4:9],
                         ['Apple=Apple', 'BANANA=BANANA', 'Banana=Banana', 'banana=banana', 'Cherry=Cherry'])
        self.add('apple')  # after 'Apple'
        self.assertEqual(self.read('English').split('\n')[4:6], ['Apple=Apple', 'apple=apple'])

    def test_header_counts_only_that_line_changes(self):
        self.add('Date', 'Deutsch=Dattel')
        self.assertIn('# Untranslated: 1 / Total: 5\n', self.read('Deutsch'))
        self.assertIn('# Untranslated: 4 / Total: 5\n', self.read('Polski'))
        self.assertIn('# Untranslated: 0 / Total: 5\n', self.read('English'))

    def test_english_gets_key_equals_key(self):
        self.add('Fig')
        self.assertIn('\nFig=Fig\n', self.read('English'))
        self.assertIn('\nFig=\n', self.read('Deutsch'))

    def test_duplicate_key_is_left_and_said(self):
        before = self.read('Deutsch')
        code, out = self.add('Cherry', 'Deutsch=Anders')
        self.assertEqual(code, 0)
        self.assertEqual(self.read('Deutsch'), before)
        self.assertIn('already there', out)

    def test_unknown_language_or_bad_key_changes_nothing(self):
        before = {n: self.read(n) for n in ('English', 'Deutsch', 'Polski')}
        self.assertEqual(self.add('Fig', 'Klingon=x')[0], 1)
        self.assertEqual(self.add('a=b')[0], 1)
        self.assertEqual(self.add('Fig', 'nonsense')[0], 1)
        self.assertEqual({n: self.read(n) for n in before}, before)

    def test_values_with_equals_and_from_file(self):
        tsv = self.dir / 'new.tsv'
        tsv.write_text('# new strings\nFig\tDeutsch=Feige = gut\tPolski=Figa\nGrape\n', encoding='utf-8')
        code, out = self.add(from_file=str(tsv))
        self.assertEqual(code, 0)
        self.assertIn('\nFig=Feige = gut\n', self.read('Deutsch'))
        self.assertIn('\nFig=Figa\n', self.read('Polski'))
        self.assertIn('\nGrape=\n', self.read('Deutsch'))
        self.assertIn('# Untranslated: 4 / Total: 6\n', self.read('Polski'))

    def test_crlf_and_an_unsorted_tail_are_kept(self):
        text = lang_file('Deutsch', ['Apple=Apfel', 'Cherry=Kirsche', 'Zebra=Zebra', 'Banana=Banane']).replace('\n', '\r\n')
        (self.dir / 'Deutsch.txt').write_bytes(text.encode('utf-8'))
        self.add('Berry')
        new = self.read('Deutsch')
        self.assertIn('Apple=Apfel\r\nBerry=\r\n', new)
        self.assertEqual(new.count('\n'), new.count('\r\n'))


class Guard(Tree):
    def run_update(self, **kw):
        ns = argparse.Namespace(lang_dir=str(self.dir), remove_obsolete=False, force=False, dry_run=False)
        for k, v in kw.items():
            setattr(ns, k, v)
        out = io.StringIO()
        with contextlib.redirect_stdout(out), contextlib.redirect_stderr(out):
            code = lt.cmd_update(ns)
        return code, out.getvalue()

    def drift(self, n):
        keys = [f'K{i:03}' for i in range(n)]
        (self.dir / 'English.txt').write_text(lang_file('English', [f'{k}={k}' for k in keys]), encoding='utf-8')

    def test_small_change_is_written(self):
        code, _ = self.run_update()
        self.assertEqual(code, 0)

    def test_big_change_stops_with_2_and_writes_nothing(self):
        self.drift(30)
        before = self.read('Deutsch')
        code, out = self.run_update()
        self.assertEqual(code, 2)
        self.assertEqual(self.read('Deutsch'), before)
        self.assertIn('Deutsch.txt', out)
        self.assertIn('lines would change', out)

    def test_force_writes(self):
        self.drift(30)
        code, _ = self.run_update(force=True)
        self.assertEqual(code, 0)
        self.assertIn('K029=', self.read('Deutsch'))

    def test_dry_run_prints_counts_and_writes_nothing(self):
        self.drift(30)
        before = self.read('Deutsch')
        code, out = self.run_update(dry_run=True)
        self.assertEqual(code, 0)
        self.assertEqual(self.read('Deutsch'), before)
        self.assertRegex(out, r'Deutsch.txt\s+\d+ lines would change')

    def test_extract_guard(self):
        src = self.dir / 'src'
        src.mkdir()
        (src / 'a.cpp').write_text(''.join(f'x(_("S{i}"));\n' for i in range(40)), encoding='utf-8')
        ns = argparse.Namespace(lang_dir=str(self.dir), src_dir=[str(src)], force=False, dry_run=False)
        before = self.read('English')
        with contextlib.redirect_stdout(io.StringIO()), contextlib.redirect_stderr(io.StringIO()):
            self.assertEqual(lt.cmd_extract(ns), 2)
        self.assertEqual(self.read('English'), before)
        ns.force = True
        with contextlib.redirect_stdout(io.StringIO()):
            self.assertEqual(lt.cmd_extract(ns), 0)
        self.assertIn('S39=S39', self.read('English'))


if __name__ == '__main__':
    unittest.main()
