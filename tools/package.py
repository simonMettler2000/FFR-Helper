"""Package only built application files and public project instructions."""
import hashlib
from pathlib import Path
import platform
import shutil
import subprocess
import sys
import tarfile
import zipfile

root = Path(__file__).resolve().parents[1]
out = root / 'dist/packages'
out.mkdir(parents=True, exist_ok=True)
machine = platform.machine().lower()
if sys.platform == 'darwin':
    target = out / f'FFR-Helper-macOS-{machine}.zip'
    subprocess.run(['ditto', '-c', '-k', '--sequesterRsrc', '--keepParent',
                    str(root/'dist/FFR Helper.app'), str(target)], check=True)
elif sys.platform == 'win32':
    target = out / f'FFR-Helper-Windows-{machine}.zip'
    with zipfile.ZipFile(target, 'w', zipfile.ZIP_DEFLATED) as archive:
        archive.write(root/'dist/FFR Helper.exe', 'FFR Helper.exe')
        for name in ['README.md', 'THIRD_PARTY_NOTICES.md', 'LICENSE']:
            archive.write(root/name, name)
else:
    target = out / f'FFR-Helper-Linux-{machine}.tar.gz'
    with tarfile.open(target, 'w:gz') as archive:
        archive.add(root/'dist/FFR Helper', arcname='FFR Helper')
        for name in ['README.md', 'THIRD_PARTY_NOTICES.md', 'LICENSE']:
            archive.add(root/name, arcname=name)
checksum = hashlib.sha256(target.read_bytes()).hexdigest()
(out/(target.name+'.sha256')).write_text(checksum+'  '+target.name+'\n', encoding='ascii')
print(target.name, target.stat().st_size, 'bytes', checksum)
