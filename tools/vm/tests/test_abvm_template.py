"""abvm's sandbox template from a pcusb package: fake tarball, fake host dirs, no VM (python -m unittest discover tools/vm/tests)"""
import hashlib
import io
import json
import os
import sys
import tarfile
import tempfile
import unittest
from unittest import mock

sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..'))
import abvm  # noqa: E402


def write(path, text):
    os.makedirs(os.path.dirname(path), exist_ok=True)
    with open(path, 'w') as f:
        f.write(text)


def read(path):
    with open(path) as f:
        return f.read()


class TemplateFromPackage(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.host = os.path.join(self.tmp.name, 'sandboxes')
        os.makedirs(self.host)
        self.t = os.path.join(self.host, '_template')
        self.patches = [mock.patch.object(abvm, 'SB_HOST', self.host),
                        mock.patch.object(abvm, 'sb_open_modes', lambda path: None),
                        mock.patch('builtins.print')]
        for p in self.patches:
            p.start()

    def tearDown(self):
        for p in self.patches:
            p.stop()
        self.tmp.cleanup()

    def package_dir(self, version='v2.0.0-alpha3'):
        pkg = os.path.join(self.tmp.name, 'src', 'autobleem-pcusb')
        write(os.path.join(pkg, 'Autobleem', 'bin', 'autobleem', 'autobleem-gui'), 'gui')
        write(os.path.join(pkg, 'Autobleem', 'bin', 'abpad', 'abpad'), 'pad')
        write(os.path.join(pkg, 'Autobleem', 'bin', 'db', 'covers.db'), 'not for the template')
        write(os.path.join(pkg, 'Autobleem', 'rc', 'rc.ini'), 'rc')
        write(os.path.join(pkg, 'Themes', 'ab2.0.0', 'theme.ini'), 'new')
        write(os.path.join(pkg, 'extensions', 'store', 'extension.ini'), 'Version=1.0.2\n')
        write(os.path.join(pkg, 'Apps', 'MoviePlayer', 'app.ini'), 'app')
        write(os.path.join(pkg, 'VERSION'), version + '\n')
        return pkg

    def tarball(self, version='v2.0.0-alpha3'):
        pkg = self.package_dir(version)
        path = os.path.join(self.tmp.name, f'autobleem-pcusb-i386-{version}.tar.gz')
        with tarfile.open(path, 'w:gz') as tar:
            tar.add(pkg, arcname='autobleem-pcusb')
        return path

    def stale_template(self):
        write(os.path.join(self.t, 'Themes', 'ab2', 'theme.ini'), 'stale')
        write(os.path.join(self.t, 'VERSION'), 'v1.0.0\n')

    def test_tarball_makes_the_template_with_the_stick_variants_parts(self):
        self.stale_template()
        abvm.sb_template(self.tarball())
        self.assertEqual(read(os.path.join(self.t, 'Autobleem', 'bin', 'autobleem', 'autobleem-gui')), 'gui')
        self.assertEqual(read(os.path.join(self.t, 'Autobleem', 'bin', 'abpad', 'abpad')), 'pad')
        self.assertEqual(read(os.path.join(self.t, 'Autobleem', 'rc', 'rc.ini')), 'rc')
        self.assertEqual(read(os.path.join(self.t, 'Themes', 'ab2.0.0', 'theme.ini')), 'new')
        self.assertEqual(read(os.path.join(self.t, 'Extensions', 'store', 'extension.ini')), 'Version=1.0.2\n')
        self.assertTrue(os.path.isfile(os.path.join(self.t, 'Apps', 'MoviePlayer', 'app.ini')))
        self.assertEqual(read(os.path.join(self.t, 'VERSION')), 'v2.0.0-alpha3\n')
        self.assertFalse(os.path.exists(os.path.join(self.t, 'Themes', 'ab2')))  # nothing of the old one stays
        self.assertFalse(os.path.exists(self.t + '.new'))

    def test_an_old_template_that_cannot_be_removed_never_leaves_a_half_template(self):
        self.stale_template()
        real_rmtree = abvm.shutil.rmtree

        def rmtree(path, *args, **kwargs):  # the root-owned old tree: the host cannot delete it
            if str(path).endswith('_template.old'):
                return
            real_rmtree(path, *args, **kwargs)

        with mock.patch.object(abvm.shutil, 'rmtree', rmtree), mock.patch('builtins.print') as shown:
            abvm.sb_template(self.package_dir())
        self.assertEqual(read(os.path.join(self.t, 'VERSION')), 'v2.0.0-alpha3\n')  # the new one, whole
        self.assertTrue(os.path.isfile(os.path.join(self.t, 'Themes', 'ab2.0.0', 'theme.ini')))
        self.assertFalse(os.path.exists(os.path.join(self.t, 'Themes', 'ab2')))
        self.assertFalse(os.path.exists(self.t + '.new'))
        self.assertEqual(read(os.path.join(self.t + '.old', 'VERSION')), 'v1.0.0\n')
        said = [c[0][0] for c in shown.call_args_list]
        self.assertTrue(any('_template.old' in s and 'sudo rm -rf' in s for s in said), said)

    def test_leftovers_of_an_earlier_run_are_cleared_by_the_guest_when_the_host_cannot(self):
        write(os.path.join(self.t + '.old', 'VERSION'), 'x')
        write(os.path.join(self.t + '.new', 'VERSION'), 'y')
        real_rmtree = abvm.shutil.rmtree
        guest = []

        def rmtree(path, *args, **kwargs):
            if str(path).endswith('_template.old'):
                return
            real_rmtree(path, *args, **kwargs)

        with mock.patch.object(abvm.shutil, 'rmtree', rmtree), \
                mock.patch.object(abvm, 'guest_run', lambda cmd, *a, **k: guest.append(cmd)):
            abvm.sb_clear_leftover('_template.old')
            abvm.sb_clear_leftover('_template.new')
        self.assertEqual(guest, [f'sudo rm -rf {abvm.SB_GUEST}/_template.old'])
        self.assertFalse(os.path.exists(self.t + '.new'))

    def test_db_is_the_link_to_shared_and_the_user_dirs_are_empty(self):
        abvm.sb_template(self.package_dir())
        db = os.path.join(self.t, 'Autobleem', 'bin', 'db')
        self.assertTrue(os.path.islink(db))
        self.assertEqual(os.readlink(db), '../../../_shared/db')
        for d in ('System/Databases', 'System/Logs', 'Games'):
            self.assertEqual(os.listdir(os.path.join(self.t, d)), [])

    def test_template_source_records_package_version_and_date_and_list_shows_it(self):
        abvm.sb_template(self.tarball())
        lines = read(os.path.join(self.t, 'TEMPLATE_SOURCE')).splitlines()
        self.assertEqual(lines[0], 'source: autobleem-pcusb-i386-v2.0.0-alpha3.tar.gz')
        self.assertEqual(lines[1], 'version: v2.0.0-alpha3')
        self.assertRegex(lines[2], r'^made: \d{4}-\d\d-\d\d \d\d:\d\d$')
        with mock.patch('builtins.print') as shown:
            abvm.sb_list()
        self.assertIn('autobleem-pcusb-i386-v2.0.0-alpha3.tar.gz, v2.0.0-alpha3', shown.call_args_list[0][0][0])

    def test_list_without_a_record_says_so(self):
        with mock.patch('builtins.print') as shown:
            abvm.sb_list()
        self.assertIn('template: none', shown.call_args_list[0][0][0])

    def test_a_package_without_a_launcher_is_refused_and_the_template_kept(self):
        self.stale_template()
        os.makedirs(os.path.join(self.tmp.name, 'empty'))
        with self.assertRaises(abvm.Fail):
            abvm.sb_template(os.path.join(self.tmp.name, 'empty'))
        self.assertEqual(read(os.path.join(self.t, 'VERSION')), 'v1.0.0\n')

    def test_nightly_downloads_the_sites_newest_pcusb_package_and_checks_its_sha256(self):
        tar = self.tarball('v2.0.0-alpha4-9')
        with open(tar, 'rb') as f:
            data = f.read()
        latest = {'version': 'v2.0.0-alpha4-9', 'files': {'pcusb': {'sha256': hashlib.sha256(data).hexdigest()}}}
        urls = []

        def urlopen(url, timeout=None):
            urls.append(url)
            return io.BytesIO(json.dumps(latest).encode())

        def urlretrieve(url, path):
            urls.append(url)
            with open(path, 'wb') as f:
                f.write(data)

        with mock.patch('urllib.request.urlopen', urlopen), mock.patch('urllib.request.urlretrieve', urlretrieve):
            abvm.sb_template('nightly')
            self.assertEqual(urls, [f'{abvm.SITE}/nightly/latest.json',
                                    f'{abvm.SITE}/nightly/v2.0.0-alpha4-9/autobleem-pcusb-i386-v2.0.0-alpha4-9.tar.gz'])
            self.assertEqual(read(os.path.join(self.t, 'VERSION')), 'v2.0.0-alpha4-9\n')
            latest['files']['pcusb']['sha256'] = '0' * 64
            write(os.path.join(self.t, 'VERSION'), 'kept\n')
            with self.assertRaises(abvm.Fail):
                abvm.sb_template('nightly')
            self.assertEqual(read(os.path.join(self.t, 'VERSION')), 'kept\n')

    def test_command_passes_the_package_and_refuses_other_arguments(self):
        with mock.patch.object(abvm, 'LOCAL', True), mock.patch.object(abvm, 'lease_op'), \
                mock.patch.object(abvm, 'sb_template') as made:
            abvm.sandbox_command(['template', '--package', 'nightly'], '.')
            made.assert_called_once_with('nightly')
            made.reset_mock()
            abvm.sandbox_command(['template'], '.')
            made.assert_called_once_with()
            with self.assertRaises(abvm.Fail):
                abvm.sandbox_command(['template', '--bogus'], '.')


if __name__ == '__main__':
    unittest.main()
