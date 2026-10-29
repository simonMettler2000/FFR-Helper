"""Bounded live clock evidence; never interpreted as a restorable timer state."""
from __future__ import annotations

import math

LIMIT = 10**12
FIELDS = {'kind', 'restorable', 'source', 'coverage', 'sameRootChecked', 'elapsedDefinition',
          'savedSeconds', 'startSeconds', 'suspendedStartSeconds', 'totalSuspendedSeconds',
          'sampleSeconds', 'elapsedSeconds'}


def validate_playtime_basis(value):
    if (not isinstance(value, dict) or set(value) not in (FIELDS, FIELDS | {'roundingMode'})
            or value.get('kind') != 'partial-playtime-basis' or value.get('restorable') is not False
            or value.get('source') != 'live-playtime-subsystem' or value.get('coverage') != 'saved-base-and-clock-only'
            or value.get('sameRootChecked') is not True
            or value.get('elapsedDefinition') != 'sample-minus-start-minus-completed-suspension'):
        raise ValueError('Invalid partial playtime basis; this is not a restorable timer state.')
    if 'roundingMode' in value and (not isinstance(value['roundingMode'], str)
                                   or value['roundingMode'] not in ('nearest-even', 'downward', 'upward', 'toward-zero')):
        raise ValueError('Invalid native rounding mode in partial playtime basis.')
    saved = value['savedSeconds']
    if type(saved) not in (int, float) or not 0 <= saved <= LIMIT or not math.isfinite(saved):
        raise ValueError('Invalid saved seconds in partial playtime basis.')
    for key in ['startSeconds', 'suspendedStartSeconds', 'totalSuspendedSeconds', 'sampleSeconds', 'elapsedSeconds']:
        if type(value[key]) is not int or not 0 <= value[key] <= LIMIT:
            raise ValueError('Invalid clock seconds in partial playtime basis.')
    start, sample, suspended, total = (value[k] for k in
                                     ['startSeconds', 'sampleSeconds', 'suspendedStartSeconds', 'totalSuspendedSeconds'])
    if (not 0 < start <= sample or total > sample - start or suspended != 0 and not start <= suspended <= sample
            or value['elapsedSeconds'] != sample - start - total):
        raise ValueError('Inconsistent partial playtime basis.')
    return dict(value)
