"""Run the real detour/transport in an isolated fixture, never in FFR."""
import argparse
import hashlib
import json
from pathlib import Path
import shutil
import subprocess
import tempfile
import sys
from unittest.mock import patch


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--exe', required=True, type=Path)
    parser.add_argument('--loader', required=True, type=Path)
    parser.add_argument('--module', required=True, type=Path)
    parser.add_argument('--mode', required=True, choices=['good', 'bad-game', 'bad-loader','bad-function','abort','abort-denied',
                                                       'clone','clone-unready','clone-mismatch','clone-mutates','clone-denied',
                                                       'combat','combat-after-action','combat-denied','combat-stale',
                                                       'normalize','normalize-unready','normalize-stale','normalize-template','normalize-write-failed',
                                                       'recovery','recovery-denied','recovery-stale','recovery-drift','recovery-write-failed','recovery-map-changed'])
    parser.add_argument('--work', required=True, type=Path)
    args = parser.parse_args()
    args.work.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(dir=args.work.resolve()) as folder:
        root = Path(folder)/('FFRHelperBattlePractice' if args.mode.startswith(('abort','clone','combat','recovery','normalize')) else 'FFRHelperRuntimeNative')
        (root/'dlls').mkdir(parents=True)
        (root/'connection').mkdir()
        module = root/'dlls/main.dll'
        shutil.copy2(args.module, module)
        subprocess.run([str(args.exe.resolve()), str(module), str(args.loader.resolve()), str(root/'connection'), args.mode], check=True, timeout=30)
        if args.mode.startswith('normalize'):
            sys.path.insert(0,str(Path(__file__).resolve().parents[2]))
            from ffr_helper import opening
            from ffr_helper.normalization_probe import NormalizationProbeBridge, validate_normalization_probe
            from ffr_helper.playtime_plan import plan_opening_playtime
            from ffr_helper.runtime import NativeEntryRecorder
            status=json.loads((root/'connection/normalization-final-status.json').read_text(encoding='utf8'))
            probe=validate_normalization_probe(status['normalizationProbe'])
            fixture_hash=hashlib.sha256(args.exe.read_bytes()).hexdigest()
            source_report=json.loads((root/'connection/normalization-source-report.json').read_text(encoding='utf8'))
            assert source_report['objects']['nativeEntry']['opening']['gameSha256']==fixture_hash
            with patch.object(opening,'GAME_SHA256',fixture_hash):
                source=opening.OpeningCapture(source_report['objects']['nativeEntry']['opening'],(root/'connection/normalization-source-progression.gvas').read_bytes()).validate()
                assert source.metadata['playtimeBasis']['savedSeconds']==1000.25
                expected_phase=('rejected' if args.mode in ['normalize-unready','normalize-stale'] else 'passed' if args.mode=='normalize' else 'failed')
                assert probe['phase']==expected_phase and probe['passed']==(args.mode=='normalize')
                assert status['phase']=='idle' and status['importProbe']['available'] is False
                assert probe['attempted'] and probe['pinsReleased'] and probe['retainedOwnedPins']==0
                if args.mode=='normalize':
                    # Exercise the same production consumer that an explicit live
                    # test uses, including independent Python byte preparation.
                    recorder=NativeEntryRecorder(root);recorder.session='a'*32
                    bridge=NormalizationProbeBridge(recorder);bridge.source=source;bridge.plan=plan_opening_playtime(source)
                    bridge.request=dict(nonce=probe['nonce'],deadline=probe['captureTime']+8,sequence=probe['sequence'],sha256=probe['openingHash'])
                    evidence=bridge.evidence(probe)
                    assert evidence['expected']==evidence['clone']==bridge.plan.normalized_progression
                    assert probe['normalizedSeconds']==1970.25 and source.progression!=evidence['clone']
                elif args.mode=='normalize-template':
                    assert probe['writeAttempted'] and not probe['writeReturned'] and not probe['cloneAfterMatches'] and probe['liveUnchanged']
                elif args.mode=='normalize-write-failed':
                    assert probe['enginePassed'] and not probe['filesCommitted']
        elif args.mode.startswith('recovery'):
            sys.path.insert(0,str(Path(__file__).resolve().parents[2]))
            from ffr_helper import recovery, opening
            from ffr_helper.codec import SaveDocument
            fixture_hash=hashlib.sha256(args.exe.read_bytes()).hexdigest()
            first=json.loads((root/'connection/recovery-first-report.json').read_text(encoding='utf8'))
            final=json.loads((root/'connection/recovery-final-report.json').read_text(encoding='utf8'))
            assert first['gameSha256']==final['gameSha256']==fixture_hash
            with patch.object(recovery,'GAME_SHA256',fixture_hash),patch.object(opening,'GAME_SHA256',fixture_hash):
                recovery.validate_recovery_report(first);recovery.validate_recovery_report(final)
                source_report=json.loads((root/'connection/recovery-source-report.json').read_text(encoding='utf8'))
                source=opening.OpeningCapture(source_report['objects']['nativeEntry']['opening'],(root/'connection/recovery-source-progression.gvas').read_bytes()).validate()
                if args.mode=='recovery':
                    original=recovery.PartialRecoveryCapture((root/'connection/recovery-first-metadata.json').read_bytes(),
                                                             (root/'connection/recovery-first-progression.gvas').read_bytes()).validate()
                    latest=recovery.read_live_recovery(root/'connection',final)
                    assert first['captureNumber']==1 and final['captureNumber']==2
                    assert original.metadata['sourceOpeningHash']==latest.metadata['sourceOpeningHash']==source.metadata['sha256']
                    assert SaveDocument.decode(original.progression).root['player'].struct()['currentMoney'].integer()==202
                    assert SaveDocument.decode(latest.progression).root['player'].struct()['currentMoney'].integer()==303
                    assert SaveDocument.decode(source.progression).root['player'].struct()['currentMoney'].integer()==101
                    assert latest.metadata['playtimeBasis']['savedSeconds']==1000.25 and latest.metadata['sourceMapId']==1000
                    target=root/'connection/verified.ffrrecovery';recovery.export_recovery(target,latest)
                    restored=recovery.load_recovery(target)
                    assert restored.progression==latest.progression and restored.metadata_raw==latest.metadata_raw
                else:
                    expected={'recovery-denied':'rejected','recovery-stale':'rejected','recovery-map-changed':'rejected',
                              'recovery-drift':'failed','recovery-write-failed':'write-failed'}[args.mode]
                    assert first['phase']==final['phase']==expected and final['metadata'] is None
        elif args.mode.startswith('combat'):
            report=json.loads((root/'connection/combat-final-report.json').read_text(encoding='utf8'))
            # Test only: substitute the independently verified dummy EXE identity
            # while checking the actual DLL output with the production parser.
            # The production API has no switch for accepting another game build.
            sys.path.insert(0,str(Path(__file__).resolve().parents[2]))
            from ffr_helper import combat
            fixture_hash=hashlib.sha256(args.exe.read_bytes()).hexdigest()
            assert report['gameSha256']==fixture_hash
            with patch.object(combat,'GAME_SHA256',fixture_hash):
                combat.validate_combat_report(report)
                if (root/'connection/combat-first-report.json').exists():
                    combat.validate_combat_report(json.loads((root/'connection/combat-first-report.json').read_text(encoding='utf8')))
            assert report['adapter']=='ffr-helper-native-combat-observer' and report['readOnly'] is True and report['restorable'] is False
            assert report['format']=='ffr-helper-combat-report' and report['gameSha256']
            if args.mode in ['combat-denied','combat-stale']:
                assert report['phase']=='rejected' and report['observation'] is None
            else:
                first=json.loads((root/'connection/combat-first-report.json').read_text(encoding='utf8'))
                assert report['captureNumber']==2 and first['captureNumber']==1
                assert first['observation']['units'][0]['raw']['hpBase']==150
                assert report['observation']['units'][0]['raw']['hpBase']==7
                assert report['observation']['units'][0]['raw']['temporaryHp']==25
                assert report['observation']['units'][0]['raw']['actionCount']==(2 if args.mode=='combat-after-action' else 0)
                assert report['observation']['kind']=='partial-combat-observation' and report['observation']['restorable'] is False
                assert len(report['observation']['units'])==2 and report['request']=='d'*32
        elif args.mode.startswith('clone'):
            status=json.loads((root/'connection/clone-final-status.json').read_text(encoding='utf8'))
            assert status['readOnly'] is False and status['restorable'] is False and status['phase']=='idle'
            probe=status['importProbe']
            assert probe['available'] is True and probe['restorable'] is False and probe['pinsReleased'] is True
            assert probe['phase']==('rejected' if args.mode in ['clone-unready','clone-denied'] else 'passed' if args.mode=='clone' else 'failed')
            assert probe['passed']==(args.mode=='clone')
            if args.mode=='clone-mismatch':assert probe['cloneMatches'] is False and probe['liveUnchanged'] is True
            if args.mode=='clone-mutates':assert probe['cloneMatches'] is True and probe['liveUnchanged'] is False
            assert probe['loaderReady']==(args.mode!='clone-unready')
        elif args.mode.startswith('abort'):
            status=json.loads((root/'connection/abort-final-status.json').read_text(encoding='utf8'))
            assert status['adapter']=='ffr-helper-opening-abort-trial' and status['readOnly'] is False
            assert status['restorable'] is False and status['runtimeMode']=='experimental-abort-test'
            assert status['phase']==('manager-cleared' if args.mode=='abort' else 'rejected')
            if args.mode=='abort':
                for key,name in [('recovery','abort-recovery.gvas'),('after','abort-after.gvas')]:
                    raw=(root/'connection'/name).read_bytes()
                    assert status[key]['sha256']==hashlib.sha256(raw).hexdigest() and status[key]['byteLength']==len(raw)
        elif args.mode != 'bad-loader':
            report = json.loads((root/'connection/first-report.json').read_text(encoding='utf8'))
            assert report['adapter']=='ffr-helper-native-entry-observer'
            assert report['readOnly'] is True and report['restorable'] is False
            if args.mode=='good':
                assert len(report['events'])==1
                entry=report['events'][0]['entry']
                assert entry['friendliesUnitIdList']==[1001,1002]
                assert entry['enemiesUnitIdList']==[11,22]
                assert entry['addPassiveDataList']==[[77,88]]
                assert entry['bgmCueName']=='Fixture'
                assert entry['skipIntro'] is False
                metadata=report['objects']['nativeEntry']['opening']
                progression=(root/'connection/first-progression.gvas').read_bytes()
                assert progression==b'GVAS\x03\0\0\0\x2a\x07\x09\0'
                assert metadata['sha256']==hashlib.sha256(progression).hexdigest()
                assert metadata['byteLength']==len(progression) and metadata['sequence']==1
                assert metadata['entry']==entry and metadata['capturePoint']=='before-native-entry'
                anchor=metadata['fieldAnchor']
                assert anchor['kind']=='partial-field-anchor' and anchor['restorable'] is False
                assert anchor['sameWorldChecked'] is True and anchor['mapId']==1000 and anchor['direction']==2
                assert anchor['position']==[123.125,-456.25,7.5]
                assert report['objects']['nativeEntry']['fieldAnchorReady'] is True
                assert report['objects']['nativeEntry']['fieldAnchorFailures']==0
                sys.path.insert(0,str(Path(__file__).resolve().parents[2]))
                from ffr_helper.playtime import validate_playtime_basis
                basis=validate_playtime_basis(metadata['playtimeBasis'])
                assert basis['savedSeconds']==1000.25 and basis['startSeconds']==9000
                assert basis['sampleSeconds']==10000 and basis['totalSuspendedSeconds']==30 and basis['elapsedSeconds']==970
                assert basis['suspendedStartSeconds']==0 and basis['restorable'] is False
                assert basis['roundingMode']=='nearest-even'
                assert report['objects']['nativeEntry']['playtimeBasisReady'] is True
                assert report['objects']['nativeEntry']['playtimeBasisFailures']==0
                replacement=json.loads((root/'connection/replacement-report.json').read_text(encoding='utf8'))
                second=replacement['objects']['nativeEntry']['opening']
                second_raw=(root/'connection/replacement-progression.gvas').read_bytes()
                assert second['sequence']==2 and second['entry']['friendliesUnitIdList']==[9999,1002]
                assert second['sha256']==hashlib.sha256(second_raw).hexdigest() and second_raw[8]==43
                assert second['playtimeBasis'] is None and replacement['objects']['nativeEntry']['playtimeBasisFailures']==1
                assert progression[8]==42 and metadata['entry']['friendliesUnitIdList']==[1001,1002]
                after=json.loads((root/'connection/after-expiry-report.json').read_text(encoding='utf8'))
                assert len(after['events'])==2 and after['objects']['nativeEntry']['captureFailures']==1
                assert after['objects']['nativeEntry']['opening'] is None
            else:
                assert report['events']==[] and report['objects']['nativeEntry']['ready'] is False


if __name__=='__main__': main()
