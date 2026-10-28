"""File-based lease for the optional UE4SS field-save adapter."""
import json
from pathlib import Path
import shutil
import time
import uuid

from .storage import atomic_write


class FieldSaveBridge:
    def __init__(self, folder: Path):
        self.folder = Path(folder)
        self.connection = self.folder / 'connection'
        self.session = uuid.uuid4().hex
        self.requested = False

    def heartbeat(self):
        if not self.connection.is_dir():
            raise ValueError('Choose an installed FFRHelper adapter directory containing connection/.')
        message = f'FFR_HELPER_CONTROL_V1\n{self.session}\n{int(time.time())+5}\n{int(self.requested)}\n'
        atomic_write(self.connection / 'control.txt', message.encode('ascii'))

    def status(self):
        try:
            path = self.connection / 'status.json'
            if path.stat().st_size > 4096:
                return None
            data = json.loads(path.read_text(encoding='utf8'))
            age = time.time() - data['time']
            if data.get('protocol') != 1 or data.get('adapter') != 'ffr-helper-field-save' or not -2 <= age <= 3:
                return None
            if data.get('session') != self.session:
                return None
            if type(data.get('ready')) is not bool or type(data.get('enabled')) is not bool:
                return None
            return data
        except (OSError, ValueError, KeyError, TypeError):
            return None

    def stop(self):
        self.requested = False
        self.heartbeat()


def prepare_adapter(mods_directory: Path):
    mods_directory = Path(mods_directory)
    if mods_directory.name.casefold() != 'mods' or not mods_directory.is_dir():
        raise ValueError('Choose an existing UE4SS Mods directory.')
    target = mods_directory / 'FFRHelper'
    if target.exists():
        raise ValueError('An FFRHelper directory already exists. Connect it, or move it aside before installing a new copy.')
    source = Path(__file__).resolve().parents[1] / 'bridge' / 'FFRHelper'
    if not source.is_dir():
        # PyInstaller's bundled optional data.
        import sys
        source = Path(getattr(sys, '_MEIPASS', '')) / 'bridge' / 'FFRHelper'
    if not source.is_dir():
        raise ValueError('The optional adapter files are missing from this package.')
    shutil.copytree(source, target)
    (target / 'connection').mkdir()
    (target / 'enabled.txt').write_text('', encoding='utf8')
    return target
