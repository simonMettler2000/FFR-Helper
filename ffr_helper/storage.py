"""Atomic writes, verified backup checkpoints, and platform save discovery."""
from __future__ import annotations

from datetime import datetime, timezone
import hashlib
import json
import os
from pathlib import Path
import re
import subprocess
import sys
import tempfile
import zipfile

from .codec import MAX_FILE, SaveFormatError
from .editor import SaveEditor


def app_directory():
    if sys.platform == 'win32':
        root = Path(os.environ.get('LOCALAPPDATA', Path.home() / 'AppData/Local'))
    elif sys.platform == 'darwin':
        root = Path.home() / 'Library/Application Support'
    else:
        root = Path(os.environ.get('XDG_DATA_HOME', Path.home() / '.local/share'))
    return root / 'FFR Helper'


def discover_saves(extra_roots=()):
    roots = [Path(p).expanduser() for p in extra_roots]
    if sys.platform == 'win32':
        roots.append(Path(os.environ.get('LOCALAPPDATA', Path.home() / 'AppData/Local')) / 'FFRS/Steam')
    else:
        for steam in [Path.home() / '.steam/steam', Path.home() / '.local/share/Steam',
                      Path.home() / 'Library/Application Support/Steam']:
            roots += [steam / 'steamapps/compatdata/3259780/pfx/drive_c/users/steamuser/AppData/Local/FFRS/Steam']
    paths = set()
    for root in roots:
        if root.is_dir():
            paths.update(p.resolve() for p in root.glob('*/GameSaveData*.sav') if p.is_file())
            paths.update(p.resolve() for p in root.glob('GameSaveData*.sav') if p.is_file())
    return sorted(paths, key=lambda p: p.stat().st_mtime, reverse=True)


def atomic_write(path: Path, data: bytes):
    path = Path(path)
    path.parent.mkdir(parents=True, exist_ok=True)
    name = None
    try:
        with tempfile.NamedTemporaryFile(dir=path.parent, prefix='.' + path.name + '.', suffix='.tmp', delete=False) as f:
            name = Path(f.name)
            f.write(data)
            f.flush()
            os.fsync(f.fileno())
        os.replace(name, path)
    finally:
        if name is not None and name.exists():
            name.unlink()


def game_is_running():
    if sys.platform == 'win32':
        import ctypes
        from ctypes import wintypes

        class ProcessEntry(ctypes.Structure):
            _fields_ = [('dwSize', wintypes.DWORD), ('cntUsage', wintypes.DWORD),
                        ('th32ProcessID', wintypes.DWORD), ('th32DefaultHeapID', ctypes.c_size_t),
                        ('th32ModuleID', wintypes.DWORD), ('cntThreads', wintypes.DWORD),
                        ('th32ParentProcessID', wintypes.DWORD), ('pcPriClassBase', wintypes.LONG),
                        ('dwFlags', wintypes.DWORD), ('szExeFile', wintypes.WCHAR * 260)]
        kernel = ctypes.WinDLL('kernel32', use_last_error=True)
        kernel.CreateToolhelp32Snapshot.argtypes = [wintypes.DWORD, wintypes.DWORD]
        kernel.CreateToolhelp32Snapshot.restype = wintypes.HANDLE
        kernel.Process32FirstW.argtypes = kernel.Process32NextW.argtypes = [wintypes.HANDLE, ctypes.POINTER(ProcessEntry)]
        kernel.CloseHandle.argtypes = [wintypes.HANDLE]
        handle = kernel.CreateToolhelp32Snapshot(2, 0)
        if handle == ctypes.c_void_p(-1).value:
            raise RuntimeError('Unable to check whether the game is running.')
        try:
            entry = ProcessEntry()
            entry.dwSize = ctypes.sizeof(entry)
            if not kernel.Process32FirstW(handle, ctypes.byref(entry)):
                raise RuntimeError('Unable to enumerate running processes.')
            while True:
                if entry.szExeFile.casefold() in ['ffrs.exe', 'ffrs-win64-shipping.exe']:
                    return True
                if not kernel.Process32NextW(handle, ctypes.byref(entry)):
                    break
            return False
        finally:
            kernel.CloseHandle(handle)
    if Path('/proc').is_dir():
        for p in Path('/proc').glob('[0-9]*/comm'):
            try:
                if p.read_text().strip().casefold().startswith('ffrs'):
                    return True
                command = (p.parent/'cmdline').read_bytes().decode(errors='replace').replace('\\', '/')
                if any(Path(part).name.casefold() in ['ffrs.exe', 'ffrs-win64-shipping.exe'] for part in command.split('\0')):
                    return True
            except (OSError, UnicodeError):
                continue
        return False
    result = subprocess.run(['ps', '-axo', 'command='], text=True, capture_output=True, check=True)
    return any(re.search(r'(?:^|[/\\\s])FFRS(?:-Win64-Shipping)?\.exe(?:[\s"\x00]|$)', line, re.I)
               for line in result.stdout.splitlines())


