import unittest
from ffr_helper.skill_history import validate_skill_history


def sample():
    return dict(kind='skill-begin-event-history',readOnly=True,restorable=False,coverageReviewed=False,
        trackedFromNativeEntry=True,sequence=2,skillBeginEvents=0,foreignThreadObserved=False,overflow=False)


class SkillHistoryTests(unittest.TestCase):
    def test_unavailable_stays_null_and_known_counts_stay_partial(self):
        self.assertIsNone(validate_skill_history(None))
        value=sample();parsed=validate_skill_history(value);value['skillBeginEvents']=9
        self.assertEqual(parsed['skillBeginEvents'],0);self.assertFalse(parsed['coverageReviewed'])

    def test_rejects_restoration_review_and_extra_authority_claims(self):
        for key in ['coverageReviewed','restorable']:
            with self.assertRaises(ValueError):validate_skill_history(dict(sample(),**{key:True}))
        with self.assertRaises(ValueError):validate_skill_history(dict(sample(),safeToRetry=True))

    def test_counts_flags_and_overflow_are_strict(self):
        for key,value in [('sequence',True),('sequence',0),('skillBeginEvents',False),('skillBeginEvents',-1),
                          ('skillBeginEvents',1000001),('foreignThreadObserved',1),('overflow',True)]:
            with self.subTest(key=key,value=value),self.assertRaises(ValueError):validate_skill_history(dict(sample(),**{key:value}))

    def test_foreign_thread_and_overflow_remain_observational(self):
        value=dict(sample(),skillBeginEvents=1000000,overflow=True,foreignThreadObserved=True)
        self.assertEqual(validate_skill_history(value),value)
