"""In-process Qt logic tests; no user's desktop window is controlled."""
import copy
import json
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
from test_runtime import sample_report
from test_opening import sample_opening


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
        self.assertEqual(self.window.stat_preview.rowCount(), 2)
        self.assertEqual(self.window.stat_preview.columnCount(), 7)
        self.assertNotEqual(self.window.stat_preview.item(0, 0).text(), self.window.stat_preview.item(1, 0).text())

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

    def test_runtime_connection_does_not_attach_or_enable_field_save(self):
        w = self.window
        target = self.directory/'FFRHelperRuntime'
        (target/'connection').mkdir(parents=True)
        w.attach_runtime(target)
        self.assertIsNone(w.bridge)
        self.assertFalse(w.save_anywhere.isEnabled())
        self.assertFalse(w.save_anywhere.isChecked())
        self.assertFalse(w.restart_battle.isEnabled())
        self.assertTrue(w.runtime_disconnect.isEnabled())
        self.assertFalse(w.runtime_capture.isEnabled())
        w.disconnect_runtime()
        self.assertIsNone(w.recorder)
        self.assertEqual((target/'connection/observe.txt').read_text().splitlines()[2], '0')

    def test_runtime_report_does_not_change_working_save_or_show_enemy_ids(self):
        w = self.window
        target = self.directory/'FFRHelperRuntime'
        (target/'connection').mkdir(parents=True)
        original = w.editor.export()
        w.attach_runtime(target)
        w.capture_runtime()
        report = sample_report(w.recorder)
        report['events'] = [dict(kind='encounter-opening', entry=dict(enemiesUnitIdList=[987654]))]
        (target/'connection/runtime-report.json').write_text(json.dumps(report))
        status = {key: report[key] for key in ['protocol', 'adapter', 'session', 'time', 'readOnly']}
        status.update(hooks={}, message='Read-only connection')
        (target/'connection/observer-status.json').write_text(json.dumps(status))
        w.poll_recorder()
        self.assertTrue(w.runtime_export.isEnabled())
        self.assertTrue(w.runtime_capture.isEnabled())
        self.assertIn('not a save state', w.runtime_status.text())
        self.assertNotIn('987654', w.runtime_status.text())
        self.assertEqual(w.editor.export(), original)
        self.assertEqual(self.source.read_bytes(), self.raw)
        w.disconnect_runtime()
        self.assertFalse(w.runtime_export.isEnabled())

    def test_native_opening_enables_only_diagnostic_export(self):
        w=self.window
        target=self.directory/'FFRHelperRuntimeNative';connection=target/'connection';connection.mkdir(parents=True)
        original=w.editor.export();w.attach_runtime(target);w.capture_runtime()
        capture=sample_opening();report=sample_report(w.recorder);report['adapter']=w.recorder.adapter_name
        report['objects']={'nativeEntry':{'opening':capture.metadata}}
        status={key:report[key] for key in ['protocol','adapter','session','time','readOnly']}
        status.update(hooks={},message='Native observation')
        (connection/'observer-status.json').write_text(json.dumps(status))
        (connection/'runtime-report.json').write_text(json.dumps(report))
        (connection/'opening-progression.gvas').write_bytes(capture.progression)
        w.poll_recorder()
        self.assertTrue(w.runtime_opening_export.isEnabled())
        self.assertFalse(w.restart_battle.isEnabled())
        self.assertFalse(w.save_anywhere.isEnabled())
        self.assertNotIn('987654',w.runtime_status.text())
        self.assertIn('not a save state',w.runtime_status.text())
        self.assertEqual(w.editor.export(),original)
        (connection/'opening-progression.gvas').write_bytes(b'changed')
        w.poll_recorder();self.assertFalse(w.runtime_opening_export.isEnabled())
        self.assertIsNone(w.runtime_opening)
        w.disconnect_runtime();self.assertFalse(w.runtime_opening_export.isEnabled())


if __name__ == '__main__':
    unittest.main()
