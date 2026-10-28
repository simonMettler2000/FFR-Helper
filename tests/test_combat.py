import copy
import json
from pathlib import Path
import tempfile
import time
import unittest
from unittest.mock import patch

from ffr_helper.combat import (CombatObservationBridge, MISSING, RAW_FIELDS, export_combat, load_combat,
                               validate_combat_report, validate_observation)
from ffr_helper.opening import GAME_SHA256
import test_abort_trial


def sample_observation():
    units = []
    for index in range(2):
        raw = dict.fromkeys(RAW_FIELDS, 0)
        raw.update(unitId=1001, duplicationId=index, hpBase=150, temporaryHp=25, mp=-20, stagger=500,
                   maxStagger=100, actionCount=2)
        units.append(dict(index=index, raw=raw, accessories=[101], passives=[303], continuousEffectCount=2, ai=None))
    units[1]['ai'] = dict(skillId=3001, scheduledSkillId=3002, statusChangedSkillId=-1,
                          targets=[0], stack=[dict(skillId=3003, targets=[1, -1])])
    return dict(kind='partial-combat-observation', restorable=False, readOnly=True,
                referenceEncoding='capture-local-unit-index; -1 is null', battleTurn=3, selectedUnit=0,
                targetUnits=[1], additionalTimeline=[1, 0], units=units, missingState=MISSING.copy())


def sample_status(session):
    return dict(protocol=1, version=1, adapter='ffr-helper-native-combat-observer', format='ffr-helper-combat-report',
                kind='diagnostic-observation', readOnly=True, restorable=False, gameSha256=GAME_SHA256,
                session=session, time=int(time.time()), available=True, request='', sequence=0, openingHash='',
                captureNumber=0, captureTime=0, phase='idle', message='Fixture', observation=None)


