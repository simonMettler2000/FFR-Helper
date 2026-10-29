"""Read-only partial field recovery evidence, never an automatic rollback API."""
from __future__ import annotations

from dataclasses import dataclass
import hashlib
import io
import json
from pathlib import Path
import re
import struct
import time
import uuid
import zipfile

from .field_anchor import validate_field_anchor
from .game_build import GAME_SHA256
from .playtime import validate_playtime_basis

MAX_PROGRESSION = 16 * 1024 * 1024
MAX_METADATA = 192 * 1024
MAX_REPORT = 16384
MISSING = ['Complete world/actor/event state', 'RNG state', 'Verified restore path']
PHASES = {'idle', 'pending-write', 'captured', 'rejected', 'failed', 'write-failed', 'limit'}


def _integer(value, minimum=0, maximum=10**18-1):
    return type(value) is int and minimum <= value <= maximum


def _hex(value, size):
    return isinstance(value, str) and re.fullmatch('[0-9a-f]{'+str(size)+'}', value) is not None


def _descriptor(value, filename, maximum):
    return (isinstance(value, dict) and set(value) == {'file', 'byteLength', 'sha256'}
            and value.get('file') == filename and _integer(value.get('byteLength'), 1, maximum) and _hex(value.get('sha256'), 64))


def _json(raw, maximum):
    if type(raw) is not bytes or len(raw) > maximum:
        raise ValueError('Recovery JSON exceeds its bound or is not immutable bytes.')
    def unique(pairs):
        out = {}
        for key, value in pairs:
            if key in out:
                raise ValueError('Recovery JSON contains duplicate keys.')
            out[key] = value
        return out
    def constant(_):
        raise ValueError('Recovery JSON contains a non-finite constant.')
    try:
        return json.loads(raw, object_pairs_hook=unique, parse_constant=constant)
    except (RecursionError, UnicodeError) as error:
        raise ValueError('Recovery JSON is invalid or nested too deeply.') from error


def validate_recovery_metadata(value):
    keys = {'format', 'version', 'kind', 'readOnly', 'restorable', 'gameSha256', 'session', 'request', 'captureNumber',
            'sourceSequence', 'sourceOpeningHash', 'sourceMapId', 'captureTime', 'progression', 'fieldAnchor', 'playtimeBasis', 'coherence', 'missing'}
    if (not isinstance(value, dict) or set(value) != keys or value.get('format') != 'ffr-helper-partial-recovery'
            or type(value.get('version')) is not int or value['version'] != 1 or value.get('kind') != 'diagnostic-field-recovery'
            or value.get('readOnly') is not True or value.get('restorable') is not False or value.get('gameSha256') != GAME_SHA256
            or not _hex(value.get('session'), 32) or not _hex(value.get('request'), 32)
            or not _integer(value.get('captureNumber'), 1) or not _integer(value.get('sourceSequence'), 1)
            or not _hex(value.get('sourceOpeningHash'), 64) or not _integer(value.get('sourceMapId'), 0, 2**31-1)
            or not _integer(value.get('captureTime'), 1, 10**12) or value.get('missing') != MISSING
            or not _descriptor(value.get('progression'), 'recovery-progression.gvas', MAX_PROGRESSION)):
        raise ValueError('Invalid partial recovery metadata; this cannot be a complete restore state.')
    coherent = value.get('coherence')
    if (not isinstance(coherent, dict) or set(coherent) != {'matchingSerializations', 'stableFieldClockSamples', 'rootIdentityUnchanged', 'fieldBoundaryChecked'}
            or type(coherent.get('matchingSerializations')) is not int or coherent['matchingSerializations'] != 2
            or type(coherent.get('stableFieldClockSamples')) is not int or coherent['stableFieldClockSamples'] != 3
            or coherent.get('rootIdentityUnchanged') is not True or coherent.get('fieldBoundaryChecked') is not True):
        raise ValueError('Partial recovery coherence checks are incomplete.')
    field = validate_field_anchor(value['fieldAnchor'])
    validate_playtime_basis(value['playtimeBasis'])
    if field['mapId'] != value['sourceMapId']:
        raise ValueError('Partial recovery map differs from its retained opening.')
    raw = json.dumps(value, allow_nan=False).encode('utf8')
    if len(raw) > MAX_METADATA:
        raise ValueError('Recovery metadata exceeds its limit.')
    return _json(raw, MAX_METADATA)


