import copy
import struct
import unittest

from ffr_helper.load_bindings import BindingSnapshotError, read_name, snapshot_bindings


class Memory:
    def __init__(self):
        self.parts = {}
        self.reads = 0

    def read(self, address, size):
        self.reads += 1
        for start, value in self.parts.items():
            if start <= address and address + size <= start + len(value):
                return bytes(value[address-start:address-start+size])
        return b''

    def registry(self):
        header = bytearray(0x70)
        struct.pack_into('<Qii', header, 0, 0x20000, 3, 3)
        struct.pack_into('<i', header, 0x34, 1)  # Slot1 is a hole with garbage.
        struct.pack_into('<Qi', header, 0x40, 0x30000, 2)
        rows = bytearray(b'\xff' * 96)
        struct.pack_into('<QQiiii', rows, 0, 17, 0x40000, 2, 2, 2, 0)
        struct.pack_into('<QQiiii', rows, 64, 23, 0, 0, 0, -1, 0)
        self.parts = {0x10030: header, 0x20000: rows,
                      0x30000: bytearray(struct.pack('<ii', 0, -1)),
                      0x40000: bytearray(struct.pack('<iiQiiQ', 12, 99, 31, -1, 0, 0))}
        return self


class LoadBindingTests(unittest.TestCase):
    def test_sparse_chains_and_stale_weak_references_are_copied_without_invocation(self):
        memory = Memory().registry()
        original = copy.deepcopy(memory.parts)
        report = snapshot_bindings(memory.read, 0x10000)
        self.assertEqual([e['eventName'] for e in report['events']], [17, 23])
        self.assertEqual(report['events'][0]['bindings'][1],
                         dict(objectIndex=-1, objectSerial=0, functionName=0))
        self.assertEqual(memory.parts, original)
        self.assertFalse(report['sideEffectsVerified'] or report['completeConsumerCoverage'])

    def test_bad_chains_and_sparse_accounting_are_refused(self):
        for address, offset, value in [(0x20000, 88, 0), (0x30000, 0, 9),
                                       (0x30000, 4, 2), (0x10030, 0x34, 0),
                                       (0x10030, 0x48, 3)]:
            with self.subTest(address=address, offset=offset):
                memory = Memory().registry()
                struct.pack_into('<i', memory.parts[address], offset, value)
                with self.assertRaises(BindingSnapshotError):
                    snapshot_bindings(memory.read, 0x10000)

    def test_duplicate_event_and_active_or_deferred_dispatch_are_refused(self):
        for address, offset, value in [(0x20000, 64, 17), (0x10030, 0x58, 1),
                                       (0x10030, 0x68, 1)]:
            memory = Memory().registry()
            struct.pack_into('<i', memory.parts[address], offset, value)
            with self.assertRaises(BindingSnapshotError):
                snapshot_bindings(memory.read, 0x10000)

    def test_binding_payload_drift_is_refused_even_if_headers_match(self):
        memory = Memory().registry()
        def changing(address, size):
            value = memory.read(address, size)
            if address == 0x40000:
                memory.parts[address][0] ^= 1
            return value
        with self.assertRaisesRegex(BindingSnapshotError, 'changed'):
            snapshot_bindings(changing, 0x10000)

    def test_bad_bounds_and_incomplete_reads_are_refused(self):
        for pointer in [0, 0xffff, 0x7fffffffffff, True]:
            with self.assertRaises(BindingSnapshotError):
                snapshot_bindings(lambda *_: b'', pointer)
        memory = Memory().registry()
        struct.pack_into('<i', memory.parts[0x20000], 16, 513)
        with self.assertRaises(BindingSnapshotError):
            snapshot_bindings(memory.read, 0x10000)

    def test_ansi_wide_and_numbered_names(self):
        memory = Memory()
        for name, wide in [('Delegate.System.SaveData.Update', False), ('Example', True)]:
            text = name.encode('utf-16le' if wide else 'ascii')
            memory.parts = {0x50010: struct.pack('<Q', 0x60000),
                            0x60020: struct.pack('<H', len(name) << 6 | wide) + text}
            self.assertEqual(read_name(memory.read, 0x50000, 16 | 3 << 32), name + '_2')

    def test_name_cannot_cross_a_block_or_claim_unbounded_length(self):
        memory = Memory()
        memory.parts = {0x50010: struct.pack('<Q', 0x60000),
                        0x7fffe: struct.pack('<H', 64)}
        with self.assertRaises(BindingSnapshotError):
            read_name(memory.read, 0x50000, 0xffff)

    def test_name_block_replacement_is_not_accepted_as_stable_text(self):
        memory = Memory()
        memory.parts = {0x50010: struct.pack('<Q', 0x60000),
                        0x60020: struct.pack('<H', 64) + b'A'}
        def changing(address, size):
            value = memory.read(address, size)
            if address == 0x60020 and size == 3:
                memory.parts[0x50010] = struct.pack('<Q', 0x70000)
            return value
        with self.assertRaisesRegex(BindingSnapshotError, 'changed'):
            read_name(changing, 0x50000, 16)
