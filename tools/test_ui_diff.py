import os
import sys
import tempfile
import unittest

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import ui_diff  # noqa: E402

try:
    from PIL import Image
except ImportError:  # pragma: no cover
    Image = None


def make(path, color=(10, 20, 30), pixels=None, size=(40, 30)):
    img = Image.new('RGB', size, color)
    for xy, c in (pixels or {}).items():
        img.putpixel(xy, c)
    img.save(path)


@unittest.skipIf(Image is None, 'Pillow missing')
class UiDiffTest(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.old = os.path.join(self.tmp.name, 'old')
        self.new = os.path.join(self.tmp.name, 'new')
        os.makedirs(self.old)
        os.makedirs(self.new)

    def tearDown(self):
        self.tmp.cleanup()

    def both(self, name, **kw_new):
        make(os.path.join(self.old, name))
        make(os.path.join(self.new, name), **kw_new)

    def test_same(self):
        self.both('01-options.png')
        r = ui_diff.diff_tours(self.old, self.new)
        self.assertEqual(r, {'changed': [], 'unchanged': 1})
        self.assertFalse(os.path.exists(os.path.join(self.new, 'diff', 'options.png')))
        self.assertIn('1 of 1 screens unchanged', open(os.path.join(self.new, 'changed.html')).read())

    def test_antialiasing_tolerated(self):
        self.both('01-options.png', color=(15, 25, 35))
        self.assertEqual(ui_diff.diff_tours(self.old, self.new)['changed'], [])

    def test_one_pixel(self):
        self.both('01-options.png', pixels={(3, 4): (255, 255, 255)})
        r = ui_diff.diff_tours(self.old, self.new)
        self.assertEqual(r, {'changed': ['options'], 'unchanged': 0})
        self.assertTrue(os.path.exists(os.path.join(self.new, 'diff', 'options.png')))
        self.assertEqual(Image.open(os.path.join(self.new, 'diff', 'options.png')).size, (40 * 3 + 8, 30))
        html = open(os.path.join(self.new, 'changed.html')).read()
        self.assertIn('options', html)
        self.assertIn('0 of 1 screens unchanged', html)

    def test_big_change(self):
        self.both('01-options.png', color=(200, 0, 0))
        _, percent, _ = ui_diff.compare(os.path.join(self.old, '01-options.png'),
                                        os.path.join(self.new, '01-options.png'), 'options')
        self.assertEqual(percent, 100.0)

    def test_missing_files(self):
        make(os.path.join(self.old, '01-gone.png'))
        make(os.path.join(self.new, '01-fresh.png'))
        make(os.path.join(self.old, '02-same.png'))
        make(os.path.join(self.new, '03-same.png'))  # renumbered: still the same screen
        r = ui_diff.diff_tours(self.old, self.new)
        self.assertEqual(sorted(r['changed']), ['fresh', 'gone'])
        self.assertEqual(r['unchanged'], 1)

    def test_size_differs(self):
        make(os.path.join(self.old, '01-a.png'))
        make(os.path.join(self.new, '01-a.png'), size=(41, 30))
        self.assertEqual(ui_diff.diff_tours(self.old, self.new)['changed'], ['a'])

    def test_masks(self):
        make(os.path.join(self.old, '01-surprise-game.png'))
        make(os.path.join(self.new, '01-surprise-game.png'), color=(200, 0, 0))
        self.assertEqual(ui_diff.diff_tours(self.old, self.new)['changed'], [])
        old = dict(ui_diff.MASKS)
        try:
            ui_diff.MASKS['clock'] = {'rects': [(0, 0, 0.5, 0.5)]}
            make(os.path.join(self.old, '02-clock.png'))
            make(os.path.join(self.new, '02-clock.png'), pixels={(2, 2): (255, 255, 255)})
            self.assertEqual(ui_diff.diff_tours(self.old, self.new)['changed'], [])
            make(os.path.join(self.new, '02-clock.png'), pixels={(30, 25): (255, 255, 255)})
            self.assertEqual(ui_diff.diff_tours(self.old, self.new)['changed'], ['clock'])
        finally:
            ui_diff.MASKS.clear()
            ui_diff.MASKS.update(old)

    def test_tour_diff_without_value(self):
        import contextlib
        import io
        import ui_tour
        for argv in (['ui_tour.py', self.new, '--diff'], ['ui_tour.py', self.new, '--diff', '--show']):
            buf = io.StringIO()
            with contextlib.redirect_stdout(buf):
                self.assertEqual(ui_tour.main(argv), 2)  # before the launcher is started
            self.assertIn('--diff PREV_DIR', buf.getvalue())


if __name__ == '__main__':
    unittest.main()
