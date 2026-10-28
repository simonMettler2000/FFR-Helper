"""In-process Qt logic tests; no user's desktop window is controlled."""
import copy
import os
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch

os.environ.setdefault('QT_QPA_PLATFORM', 'offscreen')
from PySide6.QtWidgets import QApplication
from ffr_helper.app import MainWindow
from ffr_helper.editor import SaveEditor
from test_core import synthetic_save


class GuiTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.app = QApplication.instance() or QApplication([])

    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.directory = Path(self.temp.name)
        self.source = self.directory/'GameSaveData10.sav'
        self.raw = synthetic_save()
        self.source.write_bytes(self.raw)
        self.patches = [patch('ffr_helper.app.app_directory', return_value=self.directory/'app'),
                        patch('ffr_helper.storage.app_directory', return_value=self.directory/'app'),
                        patch('ffr_helper.app.discover_saves', return_value=[self.source])]
        for item in self.patches: item.start()
        self.window = MainWindow()
        self.window.open_path(self.source)
        self.assertIsNotNone(self.window.editor)

    def tearDown(self):
        self.window.dirty = False
        self.window.close()
        for item in reversed(self.patches): item.stop()
        self.temp.cleanup()

    def test_set_exp_keeps_the_forecast_reward_out_of_the_saved_total(self):
        self.window.exp_target.setValue(140)
        self.window.exp_reward.setValue(2)
        self.assertIn('2 → 3', self.window.exp_preview.text())
        self.window.apply_exp()
        self.assertEqual(self.window.editor.experience(1001), 140)
        self.assertEqual(self.source.read_bytes(), self.raw)

    def test_mrp_edits_the_selected_pair_without_changing_awakening(self):
        self.window.mrp_target.setValue(44)
        self.window.mrp_reward.setValue(1)
        self.assertIn('1 → 2', self.window.mrp_preview.text())
        self.window.apply_mrp()
        self.assertEqual(self.window.editor.vision_points(13017)[1001], 44)
        self.assertEqual(self.window.editor.root['visionList'].structs()[0]['awakeningPoint'].integer(), 1)

    def test_route_rewards_can_be_applied_as_totals(self):
        w = self.window
        w.route_exp.setValue(100)
        w.route_mrp.setValue(30)
        w.add_route_row()
        for col, value in enumerate(['Example', '2', '25', '5']):
            w.route_table.item(0, col).setText(value)
        self.assertIn('150', w.route_result.text())
        w.apply_route()
        self.assertEqual(w.editor.experience(1001), 150)
        self.assertEqual(w.editor.vision_points(13017)[1001], 40)
        self.assertEqual(self.source.read_bytes(), self.raw)

    def test_lowering_chapter_limit_removes_future_party_names(self):
        w = self.window
        future = copy.deepcopy(w.editor.units()[1001])
        future['UnitId'].set_integer(1008)
        w.editor.root['unitParameterList'].structs().append(future)
        w.chapter.setValue(5)
        self.assertGreaterEqual(w.unit.findText('Sakura'), 0)
        w.chapter.setValue(3)
        self.assertEqual(w.unit.findText('Sakura'), -1)

    def test_checkpoint_restoration_keeps_a_separately_chosen_write_target(self):
        w = self.window
        altered = SaveEditor.open(self.raw)
        altered.set_experience(1001, 1234)
        path = w.store.capture('Practice opening', altered.export(), self.source.name)
        w.load_checkpoint(path)
        self.assertEqual(w.editor.experience(1001), 1234)
        w.set_target()
        self.assertEqual(w.source, self.source)
        self.assertEqual(w.editor.experience(1001), 1234)
        self.assertEqual(self.source.read_bytes(), self.raw)


if __name__ == '__main__':
    unittest.main()