class CombatTransportTests(unittest.TestCase):
    def setUp(self):
        test_abort_trial.AbortClientTests.setUp(self)
        self.frame['guard']['friendlyActions'] = 2
        self.write_frame()
        self.bridge = CombatObservationBridge(self.recorder)
        self.combat = sample_status(self.recorder.session)
        self.write_combat()

    def tearDown(self):
        self.temp.cleanup()

    def write_frame(self):
        (self.root/'connection/abort-status.json').write_text(json.dumps(self.frame), encoding='utf8')

    def write_combat(self):
        (self.root/'connection/combat-report.json').write_text(json.dumps(self.combat), encoding='utf8')

    def receive_frame(self):
        self.bridge.send(self.report)
        request = self.bridge.request
        self.combat.update(request=request['nonce'], sequence=request['sequence'], openingHash=request['sha256'],
                           captureNumber=1, captureTime=int(time.time()), phase='captured', observation=sample_observation())
        self.write_combat()
        return self.bridge.receipt()

    def test_single_read_request_after_prior_actions_never_sends_abort_or_import(self):
        report = self.receive_frame()
        self.assertIsNotNone(report)
        lines = (self.root/'connection/combat-request.txt').read_text().splitlines()
        self.assertEqual(lines[0], 'FFR_HELPER_COMBAT_OBSERVATION_V1')
        self.assertEqual(lines[5], self.capture_metadata_hash())
        self.assertEqual(len(lines), 6)
        self.assertFalse((self.root/'connection/abort-request.txt').exists())
        with self.assertRaisesRegex(ValueError, 'already requested'):
            self.bridge.send(self.report)

    def capture_metadata_hash(self):
        return self.report['objects']['nativeEntry']['opening']['sha256']

    def test_receipt_binds_nonce_sequence_hash_and_actual_capture_time(self):
        self.receive_frame()
        original = copy.deepcopy(self.combat)
        for key, value in [('request', 'd'*32), ('sequence', 2), ('openingHash', 'e'*64),
                           ('captureTime', int(time.time())-30)]:
            self.combat = copy.deepcopy(original)
            self.combat[key] = value
            self.write_combat()
            self.assertIsNone(self.bridge.receipt())

    def test_stale_foreign_or_misleading_channel_is_not_actionable(self):
        original = copy.deepcopy(self.combat)
        for key, value in [('time', int(time.time())-30), ('session', 'f'*32), ('restorable', True),
                           ('readOnly', False), ('gameSha256', '0'*64), ('captureNumber', False)]:
            self.combat = copy.deepcopy(original)
            self.combat[key] = value
            self.write_combat()
            self.assertIsNone(self.bridge.status())
        self.assertFalse((self.root/'connection/combat-request.txt').exists())

    def test_queued_work_changed_context_or_practice_action_blocks_capture(self):
        original = copy.deepcopy(self.frame)
        for key, value in [('workingJobs', 1), ('matchingWorld', False), ('commandMenu', False), ('friendlyActions', -1)]:
            self.frame = copy.deepcopy(original)
            self.frame['guard'][key] = value
            self.write_frame()
            with self.assertRaises(ValueError):
                self.bridge.send(self.report)
            self.assertIsNone(self.bridge.request)
        self.frame = copy.deepcopy(original)
        self.frame['phase'] = 'armed'
        self.write_frame()
        with self.assertRaises(ValueError):
            self.bridge.send(self.report)

    def test_uncertain_request_write_is_not_retried(self):
        with patch('ffr_helper.storage.atomic_write', side_effect=OSError('uncertain write')):
            with self.assertRaises(OSError):
                self.bridge.send(self.report)
        with self.assertRaisesRegex(ValueError, 'already requested'):
            self.bridge.send(self.report)

    def test_archive_retains_raw_components_duplicate_ids_and_reference_indices(self):
        report = self.receive_frame()
        with tempfile.TemporaryDirectory() as folder:
            path = Path(folder)/'command.ffrcombat.json'
            export_combat(path, report)
            loaded = load_combat(path)
            self.assertEqual(loaded, report)
            a, b = loaded['observation']['units']
            self.assertEqual(a['raw']['unitId'], b['raw']['unitId'])
            self.assertEqual(a['raw']['mp'], -20)
            self.assertEqual(a['raw']['stagger'], 500)
            self.assertEqual(b['ai']['stack'][0]['targets'], [1, -1])
            with self.assertRaises(ValueError):
                export_combat(Path(folder)/'state.sav', report)
            with self.assertRaises(ValueError):
                export_combat(Path(folder)/'state.ffrsnapshot', report)

    def test_invalid_references_raw_values_or_complete_state_claims_are_rejected(self):
        original = sample_observation()
        variants = []
        for key, value in [('restorable', True), ('referenceEncoding', 'native-pointers'), ('missingState', []),
                           ('selectedUnit', 2), ('targetUnits', [2]), ('targetUnits', [True])]:
            value_copy = copy.deepcopy(original)
            value_copy[key] = value
            variants.append(value_copy)
        for key, value in [('mp', True), ('stagger', 2**31), ('hpBase', float('nan'))]:
            value_copy = copy.deepcopy(original)
            value_copy['units'][0]['raw'][key] = value
            variants.append(value_copy)
        value_copy = copy.deepcopy(original)
        value_copy['units'][0]['nativeAddress'] = 123456
        variants.append(value_copy)
        for value in variants:
            with self.assertRaises(ValueError):
                validate_observation(value)

    def test_aggregate_stack_and_reference_budgets_are_enforced(self):
        data = sample_observation()
        data['units'][1]['ai']['stack'] = [dict(skillId=1, targets=[]) for _ in range(257)]
        with self.assertRaisesRegex(ValueError, 'stack budget'):
            validate_observation(data)
        data['units'][1]['ai']['stack'] = [dict(skillId=1, targets=[0]*64) for _ in range(65)]
        with self.assertRaisesRegex(ValueError, 'reference budget'):
            validate_observation(data)

    def test_failure_receipt_has_no_accepted_state_and_cannot_be_exported(self):
        self.receive_frame()
        self.combat.update(phase='failed', observation=None)
        self.write_combat()
        report = self.bridge.receipt()
        self.assertEqual(report['phase'], 'failed')
        with self.assertRaises(ValueError):
            export_combat(self.root/'bad.ffrcombat.json', report)
        self.combat['observation'] = sample_observation()
        with self.assertRaises(ValueError):
            validate_combat_report(self.combat)
