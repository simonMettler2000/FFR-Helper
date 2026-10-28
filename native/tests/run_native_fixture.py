"""Run the real detour/transport in an isolated fixture, never in FFR."""
import argparse
import hashlib
import json
from pathlib import Path
import shutil
import subprocess
import tempfile


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--exe', required=True, type=Path)
    parser.add_argument('--loader', required=True, type=Path)
    parser.add_argument('--module', required=True, type=Path)
    parser.add_argument('--mode', required=True, choices=['good', 'bad-game', 'bad-loader','bad-function','abort','abort-denied',
                                                       'clone','clone-unready','clone-mismatch','clone-mutates','clone-denied'])
    parser.add_argument('--work', required=True, type=Path)
    args = parser.parse_args()
    args.work.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(dir=args.work.resolve()) as folder:
        root = Path(folder)/('FFRHelperBattlePractice' if args.mode.startswith(('abort','clone')) else 'FFRHelperRuntimeNative')
        (root/'dlls').mkdir(parents=True)
        (root/'connection').mkdir()
        module = root/'dlls/main.dll'
        shutil.copy2(args.module, module)
        subprocess.run([str(args.exe.resolve()), str(module), str(args.loader.resolve()), str(root/'connection'), args.mode], check=True, timeout=30)
        if args.mode.startswith('clone'):
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
                replacement=json.loads((root/'connection/replacement-report.json').read_text(encoding='utf8'))
                second=replacement['objects']['nativeEntry']['opening']
                second_raw=(root/'connection/replacement-progression.gvas').read_bytes()
                assert second['sequence']==2 and second['entry']['friendliesUnitIdList']==[9999,1002]
                assert second['sha256']==hashlib.sha256(second_raw).hexdigest() and second_raw[8]==43
                assert progression[8]==42 and metadata['entry']['friendliesUnitIdList']==[1001,1002]
                after=json.loads((root/'connection/after-expiry-report.json').read_text(encoding='utf8'))
                assert len(after['events'])==2 and after['objects']['nativeEntry']['captureFailures']==1
                assert after['objects']['nativeEntry']['opening'] is None
            else:
                assert report['events']==[] and report['objects']['nativeEntry']['ready'] is False


if __name__=='__main__': main()
