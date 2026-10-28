import hashlib
import os
from pathlib import Path
import struct
import tempfile
import unittest
from unittest.mock import patch

from ffr_helper.codec import (FORMAT_KEY, NONE, Property, PropertyList, Reader, SaveDocument,
                              SaveFormatError, TypeNode, fstring, i32, integer_property)
from ffr_helper.editor import SaveEditor
from ffr_helper.progression import level_at, reference_data, route_totals
from ffr_helper.storage import CheckpointStore, write_save
from cryptography.hazmat.primitives.ciphers import Cipher, algorithms, modes


def prop(name, kind, raw):
    prefix = fstring(name) + kind.encode()
    return Property(name, kind, prefix + i32(len(raw)) + b'\0', len(prefix), raw)


def struct_type(name):
    return TypeNode('StructProperty', (TypeNode(name, (TypeNode('/Script/mucha'),)),))


def struct_prop(name, typename, properties):
    return prop(name, struct_type(typename), PropertyList(properties).encode())


def array_prop(name, typename, records):
    return prop(name, TypeNode('ArrayProperty', (struct_type(typename),)),
                i32(len(records)) + b''.join(r.encode() for r in records))


def int_array(name, values):
    return prop(name, TypeNode('ArrayProperty', (TypeNode('IntProperty'),)), i32(len(values)) + b''.join(i32(v) for v in values))


def synthetic_save():
    unit = PropertyList([integer_property('UnitId', 1001), integer_property('Experience', 100),
                         integer_property('HitPoint', 200), integer_property('MagicPoint', 35),
                         int_array('baseParameterList', [0] * 20),
                         integer_property('EquipWeaponId', 10100), integer_property('EquipArmorId', 0),
                         int_array('EquipAccessoryIdList', [0, 0])])
    inventory = [PropertyList([integer_property('ItemId', iid), integer_property('Num', count)]) for iid, count in [(1, 5), (10100, 1)]]
    mrp = prop('synchroPoints', TypeNode('MapProperty', (TypeNode('IntProperty'), TypeNode('IntProperty'))), i32(0)+i32(1)+i32(1001)+i32(42))
    vision = PropertyList([integer_property('ID', 13017), integer_property('awakeningPoint', 1), mrp])
    root = PropertyList([struct_prop('Player', 'SaveData_Player', [integer_property('currentMoney', 99), integer_property('EscapeNum', 2), integer_property('WinNum', 3), integer_property('StealSuccessNum', 0)]),
                         array_prop('unitParameterList', 'SaveData_UnitParameter', [unit]),
                         array_prop('inventory', 'SaveData_Inventory', inventory),
                         array_prop('visionList', 'SaveData_Vision', [vision]),
                         integer_property('UnknownFutureField', 123456789)])
    prefix = b'GVAS' + i32(3) + i32(522) + i32(1017) + struct.pack('<HHHI', 5, 6, 1, 0) + fstring('UE5') + i32(3) + i32(0) + fstring('/Script/mucha.CPP_MuchaGameSaveData') + b'\0'
    plain = prefix + root.encode() + bytes(4)
    plain += bytes(16-len(plain)%16)
    enc = Cipher(algorithms.AES(FORMAT_KEY), modes.ECB()).encryptor()
    payload = enc.update(plain)+enc.finalize()
    return b'SVHD\x01\0\x01\0'+hashlib.sha1(payload).digest()+bytes(4)+payload


