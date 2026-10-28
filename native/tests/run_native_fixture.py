"""Run the real detour/transport in an isolated fixture, never in FFR."""
import argparse
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
    parser.add_argument('--mode', required=True, choices=['good', 'bad-game', 'bad-loader'])
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
                after=json.loads((root/'connection/after-expiry-report.json').read_text(encoding='utf8'))
                assert len(after['events'])==1 and after['objects']['nativeEntry']['captureFailures']==1
            else:
                assert report['events']==[] and report['objects']['nativeEntry']['ready'] is False


if __name__=='__main__': main()
