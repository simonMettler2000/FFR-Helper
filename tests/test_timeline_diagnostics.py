import struct
import unittest

from ffr_helper.timeline_diagnostics import snapshot_timeline_links,TimelineSnapshotError


class TimelineDiagnosticsTests(unittest.TestCase):
    def setUp(self):
        self.memory={};self.reads=[];self.base=0x10000000;self.image_size=0x100000
        self.battle=0x20000000;self.ui=0x20001000;self.units=0x20002000;self.owner=0x20003000
        self.actors=[0x20004000+i*0x1000 for i in range(3)]
        self.parameters=[0x20008000+i*0x1000 for i in range(3)]
        self.models=[0x2000c000+i*0x1000 for i in range(3)]
        self.storage=0x20010000;self.actor_array=0x20011000
        self.vtable=self.base+0x1000;self.target=self.base+0x2000
        for address in [self.battle,self.ui,self.units,self.owner,*self.actors,*self.parameters,*self.models,
                        self.storage,self.actor_array,self.vtable,self.target]:
            self.memory[address]=bytearray(0x1000)
        for address in [self.battle,self.ui,self.units,self.owner,*self.actors,*self.parameters,*self.models]:
            self.put(address,'<QQQQ',self.vtable,0,0x30000000,0)
        self.put(self.vtable+0x2e8,'<Q',self.target)
        self.put(self.target,'<16s',bytes.fromhex('0fb68180030000c3cccccccccccccccc'))
        self.put(self.battle+0x1a9,'<B',1);self.put(self.ui+0x80,'<B',1)
        self.put(self.battle+0x48,'<Q',self.units);self.put(self.ui+0x78,'<Q',self.owner)
        self.put(self.units+0x28,'<Qii',self.actor_array,3,3)
        self.put(self.actor_array,'<QQQ',*self.actors)
        for i,(actor,parameter,vm) in enumerate(zip(self.actors,self.parameters,self.models)):
            self.put(actor+0x838,'<Q',parameter);self.put(parameter+0x360,'<Q',actor)
            self.put(parameter+0xd0,'<i',[1001,1002,11][i])
            self.put(vm+0x198,'<i',i);self.put(vm+0x1a0,'<Q',parameter)
        # One unoccupied sparse slot contains garbage and is not interpreted.
        self.put(self.owner+0x28,'<Qii',self.storage,4,4)
        self.put(self.owner+0x5c,'<i',1);self.put(self.owner+0x60,'<i',0)
        self.put(self.owner+0x68,'<Qi',0,1)
        for i,slot in enumerate([0,2,3]):
            self.put(self.storage+slot*24,'<i4xQii',i,self.models[i],[2,3,-1][i],0)
        self.put(self.storage+24,'<24s',b'\xff'*24)

    def region(self,address,size):
        for start,raw in self.memory.items():
            if start<=address and address+size<=start+len(raw):return raw,address-start
        raise OSError('Unmapped fake memory')

    def put(self,address,fmt,*values):
        raw,offset=self.region(address,struct.calcsize(fmt));struct.pack_into(fmt,raw,offset,*values)

    def read(self,address,size):
        self.reads.append((address,size));raw,offset=self.region(address,size);return bytes(raw[offset:offset+size])

    def snapshot(self,reader=None,**overrides):
        options=dict(battle=self.battle,ui=self.ui,image_base=self.base,image_size=self.image_size,friend_ids=[1001,1002])
        options.update(overrides);return snapshot_timeline_links(reader or self.read,**options)

    def test_sparse_current_links_are_joined_without_claiming_relation_or_rendering(self):
        result=self.snapshot();self.assertTrue(result['readOnly']);self.assertFalse(result['gameCalls'])
        self.assertEqual(result['unitsWithoutCurrentViewModel'],[])
        self.assertEqual([x['expectedParty'] for x in result['units']],[True,True,False])
        self.assertEqual([x['currentUnitIndex'] for x in result['unitViewModels']],[0,1,2])
        self.assertTrue(all(x['cachedParameterState']=='current' for x in result['unitViewModels']))
        self.assertTrue(all(x['actualRelationVerified'] is False for x in result['unitViewModels']))
        self.assertFalse(result['renderedAnchorsVerified']);self.assertFalse(result['atomicSnapshot'])
        self.assertEqual(result['unitViewModels'][0]['privateRelationGetterCode'],self.read(self.target,16).hex())

    def test_null_and_invalid_cached_links_report_only_the_verified_fallback(self):
        for pointer,state in [(0,'null'),(0x20012000,'invalid-object')]:
            with self.subTest(state=state):
                if pointer:
                    self.memory[pointer]=bytearray(0x1000)
                    self.put(pointer,'<QQQQ',self.vtable,0x40000000,0x30000000,0)
                self.put(self.models[0]+0x1a0,'<Q',pointer)
                result=self.snapshot();vm=result['unitViewModels'][0]
                self.assertEqual(vm['cachedParameterState'],state);self.assertEqual(vm['getterFallbackRelation'],0)
                self.assertIsNone(vm['currentUnitIndex']);self.assertEqual(result['unitsWithoutCurrentViewModel'],[0])

    def test_unreadable_link_never_infers_a_none_relation(self):
        self.put(self.models[0]+0x1a0,'<Q',0x40000000)
        vm=self.snapshot()['unitViewModels'][0]
        self.assertEqual(vm['cachedParameterState'],'unreadable');self.assertIsNone(vm['getterFallbackRelation'])

    def test_unsupported_header_does_not_invent_the_native_fallback(self):
        stale=0x20012000;self.memory[stale]=bytearray(0x1000)
        self.put(stale,'<QQQQ',0,0,0,0);self.put(self.models[0]+0x1a0,'<Q',stale)
        vm=self.snapshot()['unitViewModels'][0]
        self.assertEqual(vm['cachedParameterState'],'unsupported-object-header');self.assertIsNone(vm['getterFallbackRelation'])

    def test_retired_but_valid_link_is_distinguished_from_current_parameter(self):
        stale=0x20012000;self.memory[stale]=bytearray(0x1000)
        self.put(stale,'<QQQQ',self.vtable,0,0x30000000,0);self.put(stale+0xd0,'<i',1001)
        self.put(self.models[0]+0x1a0,'<Q',stale)
        vm=self.snapshot()['unitViewModels'][0]
        self.assertEqual(vm['cachedParameterState'],'outside-current');self.assertIsNone(vm['currentUnitIndex'])
        self.assertEqual(vm['cachedUnitId'],1001);self.assertIsNone(vm['getterFallbackRelation'])

    def test_cycles_holes_duplicate_keys_and_uncovered_rows_are_refused(self):
        cases=[(self.storage+16,'<i',0),(self.storage+2*24,'<i',0),
               (self.owner+0x5c,'<i',0),(self.owner+0x70,'<i',3),
               (self.models[0]+0x198,'<i',99)]
        for address,fmt,value in cases:
            with self.subTest(address=address):
                before=self.read(address,struct.calcsize(fmt));self.put(address,fmt,value)
                with self.assertRaises(TimelineSnapshotError):self.snapshot()
                self.put(address,f'<{len(before)}s',before)

    def test_bad_battle_identity_party_and_virtual_targets_are_refused(self):
        for options in [dict(friend_ids=[1001,1001]),dict(friend_ids=[1003]),dict(friend_ids=[True]),
                        dict(image_size=True),dict(battle=1)]:
            with self.subTest(options=options),self.assertRaises(TimelineSnapshotError):self.snapshot(**options)
        for address,fmt,value in [(self.ui+0x80,'<B',0),(self.parameters[0]+0x360,'<Q',self.actors[1]),
                                  (self.vtable+0x2e8,'<Q',0x70000000)]:
            with self.subTest(address=address):
                before=self.read(address,struct.calcsize(fmt));self.put(address,fmt,value)
                with self.assertRaises(TimelineSnapshotError):self.snapshot()
                self.put(address,f'<{len(before)}s',before)

    def test_any_copied_field_changing_on_readback_refuses_the_snapshot(self):
        target=self.models[0]+0x1a0;seen=[False]
        def drift(address,size):
            value=self.read(address,size)
            if address==target:
                if seen[0]:return bytes(size)
                seen[0]=True
            return value
        with self.assertRaisesRegex(TimelineSnapshotError,'changed during verification'):self.snapshot(drift)

    def test_short_nonbytes_and_failed_reads_cannot_be_valid_evidence(self):
        for reader in [lambda a,s:b'',lambda a,s:bytearray(s),lambda a,s:(_ for _ in ()).throw(OSError('Read failed'))]:
            with self.subTest(reader=reader),self.assertRaises(TimelineSnapshotError):self.snapshot(reader)
