import copy
from pathlib import Path
import tempfile
import unittest
from ffr_helper.crt_rng import SUPPORTED_CRT_SHA256,validate_crt_rand_state
from ffr_helper.opening import export_opening,load_opening
from test_playtime_plan import capture_with_clock
from test_opening_comparison import full_capture,normalized_observed
from ffr_helper.opening_comparison import compare_openings


def sample_rng():
    return dict(kind='partial-crt-rand-state',readOnly=True,restorable=False,coverage='current-game-thread-crt-rand-only',
        otherSourcesCovered=False,stableAcrossProgressionCopy=True,moduleSha256=SUPPORTED_CRT_SHA256,stateIndex=0,seed=0xffffffff)


class CrtRngTests(unittest.TestCase):
    def test_optional_unsigned_seed_roundtrip_preserves_original_capture(self):
        source=capture_with_clock();original=source.progression;source.metadata['crtRandState']=sample_rng()
        with tempfile.TemporaryDirectory() as folder:
            path=Path(folder)/'seed.ffropening';export_opening(path,source);loaded=load_opening(path)
            self.assertEqual(loaded.metadata['crtRandState'],sample_rng());self.assertEqual(loaded.progression,original)
        source.metadata.pop('crtRandState');source.validate()
        for seed in [0,1,0xffffffff]:self.assertEqual(validate_crt_rand_state(dict(sample_rng(),seed=seed))['seed'],seed)

    def test_wrong_module_bounds_and_complete_rng_claims_are_rejected(self):
        original=sample_rng()
        for key,value in [('restorable',True),('otherSourcesCovered',True),('stableAcrossProgressionCopy',False),
            ('moduleSha256','0'*64),('stateIndex',2),('stateIndex',True),('seed',True),('seed',-1),('seed',2**32),('seedAddress',1234)]:
            state=copy.deepcopy(original);state[key]=value
            with self.subTest(key=key),self.assertRaises(ValueError):validate_crt_rand_state(state)

    def test_component_comparison_never_infers_complete_rng_or_restoration(self):
        source=full_capture();observed=normalized_observed(source)
        self.assertIsNone(compare_openings(source,observed)['crtRandStateEqual'])
        source.metadata['crtRandState']=sample_rng();observed.metadata['crtRandState']=sample_rng()
        self.assertTrue(compare_openings(source,observed)['crtRandStateEqual'])
        observed.metadata['crtRandState']['seed']=0
        report=compare_openings(source,observed)
        self.assertFalse(report['crtRandStateEqual']);self.assertTrue(report['normalizedProgressionEqual']);self.assertFalse(report['restartVerified'])
