import struct
import unittest

from ffr_helper.battle_backup import snapshot_battle_backup,BattleBackupError
import test_timeline_diagnostics as memory_fixtures


class BattleBackupTests(unittest.TestCase):
    def setUp(self):
        self.memory=memory_fixtures.TimelineDiagnosticsTests();self.memory.setUp()
        self.root=0x20020000;self.unit_data=0x20010000
        for address in [self.root,*range(0x20021000,0x20027000,0x1000)]:self.memory.memory[address]=bytearray(0x1000)
        self.put(self.root,'<QQQQ',self.memory.vtable,0,0x30000000,0)
        self.put(self.memory.battle+0x168,'<Qii',self.unit_data,3,3)
        for i,unit_id in enumerate([1001,1002,1001]):
            at=self.unit_data+i*0x1a0
            self.put(at+8,'<i',unit_id);self.put(at+0x38,'<i',25);self.put(at+0x44,'<i',888-i)
            self.put(at+0x48,'<i',45);self.put(at+0x4c,'<i',0);self.put(at+0x50,'<i',10)
            self.put(at+0x58,'<i',1000);self.put(at+0x128,'<Qii',0x20021000,2,2)
            self.put(at+0x140,'<Qii',0x20022000,2,2)
        self.put(0x20021000,'<ii',30001,30002);self.put(0x20022000,'<ii',101,202)
        self.put(self.memory.battle+0x178,'<Qii',0x20023000,2,2)
        self.put(self.root+0xa8,'<Qii',0x20024000,2,2)
        for address in [0x20023000,0x20024000]:self.put(address,'<iiii',1,20,3,10)
        self.put(self.memory.battle+0x188,'<Qii',0x20025000,4,4)
        self.put(self.root+0x188,'<Qii',0x20026000,4,4)
        for address in [0x20025000,0x20026000]:self.put(address,'<4s',b'\x01\x00\x01\x01')
        self.put(self.memory.battle+0x198,'<iff',5000,0.025,0.05);self.put(self.root+0x28,'<i',5000)

    def put(self,address,fmt,*values):self.memory.put(address,fmt,*values)

    def snapshot(self,reader=None):
        return snapshot_battle_backup(reader or self.memory.read,self.memory.battle,self.root,
                                      self.memory.base,self.memory.image_size)

    def test_resources_are_copied_but_duplicate_rows_and_partial_coverage_remain_explicit(self):
        result=self.snapshot()
        self.assertEqual([x['raw']['unitId'] for x in result['units']],[1001,1002,1001])
        self.assertEqual([x['raw']['hpBase'] for x in result['units']],[888,887,886])
        self.assertEqual(result['duplicateUnitIds'],[1001]);self.assertEqual(result['units'][0]['accessories'],[30001,30002])
        self.assertEqual(result['units'][0]['passives'],[101,202])
        self.assertTrue(result['currentInventoryMatchesBackup']);self.assertTrue(result['currentEventFlagsMatchBackup'])
        self.assertTrue(result['currentMoneyMatchesBackup']);self.assertTrue(result['readOnly'])
        for key in ['gameCalls','gameWrites','atomicSnapshot','restorable','deepUnitParametersVerified',
                    'effectsVerified','rngVerified','fieldClockOrRngCaptured','unitTopLevelBytesPortable']:
            self.assertIs(result[key],False)

    def test_current_resource_changes_are_reported_without_rewriting_the_backup(self):
        original=self.memory.read(self.memory.battle+0x168,0x3c)
        self.put(0x20024000+4,'<i',19);self.put(0x20026000,'<B',0);self.put(self.root+0x28,'<i',4000)
        result=self.snapshot()
        self.assertFalse(result['currentInventoryMatchesBackup']);self.assertFalse(result['currentEventFlagsMatchBackup'])
        self.assertFalse(result['currentMoneyMatchesBackup']);self.assertEqual(result['money'],5000)
        self.assertEqual(result['inventory'][0]['count'],20)
        self.assertEqual(self.memory.read(self.memory.battle+0x168,0x3c),original)

    def test_depth_of_field_bits_are_preserved_without_interpreting_them_as_rng_or_clock(self):
        self.put(self.memory.battle+0x19c,'<II',0x7fc12345,0x3d4ccccd)
        graphics=self.snapshot()['graphicsDepthOfField']
        self.assertEqual(graphics['foregroundMaxRadiusBits'],0x7fc12345)
        self.assertEqual(graphics['backgroundMaxRadiusBits'],0x3d4ccccd)

    def test_invalid_counts_unit_ids_and_inventory_rows_are_refused(self):
        cases=[(self.memory.battle+0x170,'<ii',(129,129)),(self.unit_data+8,'<i',(-1,)),
               (self.unit_data+0x128,'<Qii',(1,2,2)),(0x20023000+8,'<i',(1,)),
               (0x20023000+4,'<i',(-1,)),(self.memory.battle+0x198,'<i',(-1,)),
               (self.root+0x28,'<i',(-1,)),(self.memory.battle+0x1a9,'<B',(0,))]
        for address,fmt,values in cases:
            with self.subTest(address=address):
                old=self.memory.read(address,struct.calcsize(fmt));self.put(address,fmt,*values)
                with self.assertRaises(BattleBackupError):self.snapshot()
                self.put(address,f'<{len(old)}s',old)

    def test_nested_resource_drift_rejects_the_whole_snapshot(self):
        visits=[0]
        def drift(address,size):
            raw=self.memory.read(address,size)
            if address==0x20022000:
                visits[0]+=1
                if visits[0]>1:return b'\x00'*size
            return raw
        with self.assertRaisesRegex(BattleBackupError,'changed during capture'):self.snapshot(drift)

    def test_root_and_snapshot_readback_must_stay_stable(self):
        seen=set()
        def drift(address,size):
            raw=self.memory.read(address,size)
            if address==self.root+0x28:
                if address in seen:return bytes(size)
                seen.add(address)
            return raw
        with self.assertRaisesRegex(BattleBackupError,'changed during verification'):self.snapshot(drift)

    def test_failed_short_and_nonbytes_reads_are_not_valid_evidence(self):
        for reader in [lambda a,s:b'',lambda a,s:bytearray(s),lambda a,s:(_ for _ in ()).throw(OSError('read only fixture'))]:
            with self.subTest(reader=reader),self.assertRaises(BattleBackupError):self.snapshot(reader)
