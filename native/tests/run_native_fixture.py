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
    parser.add_argument('--mode', required=True, choices=['good', 'bad-game', 'bad-loader','bad-function'])
    parser.add_argument('--work', required=True, type=Path)
    args = parser.parse_args()
    args.work.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(dir=args.work.resolve()) as folder:
        root = Path(folder)/'FFRHelperRuntimeNative'
        (root/'dlls').mkdir(parents=True)
        (root/'connection').mkdir()
        module = root/'dlls/main.dll'
        shutil.copy2(args.module, module)
        subprocess.run([str(args.exe.resolve()), str(module), str(args.loader.resolve()), str(root/'connection'), args.mode], check=True, timeout=30)
        if args.mode != 'bad-loader':
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
