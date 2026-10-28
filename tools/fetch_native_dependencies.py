"""Fetch only a checksum-pinned native build dependency, never install it."""
import argparse
import hashlib
from pathlib import Path
import urllib.request
import zipfile

URL = 'https://github.com/TsudaKageyu/minhook/archive/refs/tags/v1.3.4.zip'
SHA256 = '172708123daa0c98d20d3a980b16a50be14af243dc95dee6f79c24193ad010e4'


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path, default=Path('local/native-deps'))
    args = parser.parse_args()
    root = args.output.resolve()
    root.mkdir(parents=True, exist_ok=True)
    archive = root/'minhook-v1.3.4.zip'
    if archive.exists():
        raw = archive.read_bytes()
    else:
        request = urllib.request.Request(URL, headers={'User-Agent': 'FFR-Helper-Build'})
        with urllib.request.urlopen(request, timeout=45) as response:
            raw = response.read(5_000_001)
        if len(raw) > 5_000_000 or hashlib.sha256(raw).hexdigest() != SHA256:
            raise RuntimeError('MinHook download checksum/size mismatch.')
        archive.write_bytes(raw)
    if hashlib.sha256(raw).hexdigest() != SHA256:
        raise RuntimeError('Cached MinHook archive checksum mismatch.')
    with zipfile.ZipFile(archive) as package:
        for entry in package.infolist():
            target = (root/entry.filename).resolve()
            if not target.is_relative_to(root) or entry.file_size > 5_000_000:
                raise RuntimeError('Unexpected dependency archive path or size.')
            if entry.is_dir():
                target.mkdir(parents=True, exist_ok=True)
            else:
                data = package.read(entry)
                if target.exists():
                    if target.read_bytes() != data:
                        raise RuntimeError('Existing dependency files differ; choose a fresh --output directory.')
                else:
                    target.parent.mkdir(parents=True, exist_ok=True)
                    with target.open('xb') as handle:
                        handle.write(data)
    print('Verified MinHook 1.3.4 source:', root/'minhook-1.3.4')


if __name__ == '__main__':
    main()
