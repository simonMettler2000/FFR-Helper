import copy
import unittest

from ffr_helper.playtime import validate_playtime_basis


def sample_basis():
    return dict(kind='partial-playtime-basis', restorable=False, source='live-playtime-subsystem',
                coverage='saved-base-and-clock-only', sameRootChecked=True,
                elapsedDefinition='sample-minus-start-minus-completed-suspension', savedSeconds=1000.25,
                startSeconds=9000, suspendedStartSeconds=0, totalSuspendedSeconds=30, sampleSeconds=10000, elapsedSeconds=970)


class PlaytimeTests(unittest.TestCase):
    def test_detached_valid_basis_and_ongoing_suspension(self):
        value = sample_basis()
        copied = validate_playtime_basis(value)
        value['startSeconds'] = 1
        self.assertEqual(copied['startSeconds'], 9000)
        copied['suspendedStartSeconds'] = 9900
        self.assertEqual(validate_playtime_basis(copied)['elapsedSeconds'], 970)

    def test_inconsistent_clocks_are_refused(self):
        for key, changed in [('startSeconds', 0), ('startSeconds', 10001), ('elapsedSeconds', 969),
                             ('totalSuspendedSeconds', 1001), ('suspendedStartSeconds', 8999), ('suspendedStartSeconds', 10001)]:
            value = sample_basis(); value[key] = changed
            with self.subTest(key=key, value=changed), self.assertRaises(ValueError):
                validate_playtime_basis(value)

    def test_numeric_types_and_bounds_are_refused(self):
        for key in ['savedSeconds', 'startSeconds', 'suspendedStartSeconds', 'totalSuspendedSeconds', 'sampleSeconds', 'elapsedSeconds']:
            for changed in [True, None, -1, float('nan'), float('inf'), 10**400, '1000']:
                value = sample_basis(); value[key] = changed
                with self.subTest(key=key, value=changed), self.assertRaises(ValueError):
                    validate_playtime_basis(value)

    def test_schema_and_restore_claims_are_refused(self):
        for key, changed in [('restorable', True), ('sameRootChecked', False), ('coverage', 'complete-timer'),
                             ('elapsedDefinition', 'active-play-duration'), ('address', 1234)]:
            value = sample_basis(); value[key] = changed
            with self.subTest(key=key), self.assertRaises(ValueError):
                validate_playtime_basis(value)
        value = copy.deepcopy(sample_basis()); del value['sampleSeconds']
        with self.assertRaises(ValueError):
            validate_playtime_basis(value)
