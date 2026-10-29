import copy
import hashlib
import json
from pathlib import Path
import tempfile
import time
import unittest
from unittest.mock import patch
import warnings
import zipfile

from ffr_helper import recovery
from ffr_helper.codec import SaveDocument
from ffr_helper.game_build import GAME_SHA256
from ffr_helper.opening import load_opening
from ffr_helper.runtime import NativeEntryRecorder
from ffr_helper.storage import CheckpointStore
from test_playtime_plan import capture_with_clock
from test_runtime import sample_report


def field_anchor():
    return dict(kind='partial-field-anchor',restorable=False,source='live-player-root-transform',coverage='position-and-direction-only',
                sameWorldChecked=True,mapId=1000,position=[123.125,-456.25,7.5],direction=2)


def source_opening():
    source=capture_with_clock();source.metadata['fieldAnchor']=field_anchor();return source


def sample_recovery(source=None,session='a'*32,nonce='b'*32):
    source=source or source_opening()
    doc=SaveDocument.decode(source.progression);doc.root['player'].struct()['currentMoney'].set_integer(123)
    raw=doc.prefix+doc.root.encode()+doc.suffix
    meta=dict(format='ffr-helper-partial-recovery',version=1,kind='diagnostic-field-recovery',readOnly=True,restorable=False,
              gameSha256=GAME_SHA256,session=session,request=nonce,captureNumber=1,sourceSequence=source.metadata['sequence'],
              sourceOpeningHash=source.metadata['sha256'],sourceMapId=1000,captureTime=int(time.time()),
              progression=dict(file='recovery-progression.gvas',byteLength=len(raw),sha256=hashlib.sha256(raw).hexdigest()),
              fieldAnchor=field_anchor(),playtimeBasis=copy.deepcopy(source.metadata['playtimeBasis']),
              coherence=dict(matchingSerializations=2,stableFieldClockSamples=3,rootIdentityUnchanged=True,fieldBoundaryChecked=True),
              missing=recovery.MISSING.copy())
    return recovery.PartialRecoveryCapture(json.dumps(meta).encode('utf8'),raw)


def sample_receipt(capture,idle=False):
    meta=capture.metadata
    report=dict(protocol=1,version=1,adapter='ffr-helper-native-recovery-observer',format='ffr-helper-recovery-report',
                kind='diagnostic-observation',readOnly=True,restorable=False,gameSha256=GAME_SHA256,session=meta['session'],
                time=int(time.time()),available=True,fieldReady=True,boundaryTime=int(time.time()),boundarySequence=meta['sourceSequence'],
                boundaryOpeningHash=meta['sourceOpeningHash'],request=meta['request'],sequence=meta['sourceSequence'],
                openingHash=meta['sourceOpeningHash'],captureNumber=meta['captureNumber'],captureTime=meta['captureTime'],phase='captured',
                message='Synthetic fixture',metadata=dict(file='recovery-metadata.json',byteLength=len(capture.metadata_raw),
                                                        sha256=hashlib.sha256(capture.metadata_raw).hexdigest()))
    if idle:report.update(request='',sequence=0,openingHash='',captureNumber=0,captureTime=0,phase='idle',metadata=None)
    return report


def write_files(folder,capture,report):
    (folder/'recovery-metadata.json').write_bytes(capture.metadata_raw)
    (folder/'recovery-progression.gvas').write_bytes(capture.progression)
    (folder/'recovery-report.json').write_text(json.dumps(report),encoding='utf8')


