"""Build a standalone app on the target OS; no cross-compilation is assumed."""
from pathlib import Path
import subprocess
import sys

root = Path(__file__).resolve().parents[1]
command = [sys.executable, '-m', 'PyInstaller', '--noconfirm', '--clean', '--windowed',
           '--name', 'FFR Helper', '--paths', str(root),
           '--add-data', str(root / 'ffr_helper/data') + ':ffr_helper/data',
           '--add-data', str(root / 'bridge') + ':bridge',
           '--exclude-module', 'PySide6.QtWebEngineCore', '--exclude-module', 'PySide6.QtWebEngineWidgets',
           '--exclude-module', 'PySide6.QtQml', '--exclude-module', 'PySide6.QtQuick']
if sys.platform == 'darwin':
    command += ['--osx-bundle-identifier', 'local.ffrhelper.desktop']
else:
    command += ['--onefile']
subprocess.run(command + [str(root / 'run_helper.py')], cwd=root, check=True)

