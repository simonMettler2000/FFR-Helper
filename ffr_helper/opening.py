"""Verified encounter-opening captures, deliberately not restorable save states."""
from __future__ import annotations

from dataclasses import dataclass
import hashlib
import io
import json
from pathlib import Path
import re
import struct
import zipfile

from .codec import SaveDocument
from .game_build import GAME_SHA256
from .field_anchor import validate_field_anchor
from .playtime import validate_playtime_basis
from .storage import atomic_write

MAX_PROGRESSION = 16 * 1024 * 1024
MAX_METADATA = 192 * 1024
MISSING = ['Verified live resource/progression coverage', 'Full combat and RNG state', 'Verified restoration path']


def _metadata(value):
    if not isinstance(value, dict):
        raise ValueError('No serialized encounter opening is available.')
    if (value.get('capturePoint') != 'before-native-entry' or value.get('gameSha256') != GAME_SHA256
            or value.get('file') != 'opening-progression.gvas'):
        raise ValueError('Unsupported encounter-opening source or game build.')
    if (type(value.get('sequence')) is not int or value['sequence'] < 1
            or type(value.get('time')) is not int or value['time'] < 1
            or type(value.get('byteLength')) is not int or not 8 <= value['byteLength'] <= MAX_PROGRESSION
            or not isinstance(value.get('sha256'), str) or re.fullmatch('[0-9a-f]{64}', value['sha256']) is None
            or not isinstance(value.get('entry'), dict)):
        raise ValueError('Invalid encounter-opening metadata.')
    anchor = value.get('fieldAnchor')
    if anchor is not None:
        validate_field_anchor(anchor)
    if value.get('playtimeBasis') is not None:
        validate_playtime_basis(value['playtimeBasis'])
    raw = json.dumps(value, ensure_ascii=False, allow_nan=False).encode('utf8')
    if len(raw) > MAX_METADATA:
        raise ValueError('Encounter-opening metadata exceeds its limit.')
    return json.loads(raw)


@dataclass(frozen=True)
class OpeningCapture:
    metadata: dict
    progression: bytes

    def validate(self):
        metadata = _metadata(self.metadata)
        raw = self.progression
        if (not isinstance(raw, bytes) or len(raw) != metadata['byteLength']
                or hashlib.sha256(raw).hexdigest() != metadata['sha256']):
            raise ValueError('Opening progression changed or its checksum does not match. Capture a new report.')
        if not raw.startswith(b'GVAS'):
            raise ValueError('Opening progression must be an in-memory GVAS object.')
        document = SaveDocument.decode(raw)
        if document.envelope is not None or document.suffix != bytes(4):
            raise ValueError('Unexpected trailing data in the serialized opening.')
        # Existence/uniqueness is required; unknown properties remain untouched.
        for name in ['player', 'unitParameterList', 'inventory', 'visionList']:
            document.root[name]
        if metadata.get('playtimeBasis') is not None:
            try:
                saved = document.root['player'].struct()['PlayTime']
            except (KeyError, ValueError) as error:
                raise ValueError('Playtime basis requires a matching serialized Player.PlayTime.') from error
            if (saved.kind.name != 'FloatProperty' or len(saved.raw) != 4
                    or struct.unpack('<f', saved.raw)[0] != metadata['playtimeBasis']['savedSeconds']):
                raise ValueError('Playtime basis does not match serialized Player.PlayTime.')
        return self


def read_live_opening(connection: Path, report: dict):
    """Read one fixed sidecar and bind it to the matched report's hash/revision."""
    if (report.get('adapter') != 'ffr-helper-native-entry-observer' or report.get('restorable') is not False
            or report.get('readOnly') is not True or report.get('kind') != 'diagnostic-observation'):
        raise ValueError('Serialized openings require the native capture producer.')
    value = report.get('objects', {}).get('nativeEntry', {}).get('opening')
    if value is None:
        return None
    metadata = _metadata(value)
    if metadata['time'] > report['time'] + 2:
        raise ValueError('Encounter opening is newer than its diagnostic report.')
    # Never open a path supplied by game-side metadata.
    with (Path(connection) / 'opening-progression.gvas').open('rb') as handle:
        raw = handle.read(MAX_PROGRESSION + 1)
    return OpeningCapture(metadata, raw).validate()


def export_opening(path: Path, capture: OpeningCapture):
    if not str(path).lower().endswith('.ffropening'):
        raise ValueError('Use .ffropening; this capture cannot be loaded as a save state.')
    capture.validate()
    manifest = dict(format='ffr-helper-opening-capture', version=1, kind='diagnostic-opening',
                    restorable=False, missingState=MISSING, opening=capture.metadata)
    raw = json.dumps(manifest, ensure_ascii=False, allow_nan=False).encode('utf8')
    if len(raw) > MAX_METADATA:
        raise ValueError('Opening manifest exceeds its limit.')
    buffer = io.BytesIO()
    with zipfile.ZipFile(buffer, 'w', zipfile.ZIP_DEFLATED) as archive:
        archive.writestr('manifest.json', raw)
        archive.writestr('progression.gvas', capture.progression)
    atomic_write(Path(path), buffer.getvalue())


def load_opening(path: Path):
    """Inspect an archive without extracting files or invoking the game."""
    with Path(path).open('rb') as handle:
        raw = handle.read(MAX_PROGRESSION + MAX_METADATA + 65537)
    if len(raw) > MAX_PROGRESSION + MAX_METADATA + 65536:
        raise ValueError('Opening archive exceeds its limit.')
    with zipfile.ZipFile(io.BytesIO(raw)) as archive:
        entries = archive.infolist()
        if len(entries) != 2 or sorted(e.filename for e in entries) != ['manifest.json', 'progression.gvas']:
            raise ValueError('Opening archive contains unexpected or duplicate entries.')
        for entry in entries:
            limit = MAX_METADATA if entry.filename == 'manifest.json' else MAX_PROGRESSION
            if entry.file_size > limit or entry.flag_bits & 1:
                raise ValueError('Opening archive entry is oversized or encrypted.')
        manifest = json.loads(archive.read('manifest.json'))
        if (not isinstance(manifest, dict) or manifest.get('format') != 'ffr-helper-opening-capture'
                or manifest.get('version') != 1 or manifest.get('kind') != 'diagnostic-opening'
                or manifest.get('restorable') is not False or manifest.get('missingState') != MISSING):
            raise ValueError('Opening captures are incomplete diagnostics, not restorable states.')
        return OpeningCapture(_metadata(manifest.get('opening')), archive.read('progression.gvas')).validate()
