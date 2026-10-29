import copy
import unittest
from ffr_helper.opening import summarize_opening
from test_crt_rng import sample_rng
from test_map_cache import sample_cache
from test_playtime_plan import capture_with_clock


class OpeningSummaryTests(unittest.TestCase):
    def test_optional_presence_is_partial_and_does_not_expose_save_values(self):
        capture=capture_with_clock();capture.metadata.update(mapUnitCache=sample_cache(),crtRandState=sample_rng(),
            fieldAnchor=dict(kind='partial-field-anchor',restorable=False,source='live-player-root-transform',coverage='position-and-direction-only',
                sameWorldChecked=True,mapId=7654321,position=[111.25,222.5,333.75],direction=2))
        before=copy.deepcopy(capture.metadata);raw=capture.progression
        summary=summarize_opening(capture)
        self.assertIn('rounding: nearest-even',summary);self.assertIn('2 entries; progression coherence unverified',summary)
        self.assertIn('other RNG missing',summary);self.assertIn('not a save state',summary)
        for hidden in ['7654321','111.25','987654','4294967295',capture.metadata['sha256']]:self.assertNotIn(hidden,summary)
        self.assertEqual(capture.metadata,before);self.assertEqual(capture.progression,raw)
        capture.metadata['crtRandState']['otherSourcesCovered']=True
        with self.assertRaises(ValueError):summarize_opening(capture)