class SaveTests(unittest.TestCase):
    def setUp(self):
        self.raw = synthetic_save()
        self.editor = SaveEditor.open(self.raw)

    def test_encrypted_roundtrip_is_byte_identical(self):
        self.assertEqual(self.editor.export(), self.raw)

    def test_unknown_fields_survive_mutations(self):
        before = self.editor.root['UnknownFutureField'].encode()
        self.editor.set_experience(1001, 12345)
        self.editor.set_mrp(13017, 1001, 9876)
        self.editor.set_item(2, 17)
        decoded = SaveEditor.open(self.editor.export())
        self.assertEqual(decoded.root['UnknownFutureField'].encode(), before)
        self.assertEqual(decoded.experience(1001), 12345)
        self.assertEqual(decoded.vision_points(13017)[1001], 9876)
        self.assertEqual(decoded.inventory()[2], 17)

    def test_mrp_does_not_change_awakening(self):
        self.editor.set_mrp(13017, 1001, 5000)
        decoded = SaveEditor.open(self.editor.export())
        self.assertEqual(decoded.root['visionList'].structs()[0]['awakeningPoint'].integer(), 1)

    def test_seed_and_mastery_bonus_array_is_preserved(self):
        before = self.editor.units()[1001]['baseParameterList'].encode()
        self.editor.set_experience(1001, 1000000)
        self.assertEqual(SaveEditor.open(self.editor.export()).units()[1001]['baseParameterList'].encode(), before)

    def test_inventory_add_remove_updates_array_lengths(self):
        self.editor.set_item(3, 14)
        self.editor.set_item(1, 0)
        decoded = SaveEditor.open(self.editor.export())
        self.assertNotIn(1, decoded.inventory())
        self.assertEqual(decoded.inventory()[3], 14)

    def test_last_equipped_copy_cannot_be_removed(self):
        with self.assertRaisesRegex(ValueError, 'Unequip'):
            self.editor.set_item(10100, 0)

    def test_checksum_damage_is_rejected(self):
        raw = bytearray(self.raw)
        raw[-20] ^= 1
        with self.assertRaisesRegex(SaveFormatError, 'checksum'):
            SaveEditor.open(bytes(raw))

    def test_truncations_are_rejected(self):
        for length in [0, 3, 31, 32, 48, len(self.raw)-1, len(self.raw)-16]:
            with self.subTest(length=length), self.assertRaises(SaveFormatError):
                SaveEditor.open(self.raw[:length])

    def test_negative_and_overflow_totals_are_rejected(self):
        for value in [-1, 2**31, 1.5, True]:
            with self.subTest(value=value), self.assertRaises(ValueError):
                self.editor.set_experience(1001, value)

    def test_invalid_preset_is_atomic(self):
        preset = self.editor.preset('Test')
        preset['experience']['1001'] = 50000
        preset['mrp']['13017']['1001'] = -1
        with self.assertRaises(ValueError):
            self.editor.apply_preset(preset)
        self.assertEqual(self.editor.export(), self.raw)

    def test_snapshot_roundtrip_and_automatic_backup(self):
        with tempfile.TemporaryDirectory() as folder:
            store = CheckpointStore(Path(folder)/'checkpoints')
            checkpoint = store.capture('Before boss', self.raw, 'GameSaveData10.sav')
            metadata, loaded = store.read(checkpoint)
            self.assertEqual(loaded, self.raw)
            self.assertEqual(metadata['name'], 'Before boss')
            target = Path(folder)/'GameSaveData10.sav'
            target.write_bytes(self.raw)
            self.editor.set_item(3, 7)
            with patch('ffr_helper.storage.game_is_running', return_value=False):
                backup = write_save(target, self.editor.export(), store, live_slot=True)
            self.assertEqual(store.read(backup)[1], self.raw)
            self.assertEqual(SaveEditor.open(target.read_bytes()).inventory()[3], 7)

    def test_running_game_blocks_live_slot_replacement(self):
        with tempfile.TemporaryDirectory() as folder:
            path = Path(folder)/'GameSaveData10.sav'
            path.write_bytes(self.raw)
            with patch('ffr_helper.storage.game_is_running', return_value=True), self.assertRaisesRegex(RuntimeError, 'Close the game'):
                write_save(path, self.raw, CheckpointStore(Path(folder)/'checkpoints'), live_slot=True)
            self.assertEqual(path.read_bytes(), self.raw)

    def test_gear_compatibility_uses_game_data(self):
        data = reference_data()
        self.editor.set_equipment(1001, 'weapon', 10100, data)
        foreign = next((int(i) for i, x in data['items'].items() if x['gear'] and x['gear']['type']==1 and x['gear']['detail']!=data['characters']['1001']['weaponType']))
        self.editor.set_item(foreign, 1)
        with self.assertRaisesRegex(ValueError, 'weapon family'):
            self.editor.set_equipment(1001, 'weapon', foreign, data)


class ProgressionTests(unittest.TestCase):
    def test_character_costs_are_accumulated(self):
        curve = reference_data()['characters']['1001']['levels']
        self.assertEqual([r['total'] for r in curve[:4]], [0, 52, 142, 384])
        self.assertEqual(level_at(141, curve)['level'], 2)
        self.assertEqual(level_at(142, curve)['level'], 3)

    def test_mrp_costs_are_accumulated_per_vision(self):
        curve = reference_data()['visions']['13017']['mrp']
        self.assertEqual([r['total'] for r in curve[:4]], [0, 10, 45, 140])
        self.assertEqual(level_at(44, curve)['level'], 1)
        self.assertEqual(level_at(45, curve)['level'], 2)

    def test_route_totals_and_next_battle_boundary(self):
        result = route_totals(100, 30, [dict(count=2, exp=20, mrp=5)])
        self.assertEqual(result, (140, 40))
        self.assertEqual(level_at(result[0]+2, reference_data()['characters']['1001']['levels'])['level'], 3)

    def test_route_overflow_is_rejected(self):
        with self.assertRaises(ValueError):
            route_totals(2**31-2, 0, [dict(count=2, exp=1, mrp=0)])


@unittest.skipUnless(os.environ.get('FFR_TEST_SAVE'), 'Optional private local save is not part of the repository')
class RealSaveTests(unittest.TestCase):
    def test_real_save_lossless_edit_roundtrip(self):
        raw = Path(os.environ['FFR_TEST_SAVE']).read_bytes()
        editor = SaveEditor.open(raw)
        self.assertEqual(editor.export(), raw)
        uid = next(iter(editor.units()))
        old_xp = editor.experience(uid)
        editor.set_experience(uid, old_xp+1)
        vision = editor.root['visionList'].structs()[0]['ID'].integer()
        old_mrp = editor.vision_points(vision).get(uid, 0)
        editor.set_mrp(vision, uid, old_mrp+1)
        item = next(iter(editor.inventory()))
        editor.set_item(item, editor.inventory()[item]+1)
        output = editor.export()
        decoded = SaveEditor.open(output)
        self.assertEqual(decoded.experience(uid), old_xp+1)
        self.assertEqual(decoded.vision_points(vision)[uid], old_mrp+1)
        self.assertEqual(decoded.document.root['Map'].encode(), editor.document.root['Map'].encode())
        # Restore all changed values and require exactly the original encrypted file.
        decoded.set_experience(uid, old_xp)
        decoded.set_mrp(vision, uid, old_mrp)
        decoded.set_item(item, decoded.inventory()[item]-1)
        self.assertEqual(decoded.export(), raw)


if __name__ == '__main__':
    unittest.main()
