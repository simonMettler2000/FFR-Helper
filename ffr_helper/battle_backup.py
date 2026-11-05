"""Read-only partial inspection of the game's own pre-battle backup.

Requires an independently verified exact-build context. Native pointer-bearing
UNIT_PARAMETER records are never treated as portable or restorable save states.
"""
from __future__ import annotations

from collections import Counter
import hashlib
import struct


class BattleBackupError(ValueError):
    pass


FIELDS={'unitId':0x8,'baseLevel':0x38,'hpBase':0x44,'mp':0x48,'stagger':0x4c,'limitBurst':0x50,
        'maxHp':0x58,'maxMp':0x5c,'maxStagger':0x60,'attack':0x64,'defence':0x68,
        'intelligence':0x6c,'mind':0x70,'divine':0x74,'agility':0x78,
        'weaponId':0x120,'armorId':0x124,'visionId':0x138,'experience':0x154}


def snapshot_battle_backup(read,battle,root,image_base,image_size):
    """Copy bounded resource components, preserving every duplicate unit row.

    The caller establishes current game/root ownership before and after this
    function. No engine getter, constructor, deserializer or restore is called.
    """
    if (type(image_base) is not int or type(image_size) is not int
            or not 0x10000 <= image_base < 0x7fffffffffff
            or not 0 < image_size <= 2**32 or image_base+image_size >= 0x7fffffffffff):
        raise BattleBackupError('Invalid exact-build image context')
    regions={};budget=4*1024*1024
    def copy(address,size):
        nonlocal budget
        if (type(address) is not int or type(size) is not int or not 0 < size <= budget
                or not 0x10000 <= address < 0x7fffffffffff or address+size >= 0x7fffffffffff):
            raise BattleBackupError('Invalid or over-budget backup range')
        raw=read(address,size)
        if not isinstance(raw,bytes) or len(raw)!=size:raise BattleBackupError('Incomplete backup read')
        budget-=size;key=address,size
        if key in regions and regions[key]!=raw:raise BattleBackupError('Backup changed during capture')
        regions[key]=raw;return raw
    def object(address):
        vtable,flags,klass,_=struct.unpack('<QQQQ',copy(address,32))
        if not image_base <= vtable < image_base+image_size or not 0x10000 <= klass < 0x7fffffffffff or flags&0x40000000:
            raise BattleBackupError('Invalid backup/root object')
    def array(raw,offset,limit,stride):
        pointer,count,capacity=struct.unpack_from('<Qii',raw,offset)
        if not 0 <= count <= capacity <= limit or count and pointer < 0x10000:
            raise BattleBackupError('Malformed bounded backup array')
        return copy(pointer,count*stride) if count else b''
    def items(raw):
        result=[];seen=set()
        for unit_id,count in struct.iter_unpack('<ii',raw):
            if unit_id<=0 or count<0 or unit_id in seen:raise BattleBackupError('Invalid or repeated inventory row')
            seen.add(unit_id);result.append(dict(id=unit_id,count=count))
        return result
    try:
        object(battle);object(root)
        if copy(battle+0x1a9,1)!=b'\1':raise BattleBackupError('An active battle is required for this backup diagnostic')
        before=copy(battle+0x168,0x3c)
        unit_raw=array(before,0,128,0x1a0);units=[]
        for index,offset in enumerate(range(0,len(unit_raw),0x1a0)):
            row=unit_raw[offset:offset+0x1a0]
            fields={key:struct.unpack_from('<i',row,at)[0] for key,at in FIELDS.items()}
            if fields['unitId']<=0:raise BattleBackupError('Invalid backup unit ID')
            accessories=[x[0] for x in struct.iter_unpack('<i',array(row,0x128,16,4))]
            passives=[x[0] for x in struct.iter_unpack('<i',array(row,0x140,128,4))]
            units.append(dict(index=index,raw=fields,accessories=accessories,passives=passives))
        inventory_raw=array(before,0x10,4096,8);inventory=items(inventory_raw)
        flags=array(before,0x20,1024*1024,1);money=struct.unpack_from('<i',before,0x30)[0]
        if money<0:raise BattleBackupError('Invalid backup money')
        current_inventory_raw=array(copy(root+0xa8,16),0,4096,8)
        items(current_inventory_raw)
        current_flags=array(copy(root+0x188,16),0,1024*1024,1)
        current_money=struct.unpack('<i',copy(root+0x28,4))[0]
        if current_money<0:raise BattleBackupError('Invalid current money')
        for (address,size),raw in regions.items():
            if read(address,size)!=raw:raise BattleBackupError('Backup changed during verification')
        counts=Counter(x['raw']['unitId'] for x in units)
        return dict(format='ffr-helper-partial-native-battle-backup',protocol=1,readOnly=True,
            gameCalls=False,gameWrites=False,atomicSnapshot=False,restorable=False,
            deepUnitParametersVerified=False,effectsVerified=False,rngVerified=False,
            units=units,duplicateUnitIds=sorted(key for key,value in counts.items() if value>1),
            inventory=inventory,eventFlagCount=len(flags),eventFlagsSha256=hashlib.sha256(flags).hexdigest(),money=money,
            graphicsDepthOfField=dict(foregroundMaxRadiusBits=struct.unpack_from('<I',before,0x34)[0],
                backgroundMaxRadiusBits=struct.unpack_from('<I',before,0x38)[0],source='static-console-variable-getter-audit'),
            currentInventoryMatchesBackup=current_inventory_raw==inventory_raw,
            currentEventFlagsMatchBackup=current_flags==flags,currentMoneyMatchesBackup=current_money==money,
            unitTopLevelBytesPortable=False,fieldClockOrRngCaptured=False)
    except (OSError,struct.error) as error:
        raise BattleBackupError('Incomplete native backup snapshot') from error
