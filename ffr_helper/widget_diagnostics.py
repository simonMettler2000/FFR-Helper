"""Private, bounded read-only stored timeline-widget values.

The caller must verify the exact build, loaded code/layout and current battle.
No process is opened and no engine function is invoked here. Object enumeration
is deliberately incomplete/non-atomic; stored anchor values are not pixels or
proof that a widget is attached to the visible viewport.
"""
from __future__ import annotations

import math
import re
import struct


class WidgetSnapshotError(ValueError):
    pass


def snapshot_timeline_widgets(read, read_name, *, image_base, image_size,
                              object_table_address, object_count_address,
                              attachment_class, current_models, slate_vtables=None):
    """Copy anchors and VM links from retained CVA_Unit attachments.

    FField/FProperty offsets follow the target's retained UE4SS layout report.
    Native ContentAnchor offset/size are independently corroborated by generated
    property metadata and its native getter. Optional Slate reads additionally
    require the caller-verified native allocation, vtables and setter bodies.
    Blueprint VM-member offsets come from the actual class's property list,
    never guessed asset property order or writes.
    """
    def pointer(value):
        return type(value) is int and 0x10000 <= value < 0x7fffffffffff

    if (not pointer(image_base) or type(image_size) is not int or not 0 < image_size <= 2**32
            or image_base+image_size >= 0x7fffffffffff
            or any(not pointer(x) for x in [object_table_address,object_count_address,attachment_class])
            or not isinstance(current_models,dict) or len(current_models)>256
            or any(not pointer(k) or type(v) is not int or not 0<=v<64 for k,v in current_models.items())):
        raise WidgetSnapshotError('Invalid caller-verified widget context')
    if slate_vtables is not None and (not isinstance(slate_vtables,tuple) or len(slate_vtables)!=2
            or any(type(x) is not int or not image_base<=x<image_base+image_size for x in slate_vtables)):
        raise WidgetSnapshotError('Invalid caller-verified Slate context')
    regions={};names={};budget=96*1024*1024

    def copy(address,size,remember=True):
        nonlocal budget
        if (not pointer(address) or type(size) is not int or not 0<size<=min(budget,2*1024*1024)
                or address+size>=0x7fffffffffff):
            raise WidgetSnapshotError('Invalid or over-budget widget range')
        budget-=size
        raw=read(address,size)
        if not isinstance(raw,bytes) or len(raw)!=size:
            raise WidgetSnapshotError('Incomplete widget memory read')
        if remember:
            key=address,size
            if key in regions and regions[key]!=raw:
                raise WidgetSnapshotError('Widget values changed during capture')
            regions[key]=raw
        return raw

    def value(address,fmt,remember=True):
        return struct.unpack(fmt,copy(address,struct.calcsize(fmt),remember))[0]

    def name(identity):
        try:text=read_name(identity)
        except (OSError,ValueError,LookupError) as error:
            raise WidgetSnapshotError('Unreadable widget/property name') from error
        if not isinstance(text,str) or not 0<len(text)<=1024 or '\0' in text:
            raise WidgetSnapshotError('Invalid widget/property name')
        if identity in names and names[identity]!=text:
            raise WidgetSnapshotError('Widget/property name changed')
        names[identity]=text
        return text

    def object_header(address):
        raw=copy(address,40)
        vtable,flags,index,klass,identity,outer=struct.unpack('<QIIQQQ',raw)
        if (not image_base<=vtable<image_base+image_size or flags&0x40000000
                or not pointer(klass) or index>=2**31):
            raise WidgetSnapshotError('Invalid retained widget object')
        return dict(vtable=vtable,flags=flags,index=index,klass=klass,name=identity,outer=outer)

    class_cache={}
    def class_info(address):
        if address not in class_cache:
            h=object_header(address)
            class_cache[address]=(name(h['name']),h)
        return class_cache[address]

    def widget_owner(outer):
        seen=set()
        for _ in range(8):
            if not outer:return None
            if outer in seen:raise WidgetSnapshotError('Cyclic widget outer chain')
            seen.add(outer);h=object_header(outer);klass_name,klass=class_info(h['klass'])
            if klass_name=='WBP_Btl_TimeLineEntry_Unit_C':
                package=object_header(klass['outer'])
                if name(package['name'])!='/Game/UI/Battle/TimeLine/Widget/WBP_Btl_TimeLineEntry_Unit':
                    raise WidgetSnapshotError('Timeline widget class package differs')
                return outer,h
            outer=h['outer']
        raise WidgetSnapshotError('Widget outer chain exceeds its bound')

    property_cache={}
    def model_offset(klass):
        if klass in property_cache:return property_cache[klass]
        origin=klass;classes=set();fields=set();found=[]
        for _ in range(32):
            if not klass:break
            if klass in classes:raise WidgetSnapshotError('Cyclic widget class hierarchy')
            classes.add(klass);object_header(klass)
            size=value(klass+0x58,'<i')
            if not 40<=size<=1024*1024:raise WidgetSnapshotError('Invalid widget class size')
            field=value(klass+0x50,'<Q')
            while field:
                if field in fields or len(fields)>=4096:raise WidgetSnapshotError('Cyclic or excessive widget properties')
                fields.add(field);raw=copy(field,0x48)
                vtable,field_class=struct.unpack_from('<QQ',raw)
                if not (image_base<=vtable<image_base+image_size and image_base<=field_class<image_base+image_size):
                    raise WidgetSnapshotError('Unsupported widget property header')
                identity=struct.unpack_from('<Q',raw,0x20)[0]
                if name(identity)=='CPP_VM_BtlUnitParam':
                    dim,element=struct.unpack_from('<ii',raw,0x30);offset=struct.unpack_from('<i',raw,0x44)[0]
                    if dim!=1 or element!=8 or not 40<=offset<=size-8:
                        raise WidgetSnapshotError('Timeline VM property is not a bounded scalar object reference')
                    found.append(offset)
                field=struct.unpack_from('<Q',raw,0x18)[0]
            klass=value(klass+0x40,'<Q')
        else:raise WidgetSnapshotError('Widget class hierarchy exceeds its bound')
        if len(found)!=1:raise WidgetSnapshotError('Timeline widget VM property is missing or ambiguous')
        property_cache[origin]=found[0]
        return found[0]

    def slate_anchor(shared,widget_anchor):
        result=dict(state='not-requested',storedAnchor=None,matchesWidget=None,renderedAnchorVerified=False)
        if slate_vtables is None:return result
        obj,controller=struct.unpack('<QQ',shared)
        if not obj and not controller:
            result['state']='absent';return result
        if not pointer(obj) or not pointer(controller) or controller+16!=obj:
            result['state']='unsupported-shared-reference';return result
        try:
            if value(obj,'<Q')!=slate_vtables[0] or value(controller,'<Q')!=slate_vtables[1]:
                result['state']='profile-mismatch';return result
            strong=value(controller+8,'<i',False)
            if not 0<strong<2**30:
                result['state']='inactive-reference';return result
            raw=copy(obj+0x260,16);anchor=list(struct.unpack('<dd',raw))
            if any(not math.isfinite(x) or abs(x)>1_000_000 for x in anchor):
                result['state']='invalid-stored-anchor';return result
            if not 0<value(controller+8,'<i',False)<2**30:
                result['state']='inactive-reference';return result
            result.update(state='captured',storedAnchor=anchor,matchesWidget=anchor==widget_anchor)
        except OSError:result['state']='unreadable'
        return result

    try:
        if class_info(attachment_class)[0]!='CommonVisualAttachment':
            raise WidgetSnapshotError('Wrong native attachment class')
        count=value(object_count_address,'<i');table=value(object_table_address,'<Q')
        if not 0<=count<=1_000_000 or count and not pointer(table):
            raise WidgetSnapshotError('Object array exceeds the bounded widget audit')
        results=[];unreadable=0;seen=set()
        for first in range(0,count,65536):
            chunk=value(table+(first>>16)*8,'<Q')
            amount=min(65536,count-first);rows=copy(chunk,amount*24,False)
            for at in range(amount):
                index=first+at;slot=at*24
                obj,flags,_,serial=struct.unpack_from('<Qiii',rows,slot)
                if not obj or serial<=0 or flags&0x10200000:continue
                if not pointer(obj):
                    unreadable+=1;continue
                try:head=copy(obj,24,False)
                except OSError:
                    unreadable+=1;continue
                vtable,object_flags,internal_index,klass=struct.unpack('<QIIQ',head)
                if klass!=attachment_class or object_flags&0x40000000:continue
                if internal_index!=index or not image_base<=vtable<image_base+image_size:
                    raise WidgetSnapshotError('Attachment slot and object identity differ')
                h=object_header(obj)
                if not re.fullmatch(r'CVA_Unit(?:_[0-9]+)?',name(h['name'])):continue
                owner=widget_owner(h['outer'])
                if owner is None:continue
                if owner[1]['flags']&0x30:continue  # CDO/archetype templates are not live instances.
                if obj in seen or len(results)>=256:raise WidgetSnapshotError('Repeated or excessive timeline attachments')
                seen.add(obj)
                # Only selected weak slots become retained evidence. Unrelated
                # object allocation/deletion is not represented as an atomic
                # whole-engine object-array snapshot.
                if copy(chunk+slot,24)!=rows[slot:slot+24]:
                    raise WidgetSnapshotError('Attachment weak identity changed during enumeration')
                widget,wh=owner;offset=model_offset(wh['klass']);vm=value(widget+offset,'<Q')
                state='null' if not vm else 'outside-current'
                if vm:
                    try:
                        vm_head=copy(vm,40)
                        vm_vtable,vm_flags,vm_index,vm_class,_,_=struct.unpack('<QIIQQQ',vm_head)
                        if vm_flags&0x40000000:state='invalid-object'
                        elif not image_base<=vm_vtable<image_base+image_size or not pointer(vm_class) or vm_index>=2**31:
                            state='unsupported-object-header'
                        else:state='current' if vm in current_models else 'outside-current'
                    except OSError:state='unreadable'
                raw=copy(obj+0x1b8,16);anchor=list(struct.unpack('<dd',raw))
                if any(not math.isfinite(x) or abs(x)>1_000_000 for x in anchor):
                    raise WidgetSnapshotError('Invalid stored attachment anchor')
                shared=copy(obj+0x1c8,16)
                results.append(dict(privateAttachment=obj,privateWidget=widget,privateViewModel=vm,
                    currentUnitIndex=current_models[vm] if state=='current' else None,
                    viewModelState=state,storedContentAnchor=anchor,storedAnchorBits=raw.hex(),
                    slateObjectPresent=bool(struct.unpack_from('<Q',shared)[0]),
                    slateAnchor=slate_anchor(shared,anchor),renderedAnchorVerified=False))
        for (address,size),raw in regions.items():
            if copy(address,size,False)!=raw:raise WidgetSnapshotError('Widget values changed during verification')
        for identity,text in names.items():
            if name(identity)!=text:raise WidgetSnapshotError('Widget/property name changed during verification')
        return dict(format='ffr-helper-timeline-widget-values',protocol=1,readOnly=True,
            gameCalls=False,gameWrites=False,inputInjected=False,atomicSnapshot=False,restorable=False,
            completeEnumeration=False,visibleViewportVerified=False,renderedAnchorsVerified=False,
            objectSlotsScanned=count,unreadableObjectHeaders=unreadable,attachments=results)
    except (OSError,struct.error) as error:
        raise WidgetSnapshotError('Incomplete timeline-widget snapshot') from error