class CheckpointStore:
    def __init__(self, directory: Path | None = None):
        self.directory = Path(directory) if directory is not None else app_directory() / 'checkpoints'

    def capture(self, name: str, raw: bytes, source_name='', notes=''):
        SaveEditor.open(raw)
        label = re.sub(r'[^\w -]', '', name, flags=re.UNICODE).strip()[:70] or 'Checkpoint'
        now = datetime.now(timezone.utc)
        metadata = dict(format='ffr-helper-checkpoint', version=1, name=name[:150],
                        sourceName=Path(source_name).name, created=now.isoformat(), notes=notes[:4000],
                        bytes=len(raw), sha256=hashlib.sha256(raw).hexdigest(),
                        kind='normal-game-save', build=24250053)
        self.directory.mkdir(parents=True, exist_ok=True)
        path = self.directory / f'{now:%Y%m%d-%H%M%S-%f} {label}.ffrsnapshot'
        import io
        buffer = io.BytesIO()
        with zipfile.ZipFile(buffer, 'w', compression=zipfile.ZIP_DEFLATED) as archive:
            archive.writestr('metadata.json', json.dumps(metadata, ensure_ascii=False, indent=2))
            archive.writestr('checkpoint.sav', raw)
        atomic_write(path, buffer.getvalue())
        if self.read(path)[1] != raw:
            raise SaveFormatError('Checkpoint read-back failed.')
        return path

    @staticmethod
    def read(path: Path):
        if Path(path).stat().st_size > MAX_FILE:
            raise SaveFormatError('Checkpoint archive is too large.')
        with zipfile.ZipFile(path) as archive:
            if set(archive.namelist()) != {'metadata.json', 'checkpoint.sav'}:
                raise SaveFormatError('Unexpected checkpoint contents.')
            if archive.getinfo('checkpoint.sav').file_size > MAX_FILE or archive.getinfo('metadata.json').file_size > 16384:
                raise SaveFormatError('Checkpoint contents exceed size limits.')
            metadata = json.loads(archive.read('metadata.json'))
            raw = archive.read('checkpoint.sav')
        if metadata.get('format') != 'ffr-helper-checkpoint' or metadata.get('version') != 1:
            raise SaveFormatError('Unsupported checkpoint format.')
        if metadata.get('sha256') != hashlib.sha256(raw).hexdigest() or metadata.get('bytes') != len(raw):
            raise SaveFormatError('Checkpoint checksum verification failed.')
        SaveEditor.open(raw)
        return metadata, raw

    def list(self):
        if not self.directory.exists():
            return []
        return sorted(self.directory.glob('*.ffrsnapshot'), reverse=True)


def write_save(path: Path, raw: bytes, backups: CheckpointStore, *, live_slot=False):
    path = Path(path)
    checked = SaveEditor.open(raw)
    if checked.export() != raw:
        raise SaveFormatError('Output is not a stable save representation.')
    if live_slot:
        if not re.fullmatch(r'GameSaveData\d+\.sav', path.name):
            raise ValueError('Choose an ordinary GameSaveData slot.')
        if game_is_running():
            raise RuntimeError('Close the game before replacing a save slot. Exporting a separate copy is still available.')
    backup = None
    if path.exists():
        backup = backups.capture('Before replacing ' + path.stem, path.read_bytes(), path.name,
                                 'Automatic backup made before FFR Helper wrote this slot.')
    atomic_write(path, raw)
    if path.read_bytes() != raw:
        raise OSError('Save write verification failed.')
    return backup
