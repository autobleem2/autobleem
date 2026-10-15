"""abvm's fixed sandbox pool sb1..sb4: take-free, refusal of other names, re-lay on take, list, rm-legacy
(a faked guest: no VM is touched)"""
import contextlib
import io
import os
import shutil
import sys
import tempfile
import unittest
from unittest import mock

sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..'))
import abvm  # noqa: E402


def cp_r(cmd, **kw):
    shutil.copytree(cmd[-2], cmd[-1])  # `cp -r <template> <new slot>`: the slot does not exist yet


class Pool(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.host = os.path.join(self.tmp.name, 'sandboxes')
        os.makedirs(os.path.join(self.host, '_template'))
        with open(os.path.join(self.host, '_template', 'marker'), 'w') as f:
            f.write('template')
        self.running = set()   # names the faked guest runs
        self.stopped = []
        self.patches = [
            mock.patch.dict(os.environ, {'HOME': self.tmp.name}),
            mock.patch.object(abvm, 'SB_HOST', self.host),
            mock.patch.object(abvm, 'LOCAL', True),
            mock.patch.object(abvm, 'WHO', 'alice'),
            mock.patch.object(abvm, 'LEASE_KEY', 'vm'),
            mock.patch.object(abvm, 'vm_prepare', lambda cmd, args: None),  # no VM to wake here
            mock.patch.object(abvm, 'sb_open_modes', lambda p: None),
            mock.patch.object(abvm.subprocess, 'run', side_effect=self.fake_run),
            mock.patch.object(abvm, 'sb_running', lambda n: {'port': 1} if n in self.running else None),
            mock.patch.object(abvm, 'sb_orphans', lambda: {}),
            mock.patch.object(abvm, 'sb_stop', self.stop),
        ]
        for p in self.patches:
            p.start()

    def tearDown(self):
        for p in self.patches:
            p.stop()
        self.tmp.cleanup()

    def fake_run(self, cmd, **kw):
        if cmd[0] == 'cp':
            return cp_r(cmd)
        if cmd[0] == 'rm':
            shutil.rmtree(cmd[-1], ignore_errors=True)
            return mock.Mock(stderr='')
        raise AssertionError(cmd)

    def stop(self, name, use_driver=True):
        self.stopped.append(name)
        self.running.discard(name)

    def sb(self, *args, who='alice'):
        out, err = io.StringIO(), io.StringIO()
        with mock.patch.object(abvm, 'WHO', who), contextlib.redirect_stdout(out), contextlib.redirect_stderr(err):
            code = abvm.main(['--local', '--who', who, 'sandbox'] + list(args))
        return code, out.getvalue(), err.getvalue()

    def mkdir(self, name, size=0):
        p = os.path.join(self.host, name)
        os.makedirs(p)
        if size:
            with open(os.path.join(p, 'data'), 'wb') as f:
                f.write(b'x' * size)

    def test_non_pool_name_is_refused_except_rm(self):
        for sub in ('take', 'new', 'start', 'drive', 'reset', 'stop', 'release', 'logs'):
            code, _, err = self.sb(sub, 'mine', 'task')
            self.assertEqual(code, 1, sub)
            self.assertIn('sandbox mine: not in the pool sb1..sb4 - use take-free', err)
        self.mkdir('old')
        with mock.patch.object(abvm, 'lease_op', lambda *a, **k: 'ok'):
            code, out, _ = self.sb('rm', 'old')
        self.assertEqual(code, 0, out)
        self.assertFalse(os.path.exists(os.path.join(self.host, 'old')))

    def test_take_free_takes_in_order_and_prints_slot_last(self):
        code, out, _ = self.sb('take-free', 'job-a', '5', who='alice')
        self.assertEqual((code, out.splitlines()[-1]), (0, 'slot: sb1'))
        code, out, _ = self.sb('take-free', 'job-b', who='bob')
        self.assertEqual((code, out.splitlines()[-1]), (0, 'slot: sb2'))
        self.assertTrue(os.path.isfile(os.path.join(self.host, 'sb2', 'marker')))

    def test_take_free_busy_exit_3_names_the_holders(self):
        for i, who in enumerate(('a', 'b', 'c', 'd')):
            self.assertEqual(self.sb('take-free', f'job{i}', who=who)[0], 0)
        code, _, err = self.sb('take-free', 'late', who='e')
        self.assertEqual(code, 3)
        self.assertIn('busy: no free sandbox slot (sb1..sb4: sb1 a (job0)', err)
        self.assertIn('sb4 d (job3)', err)

    def test_take_free_skips_a_held_slot_and_kills_a_launcher_left_in_a_free_one(self):
        self.sb('take-free', 'job', who='a')           # sb1 held
        self.running.add('sb2')                        # a launcher left running, lease free
        code, out, _ = self.sb('take-free', 'job', who='b')
        self.assertEqual((code, out.splitlines()[-1]), (0, 'slot: sb2'))
        self.assertIn('sb2', self.stopped)
        self.assertNotIn('sb2', self.running)

    def test_take_lays_the_slot_fresh_from_the_template(self):
        self.mkdir('sb3')
        with open(os.path.join(self.host, 'sb3', 'leftover'), 'w') as f:
            f.write('last user')
        self.running.add('sb3')
        code, _, err = self.sb('take', 'sb3', 'job')
        self.assertEqual(code, 0, err)
        self.assertIn('sb3', self.stopped)
        self.assertFalse(os.path.exists(os.path.join(self.host, 'sb3', 'leftover')))
        self.assertTrue(os.path.isfile(os.path.join(self.host, 'sb3', 'marker')))

    def test_take_by_the_holder_again_keeps_its_work(self):
        self.sb('take', 'sb1', 'job')
        with open(os.path.join(self.host, 'sb1', 'mine'), 'w') as f:
            f.write('x')
        self.assertEqual(self.sb('take', 'sb1', 'job', '40')[0], 0)
        self.assertTrue(os.path.isfile(os.path.join(self.host, 'sb1', 'mine')))

    def test_list_shows_the_pool_the_template_and_the_legacy_count(self):
        self.mkdir('old1')
        self.mkdir('old2')
        self.mkdir('_shared')
        self.sb('take-free', 'job')
        code, out, _ = self.sb('list')
        self.assertEqual(code, 0)
        for n in ('sb1', 'sb2', 'sb3', 'sb4'):
            self.assertIn(n, out)
        self.assertNotIn('old1', out)
        self.assertIn('template:', out)
        self.assertIn('legacy folders: 2 (abvm sandbox rm-legacy removes the stopped, unleased ones; '
                      '_template and _shared stay)', out)

    def test_rm_legacy_keeps_protected_pool_leased_and_running(self):
        for n in ('_template.old', '_shared', 'sb1', 'sb2'):
            self.mkdir(n, 10)
        self.mkdir('gone', 2_000_000_000 // 1000)
        self.mkdir('leased', 10)
        self.mkdir('busy', 10)
        with open(os.path.join(self.host, 'TEMPLATE_SOURCE'), 'w') as f:
            f.write('x')
        abvm.LEASE_KEY = 'sb-leased'
        abvm.lease_op('take', 'bob', 'job', 10)
        abvm.LEASE_KEY = 'vm'
        self.running.add('busy')
        code, out, _ = self.sb('rm-legacy')
        self.assertEqual(code, 0)
        self.assertFalse(os.path.exists(os.path.join(self.host, 'gone')))
        for n in ('_template', '_template.old', '_shared', 'sb1', 'sb2', 'leased', 'busy', 'TEMPLATE_SOURCE'):
            self.assertTrue(os.path.exists(os.path.join(self.host, n)), n)
        self.assertIn('removed gone', out)
        self.assertIn('freed', out)

    def test_rm_legacy_dry_run_removes_nothing(self):
        self.mkdir('gone', 100)
        code, out, _ = self.sb('rm-legacy', '--dry-run')
        self.assertEqual(code, 0)
        self.assertTrue(os.path.isdir(os.path.join(self.host, 'gone')))
        self.assertIn('would remove gone', out)


if __name__ == '__main__':
    unittest.main()
