import copy
import hashlib
import json
from pathlib import Path
import tempfile
import time
import unittest
import zipfile

from ffr_helper.codec import SaveDocument
from ffr_helper.editor import SaveEditor
from ffr_helper.opening import GAME_SHA256, OpeningCapture, export_opening, load_opening, read_live_opening
from ffr_helper.runtime import NativeEntryRecorder
from ffr_helper.storage import CheckpointStore
from test_core import synthetic_save
from test_runtime import sample_report


def sample_opening():
    document=SaveDocument.decode(synthetic_save())
    raw=document.prefix+document.root.encode()+bytes(4)
    metadata=dict(sequence=1,time=int(time.time()),capturePoint='before-native-entry',gameSha256=GAME_SHA256,
                  file='opening-progression.gvas',byteLength=len(raw),sha256=hashlib.sha256(raw).hexdigest(),
                  entry=dict(friendliesUnitIdList=[1001],enemiesUnitIdList=[987654],skipIntro=False))
    return OpeningCapture(metadata,raw)


class OpeningTests(unittest.TestCase):
    def test_capture_preserves_exp_mrp_resources_inventory_and_unknown_data(self):
        capture=sample_opening().validate()
        with tempfile.TemporaryDirectory() as folder:
            path=Path(folder)/'sample.ffropening'
            export_opening(path,capture)
            loaded=load_opening(path)
            self.assertEqual(loaded.progression,capture.progression)
            editor=SaveEditor.open(loaded.progression)
            self.assertEqual(editor.experience(1001),100)
            self.assertEqual(editor.vision_points(13017)[1001],42)
            self.assertEqual(editor.inventory()[1],5)
            self.assertEqual(editor.units()[1001]['HitPoint'].integer(),200)
            self.assertEqual(editor.units()[1001]['MagicPoint'].integer(),35)
            self.assertEqual(editor.root['UnknownFutureField'].integer(),123456789)
            with self.assertRaises(ValueError):
                CheckpointStore(Path(folder)/'store').read(path)

    def test_native_sidecar_is_bound_to_current_report_and_hash(self):
        capture=sample_opening()
        with tempfile.TemporaryDirectory() as folder:
            connection=Path(folder)/'connection';connection.mkdir()
            recorder=NativeEntryRecorder(Path(folder));recorder.capture()
            report=sample_report(recorder);report['adapter']=recorder.adapter_name
            report['objects']={'nativeEntry':{'opening':capture.metadata}}
            sidecar=connection/'opening-progression.gvas';sidecar.write_bytes(capture.progression)
            self.assertEqual(recorder.opening(report).progression,capture.progression)
            sidecar.write_bytes(capture.progression[:-1])
            with self.assertRaisesRegex(ValueError,'checksum'):
                recorder.opening(report)
            sidecar.write_bytes(capture.progression)
            report['session']='b'*32
            with self.assertRaisesRegex(ValueError,'fresh report'):
                recorder.opening(report)

    def test_wrong_source_path_build_or_metadata_is_refused(self):
        capture=sample_opening()
        for key,value in [('file','../personal.sav'),('gameSha256','0'*64),('sequence',True),('byteLength',2**40),('sha256','x'*64)]:
            metadata=copy.deepcopy(capture.metadata);metadata[key]=value
            with self.subTest(key=key),self.assertRaises(ValueError):
                OpeningCapture(metadata,capture.progression).validate()

    def test_matching_hash_does_not_make_an_invalid_gvas_acceptable(self):
        capture=sample_opening()
        for raw in [b'X'+capture.progression[1:],capture.progression+b'\0',capture.progression.replace(b'CPP_MuchaGameSaveData',b'CPP_InvalidClassName')]:
            metadata=copy.deepcopy(capture.metadata)
            metadata.update(byteLength=len(raw),sha256=hashlib.sha256(raw).hexdigest())
            with self.assertRaises(ValueError): OpeningCapture(metadata,raw).validate()

    def test_archives_reject_restorable_claims_and_extra_or_duplicate_entries(self):
        with tempfile.TemporaryDirectory() as folder:
            path=Path(folder)/'capture.ffropening';export_opening(path,sample_opening())
            with zipfile.ZipFile(path) as original:
                manifest=json.loads(original.read('manifest.json'));progression=original.read('progression.gvas')
            for claim in [True,None,0]:
                changed=copy.deepcopy(manifest);changed['restorable']=claim
                with zipfile.ZipFile(path,'w') as z:
                    z.writestr('manifest.json',json.dumps(changed));z.writestr('progression.gvas',progression)
                with self.assertRaisesRegex(ValueError,'incomplete diagnostics'): load_opening(path)
            for extra in ['../outside.txt','manifest.json']:
                with zipfile.ZipFile(path,'w') as z:
                    z.writestr('manifest.json',json.dumps(manifest));z.writestr('progression.gvas',progression)
                    # zipfile warns about duplicates; malformed input is intentional.
                    import warnings
                    with warnings.catch_warnings():
                        warnings.simplefilter('ignore',UserWarning);z.writestr(extra,'x')
                with self.assertRaisesRegex(ValueError,'unexpected or duplicate'): load_opening(path)

    def test_export_cannot_be_named_as_a_game_save_or_ordinary_checkpoint(self):
        with tempfile.TemporaryDirectory() as folder:
            for extension in ['.sav','.ffrsnapshot']:
                with self.assertRaisesRegex(ValueError,'ffropening'):
                    export_opening(Path(folder)/('sample'+extension),sample_opening())
            self.assertEqual(list(Path(folder).iterdir()),[])
