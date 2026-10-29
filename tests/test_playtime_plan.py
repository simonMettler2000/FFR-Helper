import copy
import hashlib
import json
from pathlib import Path
import struct
import tempfile
import unittest

from ffr_helper.codec import SaveDocument, TypeNode
from ffr_helper.opening import OpeningCapture
from ffr_helper.playtime_plan import export_playtime_plan, nearest_float32, plan_opening_playtime
from test_core import prop
from test_opening import sample_opening
from test_playtime import sample_basis


def capture_with_clock(saved=1000.25, elapsed=970):
    capture = sample_opening()
    document = SaveDocument.decode(capture.progression)
    packed = struct.pack('<f', saved)
    saved = struct.unpack('<f', packed)[0]
    document.root['player'].struct().properties.append(prop('PlayTime', TypeNode('FloatProperty'), packed))
    raw = document.prefix + document.root.encode() + document.suffix
    basis = dict(sample_basis(), savedSeconds=saved, startSeconds=1000, suspendedStartSeconds=0,
                 sampleSeconds=1000 + elapsed, totalSuspendedSeconds=0, elapsedSeconds=elapsed, roundingMode='nearest-even')
    return OpeningCapture(dict(capture.metadata, byteLength=len(raw), sha256=hashlib.sha256(raw).hexdigest(), playtimeBasis=basis), raw)


class PlaytimePlanTests(unittest.TestCase):
    def test_preview_changes_only_known_float_and_preserves_original_capture(self):
        capture = capture_with_clock()
        before_metadata = copy.deepcopy(capture.metadata)
        plan = plan_opening_playtime(capture)
        self.assertEqual(plan.normalized_saved_seconds, 1970.25)
        self.assertEqual(capture.metadata, before_metadata)
        self.assertEqual(capture.metadata['sha256'], hashlib.sha256(capture.progression).hexdigest())
        target = SaveDocument.decode(plan.normalized_progression)
        source = SaveDocument.decode(capture.progression)
        self.assertEqual(target.root['UnknownFutureField'].encode(), source.root['UnknownFutureField'].encode())
        self.assertEqual(struct.unpack('<f', target.root['player'].struct()['PlayTime'].raw)[0], 1970.25)
        target.root['player'].struct()['PlayTime'].raw = source.root['player'].struct()['PlayTime'].raw
        self.assertEqual(target.prefix + target.root.encode() + target.suffix, capture.progression)

    def test_int_conversion_and_add_are_distinct_nearest_even_stages(self):
        for saved, elapsed, rounded_elapsed, expected in [(16777216, 1, 1, 16777216), (16777218, 1, 1, 16777220),
                                                         (1, 16777217, 16777216, 16777216), (1, 16777219, 16777220, 16777220)]:
            plan = plan_opening_playtime(capture_with_clock(saved, elapsed))
            self.assertEqual((plan.rounded_elapsed_seconds, plan.normalized_saved_seconds), (rounded_elapsed, expected))

    def test_unknown_or_unsupported_rounding_is_refused(self):
        for mode in [None, 'downward', 'upward', 'toward-zero']:
            capture = capture_with_clock()
            if mode is None:
                del capture.metadata['playtimeBasis']['roundingMode']
            else:
                capture.metadata['playtimeBasis']['roundingMode'] = mode
            capture.validate()  # Still a valid diagnostic capture.
            with self.subTest(mode=mode), self.assertRaisesRegex(ValueError, 'nearest-even'):
                plan_opening_playtime(capture)

    def test_ongoing_suspension_is_refused_without_changing_the_source(self):
        capture = capture_with_clock(elapsed=100)
        capture.metadata['playtimeBasis']['suspendedStartSeconds'] = 1050
        with self.assertRaisesRegex(ValueError, 'ongoing suspension'):
            plan_opening_playtime(capture)
        capture.validate()

    def test_subnormal_and_overflowing_plans_are_refused(self):
        for capture in [capture_with_clock(1e-40, 0), capture_with_clock(600000000000, 500000000000)]:
            with self.assertRaises(ValueError):
                plan_opening_playtime(capture)
        self.assertEqual(nearest_float32(0.0), 0.0)
        for value in [-1, float('inf'), float('nan'), 2e12]:
            with self.assertRaises(ValueError):
                nearest_float32(value)

    def test_export_is_only_a_nonrestorable_receipt_and_never_a_save(self):
        plan = plan_opening_playtime(capture_with_clock())
        with tempfile.TemporaryDirectory() as folder:
            path = Path(folder)/'plan.ffrplaytime.json'
            export_playtime_plan(path, plan)
            receipt = json.loads(path.read_text(encoding='utf8'))
            self.assertIs(receipt['restorable'], False)
            self.assertEqual(receipt['targetSha256'], hashlib.sha256(plan.normalized_progression).hexdigest())
            self.assertNotIn('GVAS', path.read_text(encoding='utf8'))
            for suffix in ['.sav', '.ffropening', '.ffrsnapshot']:
                with self.assertRaises(ValueError):
                    export_playtime_plan(Path(folder)/('invalid'+suffix), plan)
            self.assertEqual(len(list(Path(folder).iterdir())), 1)
