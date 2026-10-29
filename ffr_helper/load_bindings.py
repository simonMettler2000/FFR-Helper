"""Bounded read-only snapshot of the profiled named script-delegate registry.

Internal research data: addresses and unresolved weak references are private.
The caller must establish the exact game/code identity and matching field first.
This copies bindings, never invokes them and does not prove their side effects.
"""
from __future__ import annotations

import struct


class BindingSnapshotError(ValueError):
    pass


def snapshot_bindings(read, manager: int) -> dict:
    """Copy live hash chains, including holes and stale delegate references.

    Layout follows build24250053's named dispatcher and script broadcast:
    32-byte sparse rows, a 16-byte multicast array and 16-byte script bindings.
    A second read rejects drift; this is still not an atomic engine snapshot.
    """
    if type(manager) is not int or not 0x10000 <= manager < 0x7fffffffff00:
        raise BindingSnapshotError('Invalid registry owner')
    maximum_slots = 4096
    maximum_bindings = 8192
    budget = 1024 * 1024
    segments = {}

    def copy(address, size):
        nonlocal budget
        if (type(address) is not int or type(size) is not int
                or not 0x10000 <= address < 0x7fffffffffff
                or size < 0 or size > budget or address + size >= 0x7fffffffffff):
            raise BindingSnapshotError('Unreadable or over-budget registry range')
        if not size:
            return b''
        value = read(address, size)
        if not isinstance(value, bytes) or len(value) != size:
            raise BindingSnapshotError('Incomplete registry read')
        budget -= size
        key = address, size
        if key in segments and segments[key] != value:
            raise BindingSnapshotError('Registry changed during capture')
        segments[key] = value
        return value

    def array(raw, offset, limit):
        data, count, capacity = struct.unpack_from('<Qii', raw, offset)
        if not 0 <= count <= capacity <= limit or (count and data < 0x10000):
            raise BindingSnapshotError('Malformed registry array')
        return data, count, capacity

    header = copy(manager + 0x30, 0x70)
    storage, count, _ = array(header, 0, maximum_slots)
    free = struct.unpack_from('<i', header, 0x34)[0]
    bucket_storage, bucket_count = struct.unpack_from('<Qi', header, 0x40)
    if not 0 <= free <= count:
        raise BindingSnapshotError('Invalid sparse free count')
    for offset in [0x50, 0x60]:
        if array(header, offset, maximum_slots)[1]:
            raise BindingSnapshotError('Delegate dispatch or deferred changes are active')
    if bucket_count == 0 and count == free == 0:
        result = []
    else:
        if not 0 < bucket_count <= maximum_slots or bucket_count & (bucket_count - 1):
            raise BindingSnapshotError('Invalid hash bucket count')
        if not bucket_storage and bucket_count != 1:
            raise BindingSnapshotError('Inline hash storage supports one bucket only')
        heads = copy(bucket_storage or manager + 0x68, 4 * bucket_count)
        rows = copy(storage, 32 * count) if count else b''
        seen = set()
        keys = set()
        result = []
        total = 0
        for (head,) in struct.iter_unpack('<i', heads):
            index = head
            while index != -1:
                if not 0 <= index < count or index in seen:
                    raise BindingSnapshotError('Invalid, repeated or cyclic registry chain')
                seen.add(index)
                offset = index * 32
                name = struct.unpack_from('<Q', rows, offset)[0]
                if name in keys:
                    raise BindingSnapshotError('Duplicate registry key')
                keys.add(name)
                data, listeners, _ = array(rows, offset + 8, 512)
                total += listeners
                if total > maximum_bindings:
                    raise BindingSnapshotError('Too many aggregate script bindings')
                payload = copy(data, 16 * listeners) if listeners else b''
                bindings = [dict(objectIndex=a, objectSerial=b, functionName=c)
                            for a, b, c in struct.iter_unpack('<iiQ', payload)]
                result.append(dict(eventName=name, bindings=bindings))
                index = struct.unpack_from('<i', rows, offset + 24)[0]
        if len(seen) != count - free:
            raise BindingSnapshotError('Hash chains do not cover every occupied sparse row')
    # Include every array/row/header, not just the root pointer and counts.
    for (address, size), value in segments.items():
        if read(address, size) != value:
            raise BindingSnapshotError('Registry changed before capture completed')
    return dict(readOnly=True, gameCalls=False, gameWrites=False, atomicSnapshot=False,
                sideEffectsVerified=False, completeConsumerCoverage=False,
                events=result)


def read_name(read, pool: int, value: int) -> str:
    """Decode the profiled FName blocks without calling/initializing the pool."""
    if type(value) is not int or not 0 <= value < 2**64:
        raise BindingSnapshotError('Invalid FName')
    identity, number = value & 0xffffffff, value >> 32
    block, offset = identity >> 16, (identity & 0xffff) * 2
    if block >= 8192 or type(pool) is not int or not 0x10000 <= pool < 0x7ffffffffeff:
        raise BindingSnapshotError('FName is outside the profiled pool')
    pointer = read(pool + 0x10 + block * 8, 8)
    if not isinstance(pointer, bytes) or len(pointer) != 8:
        raise BindingSnapshotError('Name block unavailable')
    address = struct.unpack('<Q', pointer)[0]
    if not 0x10000 <= address < 0x7fffffffffff:
        raise BindingSnapshotError('Name block unavailable')
    raw = read(address + offset, 2)
    if not isinstance(raw, bytes) or len(raw) != 2:
        raise BindingSnapshotError('Name header unavailable')
    header = struct.unpack('<H', raw)[0]
    length, wide = header >> 6, header & 1
    size = length * (2 if wide else 1)
    if not 0 < length <= 1023 or offset + 2 + size > 131072:
        raise BindingSnapshotError('Name exceeds its bounded block')
    text = read(address + offset + 2, size)
    if not isinstance(text, bytes) or len(text) != size:
        raise BindingSnapshotError('Name text unavailable')
    try:
        decoded = text.decode('utf-16le' if wide else 'ascii')
    except UnicodeError as error:
        raise BindingSnapshotError('Unsupported name encoding') from error
    if ('\0' in decoded or read(address + offset, 2 + size) != raw + text
            or read(pool + 0x10 + block * 8, 8) != pointer):
        raise BindingSnapshotError('Name changed or contains a terminator')
    return decoded + (f'_{number-1}' if number else '')
