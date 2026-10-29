"""Package the distinct mutation-capable experiment, disabled by default."""
import argparse
import hashlib
from pathlib import Path
import re
import zipfile

parser=argparse.ArgumentParser(description=__doc__)
parser.add_argument('--dll',type=Path,required=True)
parser.add_argument('--output',type=Path,default=Path('dist/abort-trial-packages'))
args=parser.parse_args()
root=Path(__file__).resolve().parents[1];raw=args.dll.read_bytes()
profile=(root/'native/include/build_profile.hpp').read_text(encoding='utf8')
for key in ['game_sha256','loader_sha256']:
    expected=re.search(rf'{key}\[\] = "([0-9a-f]{{64}})"',profile).group(1)
    if expected.encode()+b'\0' not in raw:raise RuntimeError('Production fingerprints are missing; fixture binaries must not be packaged.')
if b'experimental-abort-test\0' not in raw or b'read-only-observer\0' in raw or b'ffr_observer_runtime_mode\0' not in raw:
    raise RuntimeError('This is not the distinct experimental abort build.')
args.output.mkdir(parents=True,exist_ok=True)
target=args.output/'FFR-Helper-Opening-Abort-Trial-Windows-x86_64-dev.zip'
prefix='FFRHelperBattlePractice/'
with zipfile.ZipFile(target,'w',zipfile.ZIP_DEFLATED) as archive:
    archive.writestr(prefix+'dlls/main.dll',raw)
    archive.write(root/'native/ABORT_TRIAL.md',prefix+'README.md')
    archive.write(root/'native/PARTIAL_RECOVERY.md',prefix+'PARTIAL_RECOVERY.md')
    archive.write(root/'native/licenses/MinHook.txt',prefix+'licenses/MinHook.txt')
    archive.write(root/'LICENSE',prefix+'LICENSE.txt')
    archive.writestr(prefix+'connection/',b'')
digest=hashlib.sha256(target.read_bytes()).hexdigest()
(target.parent/(target.name+'.sha256')).write_text(digest+'  '+target.name+'\n',encoding='ascii')
print(target,'SHA256',digest)
