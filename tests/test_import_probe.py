import copy
import json
import unittest
from unittest.mock import patch

from ffr_helper.import_probe import ImportProbeBridge
import test_abort_trial


class ImportProbeTests(unittest.TestCase):
    def setUp(self):
        test_abort_trial.AbortClientTests.setUp(self)
        self.bridge = ImportProbeBridge(self.recorder)
        self.frame['importProbe'] = dict(available=True, loaderReady=True, phase='idle', nonce='', session='',
                                         boundaryPolicy='ordinary-idle-menu',
                                         sequence=0, openingHash='', passed=False, cloneMatches=False,
                                         liveUnchanged=False, inputUnchanged=False, pinsReleased=True,
                                         byteLength=0, message='Fixture', restorable=False)
        self.write_frame()

    def tearDown(self):
        self.temp.cleanup()

    def write_frame(self):
        (self.root/'connection/abort-status.json').write_text(json.dumps(self.frame), encoding='utf8')

    def passed_receipt(self):
        self.bridge.send(self.report)
        request = self.bridge.request
        self.frame['importProbe'].update(phase='passed', nonce=request['nonce'], session=self.recorder.session,
                                         sequence=request['sequence'], openingHash=request['sha256'], passed=True,
                                         cloneMatches=True, liveUnchanged=True, inputUnchanged=True, pinsReleased=True,
                                         byteLength=len(self.capture.progression))
        self.write_frame()

    def test_explicit_single_import_command_and_bound_receipt(self):
        self.assertIsNone(self.bridge.receipt())
        self.passed_receipt()
        lines = (self.root/'connection/abort-request.txt').read_text().splitlines()
        self.assertEqual(lines[0], 'FFR_HELPER_ABORT_OPENING_V1')
        self.assertEqual(lines[-1], 'probe-import')
        self.assertEqual(lines[5], self.capture.metadata['sha256'])
        self.assertTrue(self.bridge.receipt()['passed'])
        with self.assertRaisesRegex(ValueError, 'already requested'):
            self.bridge.send(self.report)

    def test_inactive_uninitialized_or_unsafe_context_sends_nothing(self):
        original = copy.deepcopy(self.frame)
        for key, value in [('available', False), ('loaderReady', False), ('phase', 'running')]:
            self.frame = copy.deepcopy(original)
            self.frame['importProbe'][key] = value
            self.write_frame()
            with self.assertRaises(ValueError):
                self.bridge.send(self.report)
            self.assertIsNone(self.bridge.request)
        self.frame = copy.deepcopy(original)
        self.frame['guard']['workingJobs'] = 1
        self.write_frame()
        with self.assertRaises(ValueError):
            self.bridge.send(self.report)
        self.assertFalse((self.root/'connection/abort-request.txt').exists())

    def test_turn_beginnings_do_not_block_detached_import_or_enable_abort(self):
        self.frame['guard']['friendlyActions']=3
        self.frame['guard']['openingHistoryUntouched']=False
        self.write_frame();self.bridge.send(self.report)
        self.assertEqual((self.root/'connection/abort-request.txt').read_text().splitlines()[-1],'probe-import')
        with self.assertRaises(ValueError):test_abort_trial.AbortTrialBridge(self.recorder).prepare(self.report)
        self.assertEqual((self.root/'connection/abort-request.txt').read_text().splitlines()[-1],'probe-import')

    def test_abort_and_import_probes_cannot_be_combined(self):
        self.frame['phase'] = 'armed'
        self.write_frame()
        with self.assertRaises(ValueError):
            self.bridge.send(self.report)
        self.frame['phase'] = 'idle'
        self.frame['importProbe']['phase'] = 'passed'
        self.write_frame()
        abort = test_abort_trial.AbortTrialBridge(self.recorder)
        with self.assertRaises(ValueError):
            abort.prepare(self.report)

    def test_malformed_or_misleading_probe_status_is_rejected(self):
        original = copy.deepcopy(self.frame)
        for key, value in [('restorable', True), ('passed', 1), ('pinsReleased', None),
                           ('phase', 'restored'), ('byteLength', 16*1024*1024+1),('boundaryPolicy','opening-untouched')]:
            self.frame = copy.deepcopy(original)
            self.frame['importProbe'][key] = value
            self.write_frame()
            self.assertIsNone(self.bridge.status())

    def test_other_nonce_revision_session_or_capture_is_not_our_receipt(self):
        self.passed_receipt()
        original = copy.deepcopy(self.frame)
        for key, value in [('nonce', 'c'*32), ('sequence', 2), ('session', 'd'*32), ('openingHash', 'e'*64)]:
            self.frame = copy.deepcopy(original)
            self.frame['importProbe'][key] = value
            self.write_frame()
            self.assertIsNone(self.bridge.receipt())

    def test_success_requires_every_comparison_and_pin_release(self):
        self.passed_receipt()
        original = copy.deepcopy(self.frame)
        for key in ['cloneMatches', 'liveUnchanged', 'inputUnchanged', 'pinsReleased']:
            self.frame = copy.deepcopy(original)
            self.frame['importProbe'][key] = False
            self.write_frame()
            with self.assertRaisesRegex(ValueError, 'inconsistent'):
                self.bridge.receipt()

    def test_uncertain_write_is_never_automatically_resent(self):
        with patch('ffr_helper.abort_trial.atomic_write', side_effect=OSError('uncertain command write')):
            with self.assertRaises(OSError):
                self.bridge.send(self.report)
        self.assertIsNotNone(self.bridge.request)
        with self.assertRaisesRegex(ValueError, 'already requested'):
            self.bridge.send(self.report)
