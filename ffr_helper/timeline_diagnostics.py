"""Bounded read-only unit/view-model links for the timeline investigation.

Caller must establish build24250053's image/layout identity and a current battle
boundary. No pointer here is invoked. Private addresses/code samples must not be
published. Repeated reads detect drift, but do not provide an atomic snapshot.
"""
from __future__ import annotations

import struct


class TimelineSnapshotError(ValueError):
    pass


def snapshot_timeline_links(read, battle, ui, image_base, image_size, friend_ids):
    """Join the UI unit-VM hash map to actual current battle parameters.

    The portrait Blueprint consumes GetRelation, which returns None for a null
    or invalid BtlUnitParam. A matching current link alone does not prove the
    getter result, MVVM notification delivery or the rendered widget anchor.
    """
    if (type(image_base) is not int or type(image_size) is not int
            or not 0x10000 <= image_base < 0x7fffffffffff
            or not 0 < image_size <= 2**32 or image_base+image_size >= 0x7fffffffffff
            or not isinstance(friend_ids,(list,tuple)) or not 0 < len(friend_ids) <= 16
            or any(type(x) is not int or not 0 < x <= 2**31-1 for x in friend_ids)
            or len(set(friend_ids)) != len(friend_ids)):
        raise TimelineSnapshotError('Invalid exact-build context or expected party IDs')
    regions={};budget=2*1024*1024

    def copy(address,size):
        nonlocal budget
        if (type(address) is not int or type(size) is not int or size <= 0 or size > budget
                or not 0x10000 <= address < 0x7fffffffffff or address+size >= 0x7fffffffffff):
            raise TimelineSnapshotError('Invalid or over-budget memory range')
        raw=read(address,size)
        if not isinstance(raw,bytes) or len(raw)!=size:
            raise TimelineSnapshotError('Incomplete timeline memory read')
        budget-=size
        key=(address,size)
        if key in regions and regions[key]!=raw:
            raise TimelineSnapshotError('Timeline links changed during capture')
        regions[key]=raw
        return raw

    def number(address,fmt):
        return struct.unpack(fmt,copy(address,struct.calcsize(fmt)))[0]

    def header(address,allow_invalid=False):
        raw=copy(address,32)
        vtable,flags,klass,_=struct.unpack('<QQQQ',raw)
        valid=(image_base <= vtable < image_base+image_size and 0x10000 <= klass < 0x7fffffffffff
               and not flags & 0x40000000)
        if not valid and not allow_invalid:
            raise TimelineSnapshotError('Invalid battle/UI object header')
        return valid,vtable,flags

    def array(address,limit,stride):
        raw=copy(address,16);data,count,capacity=struct.unpack('<Qii',raw)
        if not 0 <= count <= capacity <= limit or count and data < 0x10000:
            raise TimelineSnapshotError('Malformed bounded timeline array')
        return copy(data,count*stride) if count else b''

    try:
        header(battle);header(ui)
        if number(battle+0x1a9,'<B')!=1 or number(ui+0x80,'<B')!=1:
            raise TimelineSnapshotError('A current active battle is required')
        units=number(battle+0x48,'<Q');header(units)
        actors=[x[0] for x in struct.iter_unpack('<Q',array(units+0x28,64,8))]
        if not actors or len(set(actors))!=len(actors):
            raise TimelineSnapshotError('Missing or repeated active actor')
        active=[];by_parameter={};found_friends=set()
        for index,actor in enumerate(actors):
            header(actor);parameter=number(actor+0x838,'<Q');header(parameter)
            if parameter in by_parameter or number(parameter+0x360,'<Q')!=actor:
                raise TimelineSnapshotError('Current actor/parameter ownership differs')
            unit_id=number(parameter+0xd0,'<i')
            if unit_id <= 0:
                raise TimelineSnapshotError('Invalid current unit ID')
            expected_friend=unit_id in friend_ids
            if expected_friend:
                if unit_id in found_friends:raise TimelineSnapshotError('Repeated expected party unit')
                found_friends.add(unit_id)
            item=dict(index=index,unitId=unit_id,expectedParty=expected_friend,
                      privateActor=actor,privateParameter=parameter)
            active.append(item);by_parameter[parameter]=item
        if found_friends!=set(friend_ids):
            raise TimelineSnapshotError('Current battle does not contain the expected party')

        owner=number(ui+0x78,'<Q');header(owner)
        # GetUnitParamVM indexes these 24-byte sparse rows through live hash
        # chains. A hole is not a VM and must never be interpreted as one.
        map_header=copy(owner+0x28,0x50)
        storage,count,capacity=struct.unpack_from('<Qii',map_header)
        free=struct.unpack_from('<i',map_header,0x34)[0]
        buckets,bucket_count=struct.unpack_from('<Qi',map_header,0x40)
        if (not 0 <= free <= count <= capacity <= 256 or count and storage < 0x10000
                or not 0 < bucket_count <= 512 or bucket_count & (bucket_count-1)
                or not buckets and bucket_count!=1):
            raise TimelineSnapshotError('Malformed unit-view-model hash map')
        rows=copy(storage,count*24) if count else b''
        heads=copy(buckets or owner+0x60,bucket_count*4)
        seen=set();keys=set();vm_pointers=set();models=[];represented=set()
        for bucket,(head,) in enumerate(struct.iter_unpack('<i',heads)):
            slot=head
            while slot!=-1:
                if not 0 <= slot < count or slot in seen:
                    raise TimelineSnapshotError('Cyclic, repeated or invalid unit-VM chain')
                seen.add(slot);offset=slot*24
                unique_id=struct.unpack_from('<i',rows,offset)[0]
                vm=struct.unpack_from('<Q',rows,offset+8)[0]
                if unique_id < 0 or unique_id & (bucket_count-1)!=bucket or unique_id in keys or vm in vm_pointers:
                    raise TimelineSnapshotError('Repeated or invalid unit-VM identity')
                keys.add(unique_id);vm_pointers.add(vm);header(vm)
                if number(vm+0x198,'<i')!=unique_id:
                    raise TimelineSnapshotError('Unit-VM key and stored unique ID differ')
                source_parameter=number(vm+0x70,'<Q')
                source_link=by_parameter.get(source_parameter)
                parameter=number(vm+0x1a0,'<Q')
                link=by_parameter.get(parameter)
                state='current' if link else 'outside-current'
                fallback=None;getter=None;code=None;unit_id=None
                if not parameter:
                    state='null';fallback=0
                else:
                    try:
                        valid,vtable,flags=header(parameter,allow_invalid=True)
                        if not valid:
                            state='invalid-object' if flags & 0x40000000 else 'unsupported-object-header'
                            fallback=0 if flags & 0x40000000 else None
                        else:
                            unit_id=number(parameter+0xd0,'<i')
                            if vtable+0x2f0>image_base+image_size:
                                raise TimelineSnapshotError('Relation virtual slot is outside the bound image')
                            target=number(vtable+0x2e8,'<Q')
                            if not image_base <= target <= image_base+image_size-16:
                                raise TimelineSnapshotError('Relation virtual target is outside the bound image')
                            getter=target-image_base;code=copy(target,16).hex()
                    except OSError:
                        # Preserve an unreadable link as unavailable evidence,
                        # never infer a None relation from a failed host read.
                        state='unreadable';fallback=None
                if link and state=='current':represented.add(link['index'])
                models.append(dict(uniqueId=unique_id,privateViewModel=vm,privateParameter=parameter,
                    privateSourceParameter=source_parameter,sourceParameterMatchesCached=source_parameter==parameter,
                    sourceParameterCurrentUnitIndex=source_link['index'] if source_link else None,
                    cachedParameterState=state,currentUnitIndex=link['index'] if link and state=='current' else None,
                    cachedUnitId=unit_id,getterFallbackRelation=fallback,
                    privateRelationGetterRva=getter,privateRelationGetterCode=code,
                    actualRelationVerified=False,renderedAnchorVerified=False))
                slot=struct.unpack_from('<i',rows,offset+16)[0]
        if len(seen)!=count-free:
            raise TimelineSnapshotError('Unit-VM hash chains do not cover occupied slots')
        for (address,size),raw in regions.items():
            if read(address,size)!=raw:
                raise TimelineSnapshotError('Timeline links changed during verification')
        return dict(format='ffr-helper-timeline-links',protocol=1,readOnly=True,
            gameCalls=False,gameWrites=False,atomicSnapshot=False,restorable=False,
            relationVerified=False,renderedAnchorsVerified=False,
            units=active,unitViewModels=models,
            unitsWithoutCurrentViewModel=[x['index'] for x in active if x['index'] not in represented])
    except (OSError,struct.error) as error:
        raise TimelineSnapshotError('Incomplete timeline link snapshot') from error
