import copy
from pathlib import Path
import tempfile
import unittest

from ffr_helper.map_cache import validate_map_unit_cache
from ffr_helper.opening import export_opening,load_opening
from ffr_helper.opening_comparison import compare_openings
from ffr_helper.unit_components import UNIT_FIELDS
from test_playtime_plan import capture_with_clock
from test_opening_comparison import full_capture,normalized_observed


def sample_cache():
    units=[]
    for index in range(2):
        raw=dict.fromkeys(UNIT_FIELDS,0);raw.update(unitId=1001,hpBase=150,mp=-20,temporaryHp=25,baseLevel=12)
        units.append(dict(index=index,raw=raw,accessories=[101],passives=[1,2],continuousEffectCount=2))
    return dict(kind='partial-map-unit-cache',source='map-manager-unit-parameter-storage',coverage='raw-parameter-components-only',
        readOnly=True,restorable=False,coherentWithProgression=False,units=units)


class MapCacheTests(unittest.TestCase):
    def test_optional_cache_roundtrip_preserves_raw_values_and_separate_duplicate_ids(self):
        source=capture_with_clock();source.metadata['mapUnitCache']=sample_cache();before=source.progression
        with tempfile.TemporaryDirectory() as folder:
            path=Path(folder)/'cache.ffropening';export_opening(path,source);loaded=load_opening(path)
            self.assertEqual(loaded.metadata['mapUnitCache'],sample_cache());self.assertEqual(loaded.progression,before)
        self.assertEqual(validate_map_unit_cache(sample_cache())['units'][0]['raw']['mp'],-20)
        empty=sample_cache();empty['units']=[];self.assertEqual(validate_map_unit_cache(empty)['units'],[])
        source.metadata.pop('mapUnitCache');source.validate()

    def test_cache_rejects_completeness_coherence_foreign_fields_and_value_limits(self):
        original=sample_cache()
        for key,value in [('restorable',True),('coherentWithProgression',True),('readOnly',False),('coverage','complete-world'),('nativeAddress',1234)]:
            cache=copy.deepcopy(original);cache[key]=value
            with self.subTest(key=key),self.assertRaises(ValueError):validate_map_unit_cache(cache)
        for key,value in [('index',True),('passives',[1]*129),('accessories',[False]),('continuousEffectCount',1025)]:
            cache=copy.deepcopy(original);cache['units'][0][key]=value
            with self.subTest(key=key),self.assertRaises(ValueError):validate_map_unit_cache(cache)
        cache=copy.deepcopy(original);cache['units'][0]['raw']['hpBase']=True
        with self.assertRaises(ValueError):validate_map_unit_cache(cache)
        base=copy.deepcopy(original['units'][0]);base['accessories']=[];base['passives']=[1]*128
        cache=copy.deepcopy(original);cache['units']=[dict(copy.deepcopy(base),index=i) for i in range(16)]
        validate_map_unit_cache(cache)
        cache['units'].append(dict(copy.deepcopy(base),index=16))
        with self.assertRaisesRegex(ValueError,'aggregate'):validate_map_unit_cache(cache)

    def test_offline_comparison_detects_cache_drift_without_inferring_coherence(self):
        source=full_capture();observed=normalized_observed(source)
        self.assertIsNone(compare_openings(source,observed)['mapUnitCacheEqual'])
        source.metadata['mapUnitCache']=sample_cache();observed.metadata['mapUnitCache']=sample_cache()
        self.assertTrue(compare_openings(source,observed)['mapUnitCacheEqual'])
        observed.metadata['mapUnitCache']['units'][0]['raw']['hpBase']=7
        report=compare_openings(source,observed)
        self.assertFalse(report['mapUnitCacheEqual']);self.assertTrue(report['normalizedProgressionEqual'])
        self.assertFalse(report['restartVerified']);self.assertIs(source.metadata['mapUnitCache']['coherentWithProgression'],False)