def validate_recovery_report(value):
    keys = {'protocol', 'version', 'adapter', 'format', 'kind', 'readOnly', 'restorable', 'gameSha256', 'session', 'time', 'available',
            'fieldReady', 'boundaryTime', 'boundarySequence', 'boundaryOpeningHash', 'request', 'sequence', 'openingHash',
            'captureNumber', 'captureTime', 'phase', 'message', 'metadata'}
    if (not isinstance(value, dict) or set(value) != keys or type(value.get('protocol')) is not int or value['protocol'] != 1
            or type(value.get('version')) is not int or value['version'] != 1 or value.get('adapter') != 'ffr-helper-native-recovery-observer'
            or value.get('format') != 'ffr-helper-recovery-report' or value.get('kind') != 'diagnostic-observation'
            or value.get('readOnly') is not True or value.get('restorable') is not False or value.get('gameSha256') != GAME_SHA256
            or not _hex(value.get('session'), 32) or not _integer(value.get('time'), 1, 10**12)
            or type(value.get('available')) is not bool or type(value.get('fieldReady')) is not bool
            or not _integer(value.get('boundaryTime'), 0, 10**12) or value['boundaryTime'] > value['time']+2
            or not _integer(value.get('boundarySequence')) or not isinstance(value.get('phase'), str) or value['phase'] not in PHASES
            or not _integer(value.get('sequence')) or not _integer(value.get('captureNumber')) or not _integer(value.get('captureTime'), 0, 10**12)
            or not isinstance(value.get('message'), str) or len(value['message']) > 1000):
        raise ValueError('Invalid partial recovery report.')
    if (value['boundarySequence'] == 0 and value['boundaryOpeningHash'] != ''
            or value['boundarySequence'] > 0 and not _hex(value['boundaryOpeningHash'], 64)
            or value['fieldReady'] and not value['boundarySequence']):
        raise ValueError('Recovery boundary has no matched opening identity.')
    if value['phase'] == 'idle':
        if any(value[k] != zero for k, zero in [('request', ''), ('sequence', 0), ('openingHash', ''), ('captureNumber', 0), ('captureTime', 0)]) or value['metadata'] is not None:
            raise ValueError('Idle recovery report contains a captured result.')
    elif (not _hex(value['request'], 32) or not _integer(value['sequence'], 1) or not _hex(value['openingHash'], 64)
          or not _integer(value['captureNumber'], 1) or not _integer(value['captureTime'], 1, 10**12) or value['captureTime'] > value['time']+2):
        raise ValueError('Recovery receipt has invalid request identity.')
    if value['phase'] == 'captured':
        if not _descriptor(value['metadata'], 'recovery-metadata.json', MAX_METADATA):
            raise ValueError('Recovery files have no committed metadata receipt.')
    elif value['metadata'] is not None:
        raise ValueError('Uncommitted recovery cannot advertise durable files.')
    raw = json.dumps(value, allow_nan=False).encode('utf8')
    if len(raw) > MAX_REPORT:
        raise ValueError('Recovery report exceeds its bound.')
    return _json(raw, MAX_REPORT)


@dataclass(frozen=True)
class PartialRecoveryCapture:
    metadata_raw: bytes
    progression: bytes

    @property
    def metadata(self):
        return validate_recovery_metadata(_json(self.metadata_raw, MAX_METADATA))

    def validate(self):
        from .codec import SaveDocument
        metadata = self.metadata
        descriptor = metadata['progression']
        if (type(self.progression) is not bytes or len(self.progression) != descriptor['byteLength']
                or hashlib.sha256(self.progression).hexdigest() != descriptor['sha256'] or not self.progression.startswith(b'GVAS')):
            raise ValueError('Partial recovery progression failed its size/hash/source checks.')
        document = SaveDocument.decode(self.progression)
        if document.envelope is not None or document.suffix != bytes(4):
            raise ValueError('Partial recovery requires an in-memory GVAS object.')
        for name in ['player', 'unitParameterList', 'inventory', 'visionList']:
            document.root[name]
        saved = document.root['player'].struct()['PlayTime']
        if saved.kind.name != 'FloatProperty' or len(saved.raw) != 4 or struct.unpack('<f', saved.raw)[0] != metadata['playtimeBasis']['savedSeconds']:
            raise ValueError('Recovery clock does not match its serialized saved float.')
        return self


def read_live_recovery(connection: Path, report):
    report = validate_recovery_report(report)
    if report['phase'] != 'captured':
        raise ValueError('No committed partial recovery is available.')
    descriptor = report['metadata']
    with (Path(connection)/'recovery-metadata.json').open('rb') as handle:
        raw = handle.read(MAX_METADATA+1)
    if len(raw) != descriptor['byteLength'] or hashlib.sha256(raw).hexdigest() != descriptor['sha256']:
        raise ValueError('Recovery metadata changed or failed verification.')
    metadata = validate_recovery_metadata(_json(raw, MAX_METADATA))
    pairs = [('session', 'session'), ('request', 'request'), ('captureNumber', 'captureNumber'), ('captureTime', 'captureTime'),
             ('sequence', 'sourceSequence'), ('openingHash', 'sourceOpeningHash')]
    if any(report[a] != metadata[b] for a, b in pairs):
        raise ValueError('Recovery files belong to another request or opening.')
    with (Path(connection)/'recovery-progression.gvas').open('rb') as handle:
        progression = handle.read(MAX_PROGRESSION+1)
    return PartialRecoveryCapture(raw, progression).validate()


