"""Read-only native-runtime recorder transport. This is not a save-state file."""
from __future__ import annotations

import json
from pathlib import Path
import shutil
import sys
import time
import uuid

from .storage import atomic_write

MAX_REPORT = 512 * 1024
RECORDER_NAME = 'FFRHelperRuntime'


class RuntimeRecorder:
    """Only requests observation; the protocol has no mutation/restore command."""

    def __init__(self, directory: Path):
        self.directory = Path(directory)
        self.connection = self.directory / 'connection'
        if not self.connection.is_dir():
            raise ValueError('Choose an FFRHelperRuntime directory containing connection/.')
        self.session = uuid.uuid4().hex
        self.request = '0' * 32

    def heartbeat(self):
        message = f'FFR_HELPER_OBSERVER_V1\n{self.session}\n{int(time.time()) + 5}\n{self.request}\n'
        atomic_write(self.connection / 'observe.txt', message.encode('ascii'))

    def capture(self):
        self.request = uuid.uuid4().hex
        self.heartbeat()
        return self.request

    def disconnect(self):
        message = f'FFR_HELPER_OBSERVER_V1\n{self.session}\n0\n{self.request}\n'
        atomic_write(self.connection / 'observe.txt', message.encode('ascii'))

    def status(self):
        try:
            data = self._read('observer-status.json', 8192)
            if not self._current(data) or type(data.get('hooks')) is not dict:
                return None
            if type(data.get('readOnly')) is not bool or data['readOnly'] is not True:
                return None
            if not isinstance(data.get('message'), str) or len(data['message']) > 1000:
                return None
            return data
        except (OSError, ValueError, TypeError, KeyError, RecursionError):
            return None

    def report(self):
        """Match the current request, not an old report left by a previous game."""
        try:
            data = self._read('runtime-report.json', MAX_REPORT)
            if not self._current(data) or data.get('request') != self.request:
                return None
            return validate_report(data)
        except (OSError, ValueError, TypeError, KeyError, RecursionError):
            return None

    def _current(self, data):
        if not isinstance(data, dict):
            return False
        timestamp = data.get('time')
        return (data.get('protocol') == 1 and data.get('adapter') == 'ffr-helper-runtime-observer'
                and data.get('session') == self.session and type(timestamp) is int
                and -2 <= time.time() - timestamp <= 5)

    def _read(self, name, maximum):
        with (self.connection / name).open('rb') as handle:
            raw = handle.read(maximum + 1)
        if len(raw) > maximum:
            raise ValueError('Recorder output exceeds its size limit.')
        return json.loads(raw)


def validate_report(data):
    if not isinstance(data, dict) or data.get('kind') != 'diagnostic-observation':
        raise ValueError('This is not a native runtime observation.')
    if data.get('format') != 'ffr-helper-runtime-report' or data.get('version') != 1:
        raise ValueError('Unsupported runtime report version.')
    # Never accept a diagnostic as a restorable state, even if someone edits it.
    if data.get('restorable') is not False or data.get('readOnly') is not True:
        raise ValueError('Runtime observations cannot be loaded as save states.')
    if not isinstance(data.get('objects'), dict) or not isinstance(data.get('events'), list):
        raise ValueError('Invalid runtime observation.')
    if len(data['objects']) > 8 or len(data['events']) > 32:
        raise ValueError('Runtime observation exceeds its collection limits.')
    if not isinstance(data.get('missingState'), list) or not data['missingState']:
        raise ValueError('Runtime observation must state its incomplete coverage.')
    if data.get('protocol') != 1 or data.get('adapter') != 'ffr-helper-runtime-observer':
        raise ValueError('Unexpected runtime report producer.')
    raw = json.dumps(data, ensure_ascii=False, allow_nan=False).encode('utf8')
    if len(raw) > MAX_REPORT:
        raise ValueError('Runtime report is too large.')
    return data


def export_report(path: Path, report):
    validate_report(report)
    if not str(path).lower().endswith('.ffrruntime.json'):
        raise ValueError('Use .ffrruntime.json; observations are not game saves or checkpoints.')
    atomic_write(Path(path), json.dumps(report, ensure_ascii=False, allow_nan=False, indent=2).encode('utf8'))


def prepare_recorder(mods_directory: Path):
    mods_directory = Path(mods_directory)
    if mods_directory.name.casefold() != 'mods' or not mods_directory.is_dir():
        raise ValueError('Choose an existing UE4SS Mods directory.')
    target = mods_directory / RECORDER_NAME
    if target.exists():
        raise ValueError('An FFRHelperRuntime recorder already exists. Connect it or move it aside first.')
    source = Path(__file__).resolve().parents[1] / 'bridge' / RECORDER_NAME
    if not source.is_dir():
        source = Path(getattr(sys, '_MEIPASS', '')) / 'bridge' / RECORDER_NAME
    if not source.is_dir():
        raise ValueError('Runtime recorder files are missing from this package.')
    shutil.copytree(source, target)
    (target / 'connection').mkdir()
    (target / 'enabled.txt').write_text('', encoding='ascii')
    return target
