import unittest
from ffr_helper.decision_history import validate_decision_history


def sample():
    return dict(kind='decision-handler-entry-history',readOnly=True,restorable=False,
        coverageReviewed=False,commandBoundaryVerified=False,gameThreadFreshnessVerified=False,
        sequence=1,trackedFromNativeEntry=True,handlerEntries=0,foreignThreadObserved=False,overflow=False)


class DecisionHistoryTests(unittest.TestCase):
    def test_zero_count_is_only_observation_and_null_stays_unavailable(self):
        self.assertIsNone(validate_decision_history(None))
        source=sample();result=validate_decision_history(source)
        self.assertEqual(result,source);self.assertIsNot(result,source)
        source['handlerEntries']=5
        self.assertEqual(result['handlerEntries'],0)
        self.assertFalse(result['commandBoundaryVerified'])

    def test_coverage_and_restore_claims_are_rejected(self):
        for key in ['restorable','coverageReviewed','commandBoundaryVerified','gameThreadFreshnessVerified']:
            with self.subTest(key=key),self.assertRaises(ValueError):
                validate_decision_history(dict(sample(),**{key:True}))
        with self.assertRaises(ValueError):validate_decision_history(dict(sample(),safeToRetry=True))

    def test_strict_counts_and_flags(self):
        for key,value in [('sequence',True),('sequence',0),('handlerEntries',False),
                          ('handlerEntries',-1),('handlerEntries',1000001),('overflow',1),
                          ('foreignThreadObserved',0),('trackedFromNativeEntry',False)]:
            with self.subTest(key=key,value=value),self.assertRaises(ValueError):
                validate_decision_history(dict(sample(),**{key:value}))

    def test_partial_or_overflowed_history_never_becomes_authority(self):
        data=dict(sample(),handlerEntries=1000000,overflow=True,foreignThreadObserved=True)
        self.assertEqual(validate_decision_history(data),data)
        with self.assertRaises(ValueError):validate_decision_history(dict(data,handlerEntries=3))
