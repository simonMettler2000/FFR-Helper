"""Offline preview of normal-load playtime normalization; never an apply command."""
from __future__ import annotations

from dataclasses import dataclass
import hashlib
import json
import math
from pathlib import Path
import struct

from .codec import SaveDocument
from .game_build import GAME_SHA256
from .playtime import LIMIT
from .storage import atomic_write


def nearest_float32(value):
    """Round a nonnegative normal binary64 value to binary32, explicitly ties-even."""
    if value == 0:
        return 0.0
    if not math.isfinite(value) or not 2**-126 <= value <= LIMIT:
        raise ValueError('Playtime normalization requires bounded normal float values.')
    raw = struct.unpack('<Q', struct.pack('<d', value))[0]
    exponent = ((raw >> 52) & 0x7ff) - 1023
    significand = (raw & ((1 << 52) - 1)) | (1 << 52)
    rounded, remainder = divmod(significand, 1 << 29)
    if remainder > 1 << 28 or remainder == 1 << 28 and rounded & 1:
        rounded += 1
    if rounded == 1 << 24:
        rounded >>= 1
        exponent += 1
    if not -126 <= exponent <= 127:
        raise ValueError('Playtime normalization exceeds binary32 bounds.')
    result = struct.unpack('<f', struct.pack('<I', ((exponent + 127) << 23) | (rounded & 0x7fffff)))[0]
    if result > LIMIT:
        raise ValueError('Normalized playtime exceeds its supported bound.')
    return result


@dataclass(frozen=True)
class PlaytimeNormalizationPlan:
    source_sha256: str
    source_metadata_sha256: str
    normalized_progression: bytes
    saved_seconds: float
    elapsed_seconds: int
    rounded_elapsed_seconds: float
    normalized_saved_seconds: float

    def report(self):
        return dict(format='ffr-helper-playtime-plan', version=1, kind='offline-normalization-preview', restorable=False,
                    gameSha256=GAME_SHA256,
                    sourceSha256=self.source_sha256, sourceMetadataSha256=self.source_metadata_sha256,
                    targetSha256=hashlib.sha256(self.normalized_progression).hexdigest(),
                    targetByteLength=len(self.normalized_progression), roundingMode='nearest-even',
                    savedSeconds=self.saved_seconds, elapsedSeconds=self.elapsed_seconds,
                    roundedElapsedSeconds=self.rounded_elapsed_seconds, normalizedSavedSeconds=self.normalized_saved_seconds,
                    changedProperty='Player.PlayTime', originalCapturePreserved=True,
                    missing=['Native clone application and current rounding-mode verification',
                             'Verified clock reset, cache and world restoration'])


def plan_opening_playtime(capture):
    from .opening import OpeningCapture
    if not isinstance(capture, OpeningCapture):
        raise ValueError('A verified opening capture is required for playtime planning.')
    capture.validate()
    basis = capture.metadata.get('playtimeBasis')
    if not basis or basis.get('roundingMode') != 'nearest-even':
        raise ValueError('Capture a live playtime basis with verified nearest-even native rounding first.')
    if basis['suspendedStartSeconds']:
        raise ValueError('Normalization of an ongoing suspension is unsupported; no target was prepared.')
    saved = float(basis['savedSeconds'])
    if 0 < saved < 2**-126:
        raise ValueError('Subnormal saved playtime requires additional native FTZ/DAZ verification.')
    elapsed = nearest_float32(float(basis['elapsedSeconds']))
    normalized = nearest_float32(saved + elapsed)
    document = SaveDocument.decode(capture.progression)
    if document.prefix + document.root.encode() + document.suffix != capture.progression:
        raise ValueError('The original progression must round-trip byte-identically before planning.')
    prop = document.root['player'].struct()['PlayTime']
    prop.raw = struct.pack('<f', normalized)
    target = document.prefix + document.root.encode() + document.suffix
    if len(target) != len(capture.progression):
        raise ValueError('Normalization must preserve the progression layout.')
    # This prospective payload is distinct from the immutable opening source.
    return PlaytimeNormalizationPlan(capture.metadata['sha256'],
                                    hashlib.sha256(json.dumps(capture.metadata, sort_keys=True, allow_nan=False,
                                                              separators=(',', ':')).encode('utf8')).hexdigest(),
                                    target, saved, basis['elapsedSeconds'], elapsed, normalized)


def export_playtime_plan(path: Path, plan: PlaytimeNormalizationPlan):
    if not str(path).lower().endswith('.ffrplaytime.json'):
        raise ValueError('Use .ffrplaytime.json; this preview is not a game save or restore state.')
    # Only the small receipt is exported. Prospective GVAS bytes never replace
    # the opening archive or a game slot, and no native command consumes them.
    raw = json.dumps(plan.report(), indent=2, allow_nan=False).encode('utf8')
    atomic_write(Path(path), raw)
