import copy
from pathlib import Path
import tempfile
import unittest
from ffr_helper.engine_rng import validate_engine_global_rng
from ffr_helper.opening import export_opening,load_opening,summarize_opening
from ffr_helper.opening_comparison import compare_openings
from test_opening_comparison import full_capture,normalized_observed


def sample_engine_rng():
    return dict(kind='partial-engine-global-rng-state',readOnly=True,restorable=False,coverage='single-profiled-global-seed-only',
        otherSourcesCovered=False,atomicSnapshot=False,matchingSamplesAroundProgressionCopy=True,seed=0xffffffff)


class EngineRngTests(unittest.TestCase):
    def test_roundtrip_comparison_and_summary_do_not_claim_complete_or_atomic_rng(self):
        source=full_capture();observed=normalized_observed(source)
        self.assertIsNone(compare_openings(source,observed)['engineGlobalRngStateEqual'])
        source.metadata['engineGlobalRngState']=sample_engine_rng();observed.metadata['engineGlobalRngState']=sample_engine_rng()
        original=source.progression;metadata=copy.deepcopy(source.metadata)
        with tempfile.TemporaryDirectory() as folder:
            path=Path(folder)/'global.ffropening';export_opening(path,source);loaded=load_opening(path)
            self.assertEqual(loaded.metadata['engineGlobalRngState'],sample_engine_rng())
            summary=summarize_opening(loaded);self.assertIn('concurrent draws unverified',summary);self.assertNotIn('4294967295',summary)
        self.assertTrue(compare_openings(source,observed)['engineGlobalRngStateEqual'])
        observed.metadata['engineGlobalRngState']['seed']=0
        report=compare_openings(source,observed)
        self.assertFalse(report['engineGlobalRngStateEqual']);self.assertFalse(report['restartVerified'])
        self.assertEqual(source.progression,original);self.assertEqual(source.metadata,metadata)

    def test_unsupported_bounds_addresses_and_concurrency_claims_are_rejected(self):
        for key,value in [('seed',True),('seed',-1),('seed',2**32),('atomicSnapshot',True),('otherSourcesCovered',True),
            ('readOnly',False),('restorable',True),('matchingSamplesAroundProgressionCopy',False),('seedAddress',1234)]:
            bad=dict(sample_engine_rng());bad[key]=value
            with self.subTest(key=key),self.assertRaises(ValueError):validate_engine_global_rng(bad)
        for seed in [0,1,0xffffffff]:self.assertEqual(validate_engine_global_rng(dict(sample_engine_rng(),seed=seed))['seed'],seed)