def export_recovery(path: Path, capture: PartialRecoveryCapture):
    from .storage import atomic_write
    if not str(path).lower().endswith('.ffrrecovery'):
        raise ValueError('Use .ffrrecovery; this partial diagnostic cannot be loaded as a save state.')
    capture.validate()
    buffer = io.BytesIO()
    with zipfile.ZipFile(buffer, 'w', zipfile.ZIP_DEFLATED) as archive:
        archive.writestr('recovery-metadata.json', capture.metadata_raw)
        archive.writestr('recovery-progression.gvas', capture.progression)
    atomic_write(Path(path), buffer.getvalue())


def load_recovery(path: Path):
    maximum = MAX_PROGRESSION+MAX_METADATA+65536
    with Path(path).open('rb') as handle:
        raw = handle.read(maximum+1)
    if len(raw) > maximum:
        raise ValueError('Partial recovery archive exceeds its limit.')
    with zipfile.ZipFile(io.BytesIO(raw)) as archive:
        entries = archive.infolist()
        if len(entries) != 2 or sorted(e.filename for e in entries) != ['recovery-metadata.json', 'recovery-progression.gvas']:
            raise ValueError('Recovery archive has unexpected or duplicate entries.')
        for entry in entries:
            limit = MAX_METADATA if entry.filename == 'recovery-metadata.json' else MAX_PROGRESSION
            if entry.file_size > limit or entry.flag_bits & 1:
                raise ValueError('Recovery archive entry is oversized or encrypted.')
        return PartialRecoveryCapture(archive.read('recovery-metadata.json'), archive.read('recovery-progression.gvas')).validate()


class PartialRecoveryBridge:
    """One explicit read-only request; no abort, import, restart or restore dispatch."""
    def __init__(self, recorder):
        self.recorder = recorder
        self.request = None
        self.source = None

    def status(self):
        try:
            report = validate_recovery_report(self.recorder._read('recovery-report.json', MAX_REPORT))
            if report['session'] == self.recorder.session and -2 <= time.time()-report['time'] <= 5:
                return report
        except (OSError, ValueError, TypeError, KeyError, RecursionError):
            pass
        return None

    @staticmethod
    def safe_boundary(report, sequence, digest):
        return (report is not None and report.get('available') is True and report.get('fieldReady') is True
                and _integer(sequence, 1) and _hex(digest, 64) and _integer(report.get('boundaryTime'), 1, 10**12)
                and -2 <= time.time()-report['boundaryTime'] <= 2
                and report.get('boundarySequence') == sequence and report.get('boundaryOpeningHash') == digest)

    def send(self, opening_report):
        from .storage import atomic_write
        if self.request is not None:
            raise ValueError('A partial recovery was already requested; no automatic retry is allowed.')
        source = self.recorder.opening(opening_report)
        report = self.status()
        if (source is None or source.metadata.get('fieldAnchor') is None or source.metadata.get('playtimeBasis') is None
                or not self.safe_boundary(report, source.metadata['sequence'], source.metadata['sha256'])):
            raise ValueError('Wait at the matching cleared field boundary with captured field/clock metadata.')
        self.source = source
        self.request = dict(nonce=uuid.uuid4().hex, deadline=int(time.time())+8, sequence=source.metadata['sequence'], sha256=source.metadata['sha256'])
        request = self.request
        text = ('FFR_HELPER_PARTIAL_RECOVERY_V1\n'+self.recorder.session+'\n'+request['nonce']+'\n'+str(request['deadline'])+'\n'
                +str(request['sequence'])+'\n'+request['sha256']+'\n')
        atomic_write(self.recorder.connection/'recovery-request.txt', text.encode('ascii'))
        return source

    def receipt(self):
        report = self.status()
        if (report is None or self.request is None or report['request'] != self.request['nonce']
                or report['sequence'] != self.request['sequence'] or report['openingHash'] != self.request['sha256'] or report['phase'] == 'idle'):
            return None
        return report if -2 <= time.time()-report['captureTime'] <= 5 else None

    def capture_receipt(self, report):
        report = validate_recovery_report(report)
        current = self.receipt()
        identity = ['session', 'request', 'sequence', 'openingHash', 'captureNumber', 'captureTime', 'metadata', 'phase']
        if current is None or report['phase'] != 'captured' or any(current[key] != report[key] for key in identity):
            raise ValueError('A fresh matching committed recovery receipt is required.')
        capture = read_live_recovery(self.recorder.connection, report)
        metadata = capture.metadata
        if self.source is None or metadata['sourceMapId'] != self.source.metadata['fieldAnchor']['mapId']:
            raise ValueError('Recovery map does not match the retained source opening.')
        after = self.receipt()
        if after is None or any(after[key] != report[key] for key in identity):
            raise ValueError('Recovery receipt changed while its files were read.')
        return capture
