"""Working-copy editing. Disk writes are handled by the separate backup service."""
from __future__ import annotations
from dataclasses import dataclass

from .codec import SaveDocument, PropertyList, integer_property, SaveFormatError
from .progression import checked_total


@dataclass
class SaveEditor:
    document: SaveDocument

    @classmethod
    def open(cls, raw: bytes):
        editor = cls(SaveDocument.decode(raw))
        # Fully validate every editable collection before exposing any edits.
        for collection, key in [('unitParameterList', 'UnitId'), ('inventory', 'ItemId'), ('visionList', 'ID')]:
            ids = [r[key].integer() for r in editor.document.root[collection].structs()]
            if len(ids) != len(set(ids)):
                raise SaveFormatError(f'Duplicate IDs in {collection}.')
        return editor

    @property
    def root(self):
        return self.document.root

    def _record(self, collection, key, value):
        for row in self.root[collection].structs():
            if row[key].integer() == value:
                return row
        raise ValueError(f'ID {value} is not present in {collection}.')

    def units(self):
        return {r['UnitId'].integer(): r for r in self.root['unitParameterList'].structs()}

    def experience(self, unit: int):
        return self._record('unitParameterList', 'UnitId', unit)['Experience'].integer()

    def set_experience(self, unit: int, total: int):
        self._record('unitParameterList', 'UnitId', unit)['Experience'].set_integer(checked_total(total))

    def vision_points(self, vision: int):
        return self._record('visionList', 'ID', vision)['synchroPoints'].integer_map()

    def set_mrp(self, vision: int, unit: int, total: int):
        if unit not in self.units():
            raise ValueError('Character is not present in this save.')
        prop = self._record('visionList', 'ID', vision)['synchroPoints']
        points = prop.integer_map()
        points[unit] = checked_total(total)
        prop.set_integer_map(points)

    def inventory(self):
        return {r['ItemId'].integer(): r['Num'].integer() for r in self.root['inventory'].structs()}

    def set_item(self, item: int, quantity: int):
        checked_total(item)
        checked_total(quantity)
        if item <= 0:
            raise ValueError('Item ID must be positive.')
        rows = self.root['inventory'].structs()
        existing = next((r for r in rows if r['ItemId'].integer() == item), None)
        if not quantity:
            if any(self._is_equipped(row, item) for row in self.units().values()):
                raise ValueError('Unequip this item before removing its last inventory copy.')
            if existing is not None:
                rows.remove(existing)
        elif existing is not None:
            existing['Num'].set_integer(quantity)
        else:
            rows.append(PropertyList([integer_property('ItemId', item), integer_property('Num', quantity)]))

    @staticmethod
    def _is_equipped(row, item):
        return (row['EquipWeaponId'].integer() == item or row['EquipArmorId'].integer() == item
                or item in row['EquipAccessoryIdList'].integers())

    def set_equipment(self, unit: int, slot: str, item: int, reference: dict):
        row = self._record('unitParameterList', 'UnitId', unit)
        character = reference['characters'][str(unit)]
        if item:
            if self.inventory().get(item, 0) < 1:
                raise ValueError('Add an inventory copy before equipping this item.')
            gear = reference['items'].get(str(item), {}).get('gear')
            expected = 1 if slot == 'weapon' else 2 if slot == 'armor' else 3
            if not gear or gear['type'] != expected:
                raise ValueError('The item does not fit this equipment slot.')
            if gear['sex'] and gear['sex'] != character['sex']:
                raise ValueError('This character cannot equip that item.')
            if expected == 1 and gear['detail'] != character['weaponType']:
                raise ValueError('This weapon belongs to another weapon family.')
            if expected == 2 and gear['detail'] not in character['armorTypes']:
                raise ValueError('This armour type is not compatible with this character.')
        if slot in ['weapon', 'armor']:
            row['EquipWeaponId' if slot == 'weapon' else 'EquipArmorId'].set_integer(item)
        elif slot in ['accessory1', 'accessory2']:
            accessories = row['EquipAccessoryIdList'].integers()
            if len(accessories) != 2:
                raise SaveFormatError('Unexpected accessory-slot count.')
            accessories[0 if slot == 'accessory1' else 1] = item
            row['EquipAccessoryIdList'].set_integers(accessories)
        else:
            raise ValueError('Unknown equipment slot.')

    def set_counter(self, name: str, value: int):
        if name not in ['currentMoney', 'EscapeNum', 'WinNum', 'StealSuccessNum']:
            raise ValueError('Unsupported player counter.')
        self.root['Player'].struct()[name].set_integer(checked_total(value))

    def export(self):
        raw = self.document.encode()
        # A write must survive a second independent decode of the new bytes.
        check = SaveEditor.open(raw)
        if check.document.encode() != raw:
            raise SaveFormatError('Edited-save read-back validation failed.')
        return raw

    def preset(self, name: str):
        return dict(format='ffr-helper-preset', version=1, name=name,
                    experience={str(uid): row['Experience'].integer() for uid, row in self.units().items()},
                    inventory={str(k): v for k, v in self.inventory().items()},
                    mrp={str(row['ID'].integer()): {str(k): v for k, v in row['synchroPoints'].integer_map().items()}
                         for row in self.root['visionList'].structs()})

    def apply_preset(self, preset: dict):
        if preset.get('format') != 'ffr-helper-preset' or preset.get('version') != 1:
            raise ValueError('Unsupported practice preset.')
        # Work on a clone; a rejected value must not leave half a preset applied.
        clone = SaveEditor.open(self.export())
        for unit, total in preset.get('experience', {}).items():
            clone.set_experience(int(unit), total)
        for vision, points in preset.get('mrp', {}).items():
            for unit, total in points.items():
                clone.set_mrp(int(vision), int(unit), total)
        for item, quantity in preset.get('inventory', {}).items():
            clone.set_item(int(item), quantity)
        self.document = clone.document
