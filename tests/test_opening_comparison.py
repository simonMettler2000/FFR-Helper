import copy
import hashlib
import json
from pathlib import Path
import tempfile
import unittest

from ffr_helper.codec import SaveDocument
from ffr_helper.opening import OpeningCapture
from ffr_helper.opening_comparison import ARRAYS,FLAGS,INTEGERS,compare_openings,export_opening_comparison,validate_complete_entry
from ffr_helper.playtime_plan import plan_opening_playtime
from test_playtime_plan import capture_with_clock


def full_capture():
    source=capture_with_clock()
    source.metadata['entry']=dict({k:[] for k in ARRAYS},**{k:False for k in FLAGS},**{k:-1 for k in INTEGERS},
        addPassiveDataList=[[101,202],[]],bgmCueName='Fixture 🎵',preemptiveRelation=0,probabilityOfSuccessfulEscape=50)
    source.metadata['entry'].update(friendliesUnitIdList=[1001,1002],enemiesUnitIdList=[11,11],canEscape=True)
    source.metadata['fieldAnchor']=dict(kind='partial-field-anchor',restorable=False,source='live-player-root-transform',
        coverage='position-and-direction-only',sameWorldChecked=True,mapId=1000,position=[1.0,2.0,3.0],direction=2)
    return source


def normalized_observed(source):
    plan=plan_opening_playtime(source);metadata=copy.deepcopy(source.metadata)
    metadata.update(sequence=2,sha256=hashlib.sha256(plan.normalized_progression).hexdigest(),byteLength=len(plan.normalized_progression))
    metadata['playtimeBasis'].update(savedSeconds=plan.normalized_saved_seconds,elapsedSeconds=0,
        sampleSeconds=metadata['playtimeBasis']['startSeconds'])
    return OpeningCapture(metadata,plan.normalized_progression)


class OpeningComparisonTests(unittest.TestCase):
    def test_exact_components_and_two_stage_target_never_claim_a_restart(self):
        source=full_capture();original=copy.deepcopy(source.metadata);observed=normalized_observed(source)
        result=compare_openings(source,observed)
        self.assertTrue(result['encounterEntryEqual']);self.assertFalse(result['rawProgressionEqual'])
        self.assertTrue(result['sourceNormalizationSupported']);self.assertTrue(result['normalizedProgressionEqual'])
        self.assertEqual(result['fieldAnchorComparison'],dict(mapIdEqual=True,positionEqual=True,directionEqual=True))
        self.assertIs(result['restartVerified'],False);self.assertIs(result['independentCaptureVerified'],False)
        self.assertEqual(source.metadata,original)
        same=compare_openings(source,source)
        self.assertTrue(same['rawProgressionEqual']);self.assertFalse(same['normalizedProgressionEqual']);self.assertFalse(same['restartVerified'])

    def test_full_unknown_progression_and_nested_encounter_changes_are_detected_without_values(self):
        source=full_capture();observed=normalized_observed(source)
        document=SaveDocument.decode(observed.progression);document.root['UnknownFutureField'].raw=b'\x01\x00\x00\x00'
        raw=document.prefix+document.root.encode()+document.suffix
        observed=OpeningCapture(dict(observed.metadata,sha256=hashlib.sha256(raw).hexdigest(),byteLength=len(raw)),raw)
        observed.metadata['entry']['addPassiveDataList'][0][1]=303;observed.metadata['entry']['isBounty']=True
        observed.metadata['fieldAnchor']['position'][0]=9
        result=compare_openings(source,observed)
        self.assertEqual(result['changedEntryFields'],['addPassiveDataList','isBounty'])
        self.assertFalse(result['normalizedProgressionEqual']);self.assertFalse(result['fieldAnchorComparison']['positionEqual'])
        self.assertNotIn('"addPassiveDataList":',json.dumps(result))

    def test_legacy_clock_is_unavailable_rather_than_inferred(self):
        source=full_capture();source.metadata.pop('playtimeBasis');source.metadata.pop('fieldAnchor')
        result=compare_openings(source,source)
        self.assertFalse(result['sourceNormalizationSupported']);self.assertIsNone(result['expectedNormalizedSha256'])
        self.assertIsNone(result['normalizedProgressionEqual']);self.assertIsNone(result['fieldAnchorComparison'])
        self.assertTrue(result['rawProgressionEqual']);self.assertFalse(result['restartVerified'])

    def test_complete_typed_bounded_entry_is_required(self):
        original=full_capture().metadata['entry']
        for key,value in [('skipIntro',1),('ap',True),('preemptiveRelation',256),('probabilityOfSuccessfulEscape',float('inf')),
                          ('probabilityOfSuccessfulEscape',10**400),('bgmCueName','x\0y'),('bgmCueName','x'*256),('bgmCueName','\ud800'),
                          ('enemiesUnitIdList',[1]*65),('addPassiveDataList',[[1]*129])]:
            entry=copy.deepcopy(original);entry[key]=value
            with self.subTest(key=key),self.assertRaises(ValueError):validate_complete_entry(entry)
        source=full_capture();source.metadata['entry'].pop('ap')
        source.validate() # Old partial diagnostics remain valid, but cannot claim full comparison coverage.
        with self.assertRaisesRegex(ValueError,'insufficient coverage'):compare_openings(source,source)

    def test_export_is_a_small_receipt_and_rejects_save_extensions(self):
        source=full_capture();observed=normalized_observed(source)
        with tempfile.TemporaryDirectory() as folder:
            path=Path(folder)/'check.ffrcomparison.json';export_opening_comparison(path,source,observed)
            result=json.loads(path.read_text(encoding='utf8'));self.assertIs(result['restorable'],False)
            self.assertNotIn('GVAS',path.read_text(encoding='utf8'))
            for name in ['state.sav','state.ffrsnapshot','state.ffropening']:
                with self.assertRaises(ValueError):export_opening_comparison(Path(folder)/name,source,observed)
