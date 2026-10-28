"""Package only the production-profile observer, disabled by default."""
import argparse
import hashlib
from pathlib import Path
import re
import zipfile


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--dll', type=Path, required=True)
    parser.add_argument('--output', type=Path, default=Path('dist/native-packages'))
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[1]
    profile = (root/'native/include/build_profile.hpp').read_text(encoding='utf8')
    raw = args.dll.read_bytes()
    for key in ['game_sha256', 'loader_sha256']:
        expected = re.search(rf'{key}\[\] = "([0-9a-f]{{64}})"', profile).group(1)
        if expected.encode('ascii')+b'\0' not in raw:
            raise RuntimeError('The DLL does not contain the production profile; test modules must not be packaged.')
    for export in [b'start_mod\0', b'uninstall_mod\0', b'ffr_observer_game_profile\0', b'ffr_observer_loader_profile\0']:
        if export not in raw:
            raise RuntimeError('Observer metadata/lifecycle exports are missing.')
    if b'read-only-observer\0' not in raw or b'experimental-abort-test\0' in raw:
        raise RuntimeError('A mutation-capable test build must never be packaged as the read-only observer.')
    args.output.mkdir(parents=True, exist_ok=True)
    target = args.output/'FFR-Helper-Native-Observer-Windows-x86_64-dev.zip'
    prefix = 'FFRHelperRuntimeNative/'
    with zipfile.ZipFile(target, 'w', zipfile.ZIP_DEFLATED) as package:
        package.writestr(prefix+'dlls/main.dll', raw)
        package.write(root/'native/README.md', prefix+'README.md')
        package.write(root/'native/licenses/MinHook.txt', prefix+'licenses/MinHook.txt')
        package.write(root/'LICENSE', prefix+'LICENSE.txt')
        package.writestr(prefix+'connection/', b'')
        # No enabled.txt marker: this unverified module requires explicit opt-in.
    digest = hashlib.sha256(target.read_bytes()).hexdigest()
    (target.parent/(target.name+'.sha256')).write_text(digest+'  '+target.name+'\n', encoding='ascii')
    print(target, 'SHA256', digest)


if __name__ == '__main__': main()
