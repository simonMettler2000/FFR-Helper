"""Build a standalone app on the target OS; no cross-compilation is assumed."""
from pathlib import Path
from importlib import metadata
import subprocess
import sys

root = Path(__file__).resolve().parents[1]
command = [sys.executable, '-m', 'PyInstaller', '--noconfirm', '--clean', '--windowed',
           '--name', 'FFR Helper', '--paths', str(root),
           '--add-data', str(root / 'ffr_helper/data') + ':ffr_helper/data',
           '--add-data', str(root / 'bridge') + ':bridge',
           '--exclude-module', 'PySide6.QtWebEngineCore', '--exclude-module', 'PySide6.QtWebEngineWidgets',
           '--exclude-module', 'PySide6.QtQml', '--exclude-module', 'PySide6.QtQuick']
for name in ['README.md', 'LICENSE', 'THIRD_PARTY_NOTICES.md']:
    command += ['--add-data', str(root/name) + ':documentation']
for name in ['PySide6_Essentials', 'shiboken6', 'cryptography', 'pyinstaller']:
    distribution = metadata.distribution(name)
    for entry in distribution.files or []:
        if entry.name.lower().startswith(('license', 'copying')):
            source = Path(distribution.locate_file(entry))
            if source.is_file():
                command += ['--add-data', str(source) + ':licenses/' + name]
if sys.platform == 'darwin':
    command += ['--osx-bundle-identifier', 'local.ffrhelper.desktop']
else:
    command += ['--onefile']
subprocess.run(command + [str(root / 'run_helper.py')], cwd=root, check=True)
