import math
import struct
import unittest

from ffr_helper.widget_diagnostics import snapshot_timeline_widgets,WidgetSnapshotError


class WidgetDiagnosticsTests(unittest.TestCase):
    def setUp(self):
        self.memory={};self.names={};self.objects=[];self.base=0x10000000;self.vtable=self.base+0x1000
        self.table_variable=self.base+0x2000;self.count_variable=self.base+0x2008
        self.memory[self.table_variable]=bytearray(0x1000)
        self.table=0x30000000;self.chunk=0x31000000;self.field=0x32000000
        for address in [self.table,self.chunk,self.field]:self.memory[address]=bytearray(0x1000)
        self.meta=self.object('Class');self.put(self.meta+0x10,'<Q',self.meta)
        self.attachment_class=self.object('CommonVisualAttachment',klass=self.meta)
        self.package=self.object('/Game/UI/Battle/TimeLine/Widget/WBP_Btl_TimeLineEntry_Unit',klass=self.meta)
        self.widget_class=self.object('WBP_Btl_TimeLineEntry_Unit_C',klass=self.meta,outer=self.package)
        self.widget=self.object('WBP_Btl_TimeLineEntry_Unit_C_0',klass=self.widget_class)
        self.tree_class=self.object('WidgetTree',klass=self.meta)
        self.tree=self.object('WidgetTree_0',klass=self.tree_class,outer=self.widget)
        self.vm_class=self.object('CPP_VM_BtlUnitParam',klass=self.meta)
        self.vm=self.object('UnitVM_0',klass=self.vm_class)
        self.attachment=self.object('CVA_Unit',klass=self.attachment_class,outer=self.tree)
        self.put(self.widget_class+0x50,'<Q',self.field);self.put(self.widget_class+0x58,'<i',0x480)
        self.put(self.field,'<QQ',self.vtable,self.base+0x3000)
        self.put(self.field+0x20,'<Q',self.identity('CPP_VM_BtlUnitParam'))
        self.put(self.field+0x30,'<ii',1,8);self.put(self.field+0x44,'<i',0x400)
        self.put(self.widget+0x400,'<Q',self.vm)
        self.put(self.attachment+0x1b8,'<dd',0.5,1.0);self.put(self.attachment+0x1c8,'<Q',0x40000000)
        self.entry_class=self.object('CPP_VM_TimeLineEntry',klass=self.meta)
        self.entry=self.object('EntryVM_0',klass=self.entry_class)
        self.put(self.widget+0x408,'<Q',self.entry);self.put(self.entry+0x90,'<Q',self.vm)
        self.put(self.entry+0x7c,'<i',7)
        widget_entry_field=self.field+0x100;entry_param_field=self.field+0x200
        self.put(self.field+0x18,'<Q',widget_entry_field)
        for address,name,offset in [(widget_entry_field,'CPP_VM_TimeLineEntry',0x408),
                                    (entry_param_field,'UnitParamVM',0x90)]:
            self.put(address,'<QQ',self.vtable,self.base+0x3000);self.put(address+0x20,'<Q',self.identity(name))
            self.put(address+0x30,'<ii',1,8);self.put(address+0x44,'<i',offset)
        self.put(self.entry_class+0x50,'<Q',entry_param_field)
        self.put(self.table_variable,'<Q',self.table);self.put(self.table,'<Q',self.chunk)
        self.sync_objects()

    def identity(self,text):
        for identity,name in self.names.items():
            if name==text:return identity
        identity=len(self.names)+1;self.names[identity]=text;return identity

    def object(self,name,klass=None,outer=0,flags=0):
        index=len(self.objects);address=0x20000000+index*0x1000;self.memory[address]=bytearray(0x1000)
        self.put(address,'<QIIQQQ',self.vtable,flags,index,klass or address,self.identity(name),outer)
        self.put(address+0x58,'<i',0x1000);self.objects.append(address);return address

    def sync_objects(self):
        self.put(self.count_variable,'<i',len(self.objects))
        for i,address in enumerate(self.objects):self.put(self.chunk+i*24,'<Qiiii',address,0,0,i+1,0)

    def region(self,address,size):
        for start,raw in self.memory.items():
            if start<=address and address+size<=start+len(raw):return raw,address-start
        raise OSError('Unmapped synthetic widget memory')

    def put(self,address,fmt,*values):
        raw,at=self.region(address,struct.calcsize(fmt));struct.pack_into(fmt,raw,at,*values)

    def read(self,address,size):
        raw,at=self.region(address,size);return bytes(raw[at:at+size])

    def snapshot(self,reader=None,**overrides):
        options=dict(image_base=self.base,image_size=0x100000,object_table_address=self.table_variable,
            object_count_address=self.count_variable,attachment_class=self.attachment_class,current_models={self.vm:0},
            current_model_keys={7:self.vm})
        options.update(overrides)
        return snapshot_timeline_widgets(reader or self.read,self.names.__getitem__,**options)

    def test_widget_tree_owner_and_reflected_vm_offset_join_to_raw_anchor_only(self):
        result=self.snapshot();self.assertEqual(result['objectSlotsScanned'],len(self.objects))
        row=result['attachments'][0];self.assertEqual(row['storedContentAnchor'],[0.5,1.0])
        self.assertEqual(row['currentUnitIndex'],0);self.assertEqual(row['viewModelState'],'current')
        self.assertEqual(row['privateWidget'],self.widget);self.assertTrue(row['slateObjectPresent'])
        self.assertEqual(row['entryBinding']['currentUnitIndex'],0)
        self.assertTrue(row['entryBinding']['parameterViewModelMatchesWidget'])
        self.assertTrue(row['entryBinding']['cacheMatchesCurrentRegistry'])
        self.assertEqual(row['storedAnchorBits'],self.read(self.attachment+0x1b8,16).hex())
        for key in ['gameCalls','gameWrites','inputInjected','atomicSnapshot','restorable','completeEnumeration',
                    'visibleViewportVerified','renderedAnchorsVerified']:
            self.assertIs(result[key],False)

    def test_null_retired_invalid_and_unreadable_vm_references_are_not_current(self):
        retired=self.object('RetiredVM',klass=self.vm_class);self.sync_objects()
        for address,state in [(0,'null'),(retired,'outside-current'),(0x44444444,'unreadable'),(self.vm,'invalid-object')]:
            with self.subTest(state=state):
                self.put(self.widget+0x400,'<Q',address)
                if state=='invalid-object':self.put(self.vm+8,'<I',0x40000000)
                row=self.snapshot()['attachments'][0]
                self.assertEqual(row['viewModelState'],state);self.assertIsNone(row['currentUnitIndex'])

    def test_templates_and_non_timeline_attachments_do_not_claim_visible_instances(self):
        self.put(self.widget+8,'<I',0x10);self.assertEqual(self.snapshot()['attachments'],[])
        self.put(self.widget+8,'<I',0)
        self.put(self.attachment+0x18,'<Q',self.identity('UnrelatedVisualAttachment'))
        self.assertEqual(self.snapshot()['attachments'],[])

    def test_missing_cyclic_or_wrong_sized_properties_are_refused_without_guessing_offsets(self):
        cases=[(self.widget_class+0x50,'<Q',0),(self.field+0x18,'<Q',self.field),
               (self.field+0x30,'<i',2),(self.field+0x34,'<i',4),(self.field+0x44,'<i',0x480),
               (self.widget_class+0x40,'<Q',self.widget_class)]
        for address,fmt,value in cases:
            with self.subTest(address=address):
                before=self.read(address,struct.calcsize(fmt));self.put(address,fmt,value)
                with self.assertRaises(WidgetSnapshotError):self.snapshot()
                self.put(address,f'<{len(before)}s',before)

    def test_wrong_package_outer_cycles_and_weak_slot_identity_are_refused(self):
        cases=[(self.package+0x18,'<Q',self.identity('/Wrong/Package')),
               (self.tree+0x20,'<Q',self.tree),(self.attachment+0xc,'<I',999)]
        for address,fmt,value in cases:
            with self.subTest(address=address):
                before=self.read(address,struct.calcsize(fmt));self.put(address,fmt,value)
                with self.assertRaises(WidgetSnapshotError):self.snapshot()
                self.put(address,f'<{len(before)}s',before)

    def test_selected_weak_slot_swap_is_detected_even_when_object_header_stays_readable(self):
        selected=self.chunk+9*24
        def swapped(address,size):
            raw=self.read(address,size)
            if address==selected and size==24:
                raw=bytearray(raw);struct.pack_into('<i',raw,16,999);return bytes(raw)
            return raw
        with self.assertRaisesRegex(WidgetSnapshotError,'weak identity changed'):self.snapshot(swapped)

    def test_anchor_and_binding_changes_during_readback_refuse_the_snapshot(self):
        for target,size in [(self.attachment+0x1b8,16),(self.widget+0x400,8),(self.field,0x48),(self.count_variable,4)]:
            seen=0
            def drift(address,amount):
                nonlocal seen
                raw=self.read(address,amount)
                if address==target and amount==size:
                    seen+=1
                    if seen>1:return bytes(amount)
                return raw
            with self.subTest(target=target),self.assertRaises(WidgetSnapshotError):self.snapshot(drift)

    def test_unknown_names_and_shadowed_vm_properties_are_not_guessed(self):
        original=self.read(self.field+0x20,8);self.put(self.field+0x20,'<Q',999999)
        with self.assertRaisesRegex(WidgetSnapshotError,'name'):self.snapshot()
        self.put(self.field+0x20,'<8s',original)
        extra=self.field+0x300;self.put(extra,'<72s',self.read(self.field,72))
        self.put(extra+0x18,'<Q',0)
        self.put(self.field+0x18,'<Q',extra)
        with self.assertRaisesRegex(WidgetSnapshotError,'ambiguous'):self.snapshot()

    def test_nonfinite_anchors_are_refused_and_finite_unusual_values_remain_raw(self):
        for value in [math.nan,math.inf,-math.inf,1_000_001]:
            self.put(self.attachment+0x1b8,'<dd',0.5,value)
            with self.subTest(value=value),self.assertRaisesRegex(WidgetSnapshotError,'anchor'):self.snapshot()
        self.put(self.attachment+0x1b8,'<dd',0.75,-2)
        self.assertEqual(self.snapshot()['attachments'][0]['storedContentAnchor'],[0.75,-2])

    def test_unrelated_unreadable_headers_are_counted_without_a_completeness_claim(self):
        def partial(address,size):
            if address==self.package and size==24:raise OSError('Unrelated enumeration read failure')
            return self.read(address,size)
        result=self.snapshot(partial);self.assertEqual(len(result['attachments']),1)
        self.assertEqual(result['unreadableObjectHeaders'],1);self.assertFalse(result['completeEnumeration'])

    def test_typed_context_bounds_and_short_reads_are_refused(self):
        for kwargs in [dict(image_size=True),dict(current_models={self.vm:True}),dict(attachment_class=1),
                       dict(slate_vtables=(1,2)),dict(slate_vtables=(True,self.base)),
                       dict(current_model_keys={True:self.vm}),dict(current_model_keys={7:0x55555555}),
                       dict(current_model_keys={7:self.vm,8:self.vm})]:
            with self.subTest(kwargs=kwargs),self.assertRaises(WidgetSnapshotError):self.snapshot(**kwargs)
        self.put(self.count_variable,'<i',1_000_001)
        with self.assertRaises(WidgetSnapshotError):self.snapshot()
        for reader in [lambda a,s:b'',lambda a,s:bytearray(s)]:
            with self.subTest(reader=reader),self.assertRaises(WidgetSnapshotError):self.snapshot(reader)

    def slate(self,anchor=(0.5,1.0)):
        controller=0x40000000;obj=controller+16
        self.memory[controller]=bytearray(0x1000)
        vtables=(self.base+0x5000,self.base+0x6000)
        self.put(controller,'<Qii',vtables[1],1,1);self.put(obj,'<Q',vtables[0])
        self.put(obj+0x260,'<dd',*anchor);self.put(self.attachment+0x1c8,'<QQ',obj,controller)
        return obj,controller,vtables

    def test_profiled_slate_anchor_is_distinct_from_widget_storage_and_pixels(self):
        obj,controller,profile=self.slate((0.5,0))
        row=self.snapshot(slate_vtables=profile)['attachments'][0]
        self.assertEqual(row['storedContentAnchor'],[0.5,1])
        self.assertEqual(row['slateAnchor']['state'],'captured')
        self.assertEqual(row['slateAnchor']['storedAnchor'],[0.5,0]);self.assertFalse(row['slateAnchor']['matchesWidget'])
        self.assertFalse(row['slateAnchor']['renderedAnchorVerified'])
        self.put(obj+0x260,'<dd',0.5,1)
        self.assertTrue(self.snapshot(slate_vtables=profile)['attachments'][0]['slateAnchor']['matchesWidget'])

    def test_absent_mismatched_inactive_and_unreadable_slate_keep_widget_evidence(self):
        obj,controller,profile=self.slate()
        cases=[(self.attachment+0x1c8,'<QQ',(0,0),'absent'),
               (self.attachment+0x1c8,'<QQ',(obj,0),'unsupported-shared-reference'),
               (obj,'<Q',(self.base+0x7000,),'profile-mismatch'),
               (controller+8,'<i',(0,),'inactive-reference'),
               (self.attachment+0x1c8,'<QQ',(0x50000010,0x50000000),'unreadable')]
        for address,fmt,values,state in cases:
            with self.subTest(state=state):
                old=self.read(address,struct.calcsize(fmt));self.put(address,fmt,*values)
                row=self.snapshot(slate_vtables=profile)['attachments'][0]
                self.assertEqual(row['slateAnchor']['state'],state);self.assertIsNone(row['slateAnchor']['storedAnchor'])
                self.assertEqual(row['storedContentAnchor'],[0.5,1])
                self.put(address,f'<{len(old)}s',old)

    def test_slate_anchor_or_shared_owner_drift_is_not_accepted(self):
        obj,controller,profile=self.slate()
        for target,size in [(obj+0x260,16),(self.attachment+0x1c8,16),(controller,8)]:
            visits=0
            def drift(address,amount):
                nonlocal visits
                raw=self.read(address,amount)
                if address==target and amount==size:
                    visits+=1
                    if visits>1:return bytes(amount)
                return raw
            with self.subTest(target=target),self.assertRaises(WidgetSnapshotError):self.snapshot(drift,slate_vtables=profile)

    def test_transient_shared_counts_do_not_falsely_claim_an_atomic_pin(self):
        obj,controller,profile=self.slate();reads=0
        def changing_counts(address,size):
            nonlocal reads
            raw=self.read(address,size)
            if address==controller+8:
                reads+=1;return struct.pack('<i',1+reads)
            return raw
        result=self.snapshot(changing_counts,slate_vtables=profile)
        self.assertEqual(result['attachments'][0]['slateAnchor']['state'],'captured')
        self.assertFalse(result['atomicSnapshot']);self.assertFalse(result['visibleViewportVerified'])

    def test_retired_entry_cache_and_stale_widget_source_are_distinguished(self):
        retired=self.object('RetiredVM',klass=self.vm_class);self.sync_objects()
        self.put(self.entry+0x90,'<Q',retired)
        row=self.snapshot()['attachments'][0]
        self.assertEqual(row['viewModelState'],'current')
        self.assertEqual(row['entryBinding']['parameterViewModelState'],'outside-current')
        self.assertFalse(row['entryBinding']['parameterViewModelMatchesWidget'])
        self.put(self.entry+0x90,'<Q',self.vm);self.put(self.widget+0x400,'<Q',retired)
        row=self.snapshot()['attachments'][0]
        self.assertEqual(row['viewModelState'],'outside-current')
        self.assertEqual(row['entryBinding']['parameterViewModelState'],'current')
        self.assertFalse(row['entryBinding']['parameterViewModelMatchesWidget'])

    def test_optional_null_or_missing_entry_binding_does_not_invent_a_source(self):
        self.put(self.widget+0x408,'<Q',0)
        row=self.snapshot()['attachments'][0];self.assertEqual(row['entryBinding']['state'],'null')
        self.assertIsNone(row['entryBinding']['privateParameterViewModel'])
        self.put(self.widget+0x408,'<Q',self.entry);self.put(self.field+0x220,'<Q',self.identity('UnrelatedMember'))
        row=self.snapshot()['attachments'][0];self.assertEqual(row['entryBinding']['state'],'unavailable')
        self.assertEqual(row['storedContentAnchor'],[0.5,1]);self.assertIsNone(row['entryBinding']['currentUnitIndex'])

    def test_entry_cache_binding_drift_is_not_hidden_by_the_optional_projection(self):
        for target in [self.widget+0x408,self.entry+0x90]:
            visits=0
            def drift(address,size):
                nonlocal visits
                raw=self.read(address,size)
                if address==target and size==8:
                    visits+=1
                    if visits>1:return bytes(size)
                return raw
            with self.subTest(target=target),self.assertRaises(WidgetSnapshotError):self.snapshot(drift)

    def test_transient_change_inside_optional_entry_read_cannot_be_hidden_by_later_matching_bytes(self):
        visits=0
        def transient(address,size):
            nonlocal visits
            raw=self.read(address,size)
            if address==self.field and size==0x48:
                visits+=1
                if visits==2:
                    raw=bytearray(raw);struct.pack_into('<i',raw,0x44,0x408);return bytes(raw)
            return raw
        with self.assertRaisesRegex(WidgetSnapshotError,'changed or became unavailable'):self.snapshot(transient)

    def test_reused_lookup_key_reveals_a_retired_cache_without_authorizing_rebind(self):
        retired=self.object('RetiredVM',klass=self.vm_class);self.sync_objects()
        self.put(self.entry+0x90,'<Q',retired);self.put(self.widget+0x400,'<Q',retired)
        result=self.snapshot();binding=result['attachments'][0]['entryBinding']
        self.assertEqual(binding['unitLookupKey'],7);self.assertEqual(binding['expectedCurrentUnitIndex'],0)
        self.assertEqual(binding['privateRegistryViewModel'],self.vm)
        self.assertFalse(binding['cacheMatchesCurrentRegistry']);self.assertTrue(binding['parameterViewModelMatchesWidget'])
        self.assertFalse(result['gameCalls']);self.assertFalse(result['gameWrites']);self.assertFalse(result['restorable'])

    def test_missing_or_unrequested_registry_keys_do_not_invent_a_matching_target(self):
        for keys in [None,{}, {8:self.vm}]:
            with self.subTest(keys=keys):
                binding=self.snapshot(current_model_keys=keys)['attachments'][0]['entryBinding']
                self.assertIsNone(binding['privateRegistryViewModel']);self.assertIsNone(binding['cacheMatchesCurrentRegistry'])
                self.assertIsNone(binding['expectedCurrentUnitIndex'])

    def add_turn_members(self):
        turn=self.object('TurnVM_0',klass=self.meta);self.sync_objects()
        fields=[(self.field+0x400,'TurnVM',0x70,8),(self.field+0x500,'EntryIndex',0x78,4),
                (self.field+0x600,'EntryData',0x7c,20)]
        self.put(self.field+0x218,'<Q',fields[0][0])
        for i,(address,name,offset,size) in enumerate(fields):
            self.put(address,'<QQ',self.vtable,self.base+0x3000);self.put(address+0x20,'<Q',self.identity(name))
            self.put(address+0x30,'<ii',1,size);self.put(address+0x44,'<i',offset)
            self.put(address+0x18,'<Q',fields[i+1][0] if i+1<len(fields) else 0)
        self.put(self.entry+0x70,'<Q',turn);self.put(self.entry+0x78,'<i',0)
        return turn,self.read(self.entry+0x7c,20)

    def test_entry_turn_membership_and_identity_are_separate_from_timing_values(self):
        turn,raw=self.add_turn_members();expected=bytearray(raw);expected[12:]=b'\x55'*8
        row=self.snapshot(current_segments={turn:[bytes(expected)]})['attachments'][0]['entryBinding']['turnBinding']
        self.assertEqual(row['state'],'captured');self.assertTrue(row['turnMatchesCurrentTimeline'])
        self.assertTrue(row['identityMatchesCurrentTurn'])
        expected[8]=1
        row=self.snapshot(current_segments={turn:[bytes(expected)]})['attachments'][0]['entryBinding']['turnBinding']
        self.assertFalse(row['identityMatchesCurrentTurn'])

    def test_retired_turn_or_invalid_index_does_not_invent_current_entry_identity(self):
        turn,raw=self.add_turn_members()
        row=self.snapshot(current_segments={})['attachments'][0]['entryBinding']['turnBinding']
        self.assertFalse(row['turnMatchesCurrentTimeline']);self.assertIsNone(row['identityMatchesCurrentTurn'])
        self.put(self.entry+0x78,'<i',1)
        row=self.snapshot(current_segments={turn:[raw]})['attachments'][0]['entryBinding']['turnBinding']
        self.assertTrue(row['turnMatchesCurrentTimeline']);self.assertIsNone(row['identityMatchesCurrentTurn'])
        for context in [{True:[]},{turn:[b'bad']},{turn:'bad'}]:
            with self.subTest(context=context),self.assertRaises(WidgetSnapshotError):self.snapshot(current_segments=context)

    def test_stored_findings_consume_actual_reader_schema_without_promoting_old_turns(self):
        from ffr_helper.timeline_findings import analyze_timeline_widgets
        turn,raw=self.add_turn_members()
        retired=self.object('RetiredVM',klass=self.vm_class);self.sync_objects()
        self.put(self.entry+0x90,'<Q',retired);self.put(self.widget+0x400,'<Q',retired)
        links=dict(format='ffr-helper-timeline-links',protocol=1,readOnly=True,
            gameCalls=False,gameWrites=False,atomicSnapshot=False,restorable=False,
            units=[dict(index=0,expectedParty=True,storedRelationByte=1)],unitViewModels=[])
        current=self.snapshot(current_segments={turn:[raw]})
        findings=analyze_timeline_widgets(links,current)['widgetFindings'][0]
        self.assertEqual(findings['membership'],'current-turn-candidate')
        self.assertEqual(findings['findings'],['entry-cache-disagrees-with-current-registry',
                                             'portrait-vm-not-current'])
        old=self.snapshot(current_segments={})
        findings=analyze_timeline_widgets(links,old)['widgetFindings'][0]
        self.assertEqual(findings['membership'],'outside-current-turns')
        self.assertEqual(findings['findings'],[])