class RecoveryTests(unittest.TestCase):
    def test_archive_preserves_metadata_bytes_progression_and_unknown_fields(self):
        capture=sample_recovery().validate()
        external=capture.metadata;external['fieldAnchor']['position'][0]=999
        self.assertEqual(capture.metadata['fieldAnchor']['position'][0],123.125)
        with tempfile.TemporaryDirectory() as folder:
            path=Path(folder)/'field.ffrrecovery';recovery.export_recovery(path,capture)
            loaded=recovery.load_recovery(path)
            self.assertEqual((loaded.metadata_raw,loaded.progression),(capture.metadata_raw,capture.progression))
            self.assertEqual(SaveDocument.decode(loaded.progression).root['UnknownFutureField'].integer(),123456789)
            self.assertEqual(SaveDocument.decode(loaded.progression).root['player'].struct()['currentMoney'].integer(),123)
            with self.assertRaises(ValueError):load_opening(path)
            with self.assertRaises(ValueError):CheckpointStore(Path(folder)/'store').read(path)

    def test_metadata_rejects_completeness_map_pointer_and_coherence_claims(self):
        capture=sample_recovery()
        changes=[('restorable',True),('readOnly',False),('gameSha256','0'*64),('sourceMapId',1001),('missing',[]),
                 ('address',1234),('sourceSequence',True),('captureNumber',0)]
        for key,value in changes:
            meta=capture.metadata;meta[key]=value
            with self.subTest(key=key),self.assertRaises(ValueError):recovery.validate_recovery_metadata(meta)
        meta=capture.metadata;meta['coherence']['rootIdentityUnchanged']=1
        with self.assertRaises(ValueError):recovery.validate_recovery_metadata(meta)
        meta=capture.metadata;meta['fieldAnchor']['nativePointer']=1234
        with self.assertRaises(ValueError):recovery.validate_recovery_metadata(meta)

    def test_clock_must_match_full_gvas_and_blob_integrity(self):
        capture=sample_recovery()
        meta=capture.metadata;meta['playtimeBasis']['savedSeconds']+=1
        with self.assertRaisesRegex(ValueError,'serialized saved float'):
            recovery.PartialRecoveryCapture(json.dumps(meta).encode(),capture.progression).validate()
        for raw in [capture.progression[:-1],b'X'+capture.progression[1:]]:
            with self.assertRaises(ValueError):recovery.PartialRecoveryCapture(capture.metadata_raw,raw).validate()
        raw=capture.progression+b'\0';meta=capture.metadata
        meta['progression'].update(byteLength=len(raw),sha256=hashlib.sha256(raw).hexdigest())
        with self.assertRaises(ValueError):recovery.PartialRecoveryCapture(json.dumps(meta).encode(),raw).validate()

    def test_reports_reject_bad_types_unbound_or_uncommitted_files(self):
        valid=sample_receipt(sample_recovery())
        for key,value in [('restorable',True),('available',1),('phase',[]),('sequence',False),('captureNumber',True),
                          ('boundarySequence',0),('metadata',None),('time',False),('gameSha256','0'*64)]:
            report=copy.deepcopy(valid);report[key]=value
            with self.subTest(key=key),self.assertRaises(ValueError):recovery.validate_recovery_report(report)
        report=copy.deepcopy(valid);report['metadata']['file']='../personal.sav'
        with self.assertRaises(ValueError):recovery.validate_recovery_report(report)
        report=copy.deepcopy(valid);report['phase']='write-failed'
        with self.assertRaises(ValueError):recovery.validate_recovery_report(report)
        idle=sample_receipt(sample_recovery(),idle=True);idle['sequence']=False
        with self.assertRaises(ValueError):recovery.validate_recovery_report(idle)

    def test_live_files_require_matching_receipt_and_hashes(self):
        capture=sample_recovery();report=sample_receipt(capture)
        with tempfile.TemporaryDirectory() as folder:
            root=Path(folder);write_files(root,capture,report)
            self.assertEqual(recovery.read_live_recovery(root,report).progression,capture.progression)
            altered=copy.deepcopy(report);altered['request']='c'*32
            with self.assertRaisesRegex(ValueError,'another request'):recovery.read_live_recovery(root,altered)
            (root/'recovery-metadata.json').write_bytes(capture.metadata_raw+b' ')
            with self.assertRaisesRegex(ValueError,'metadata changed'):recovery.read_live_recovery(root,report)
            write_files(root,capture,report);(root/'recovery-progression.gvas').write_bytes(capture.progression[:-1])
            with self.assertRaises(ValueError):recovery.read_live_recovery(root,report)

    def test_uncommitted_receipts_do_not_read_old_sidecars(self):
        report=sample_receipt(sample_recovery());report.update(phase='write-failed',metadata=None)
        with tempfile.TemporaryDirectory() as folder:
            with self.assertRaisesRegex(ValueError,'No committed'):recovery.read_live_recovery(Path(folder),report)

    def test_duplicate_json_keys_and_archive_entries_are_refused(self):
        capture=sample_recovery();duplicate=capture.metadata_raw[:-1]+b',"restorable":false}'
        with self.assertRaisesRegex(ValueError,'duplicate keys'):recovery.PartialRecoveryCapture(duplicate,capture.progression).validate()
        with tempfile.TemporaryDirectory() as folder:
            path=Path(folder)/'field.ffrrecovery'
            for extra in ['../outside','recovery-metadata.json']:
                with zipfile.ZipFile(path,'w') as archive:
                    archive.writestr('recovery-metadata.json',capture.metadata_raw);archive.writestr('recovery-progression.gvas',capture.progression)
                    with warnings.catch_warnings():warnings.simplefilter('ignore',UserWarning);archive.writestr(extra,'x')
                with self.assertRaisesRegex(ValueError,'unexpected or duplicate'):recovery.load_recovery(path)

    def test_size_limits_and_save_filename_separation(self):
        capture=sample_recovery();meta=capture.metadata;meta['progression']['byteLength']=recovery.MAX_PROGRESSION+1
        with self.assertRaises(ValueError):recovery.validate_recovery_metadata(meta)
        with self.assertRaises(ValueError):recovery.PartialRecoveryCapture(b' '*(recovery.MAX_METADATA+1),capture.progression).validate()
        with tempfile.TemporaryDirectory() as folder:
            for suffix in ['.sav','.ffrsnapshot','.ffropening']:
                with self.assertRaises(ValueError):recovery.export_recovery(Path(folder)/('invalid'+suffix),capture)
            self.assertEqual(list(Path(folder).iterdir()),[])

    def setup_bridge(self,folder):
        root=Path(folder);(root/'connection').mkdir();recorder=NativeEntryRecorder(root);recorder.capture()
        source=source_opening();opening_report=sample_report(recorder);opening_report['adapter']=recorder.adapter_name
        opening_report['objects']={'nativeEntry':{'opening':source.metadata}}
        (recorder.connection/'opening-progression.gvas').write_bytes(source.progression)
        ready=sample_receipt(sample_recovery(source,recorder.session),idle=True)
        (recorder.connection/'recovery-report.json').write_text(json.dumps(ready),encoding='utf8')
        return recorder,source,opening_report,recovery.PartialRecoveryBridge(recorder)

    def test_bridge_sends_one_read_request_and_accepts_advancing_publication_time(self):
        with tempfile.TemporaryDirectory() as folder:
            recorder,source,opening_report,bridge=self.setup_bridge(folder)
            bridge.send(opening_report)
            text=(recorder.connection/'recovery-request.txt').read_text()
            self.assertTrue(text.startswith('FFR_HELPER_PARTIAL_RECOVERY_V1\n'))
            self.assertFalse((recorder.connection/'abort-request.txt').exists())
            with self.assertRaises(ValueError):bridge.send(opening_report)
            capture=sample_recovery(source,recorder.session,bridge.request['nonce']);report=sample_receipt(capture)
            write_files(recorder.connection,capture,report)
            newer=copy.deepcopy(report);newer['time']+=1
            with patch.object(bridge,'status',return_value=newer):
                self.assertEqual(bridge.capture_receipt(report).progression,capture.progression)

    def test_bridge_rereads_receipt_after_files_and_refuses_replacement(self):
        with tempfile.TemporaryDirectory() as folder:
            recorder,source,opening_report,bridge=self.setup_bridge(folder);bridge.send(opening_report)
            capture=sample_recovery(source,recorder.session,bridge.request['nonce']);report=sample_receipt(capture)
            write_files(recorder.connection,capture,report)
            actual_read=recovery.read_live_recovery
            def changed(connection,receipt):
                out=actual_read(connection,receipt);new=copy.deepcopy(report);new['request']='f'*32
                (connection/'recovery-report.json').write_text(json.dumps(new),encoding='utf8');return out
            with patch.object(recovery,'read_live_recovery',side_effect=changed),self.assertRaisesRegex(ValueError,'receipt changed'):
                bridge.capture_receipt(report)

    def test_stale_or_wrong_session_readiness_cannot_send(self):
        with tempfile.TemporaryDirectory() as folder:
            recorder,source,opening_report,bridge=self.setup_bridge(folder)
            for key,value in [('time',int(time.time())-60),('session','f'*32),('fieldReady',False)]:
                ready=sample_receipt(sample_recovery(source,recorder.session),idle=True);ready[key]=value
                (recorder.connection/'recovery-report.json').write_text(json.dumps(ready),encoding='utf8')
                with self.subTest(key=key),self.assertRaises(ValueError):bridge.send(opening_report)
            self.assertFalse((recorder.connection/'recovery-request.txt').exists())
