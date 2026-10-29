import copy
from pathlib import Path
import tempfile
from types import SimpleNamespace
import unittest

from ffr_helper.legacy_input import LegacyBinding, LegacyPad
from ffr_helper.practice_input import WindowsPracticeShortcut, ShortcutEdge


class LegacyInputTests(unittest.TestCase):
    def setUp(self):
        self.raw=dict(format='ffr-helper-legacy-shortcut',version=1,index=0,capabilitySha256='a'*64,
                      buttons=0x21,pov=18000,label='R1 + Square + D-pad Down')

    def test_binding_requires_exact_calibrated_device_buttons_and_pov(self):
        binding=LegacyBinding.from_dict(self.raw)
        self.assertTrue(binding.held(LegacyPad(0,'a'*64,0x21,18000)))
        for pad in [LegacyPad(1,'a'*64,0x21,18000),LegacyPad(0,'b'*64,0x21,18000),
                    LegacyPad(0,'a'*64,0x61,18000),LegacyPad(0,'a'*64,0x21,65535)]:
            self.assertFalse(binding.held(pad))

    def test_corrupt_ambiguous_and_oversized_bindings_are_rejected(self):
        for key,value in [('index',True),('index',16),('buttons',0),('buttons',1),('buttons',7),
                          ('buttons',0x100000001),('capabilitySha256','x'*64),('pov',0),
                          ('version',True),('label','Anything'),('unknown',False)]:
            raw=copy.deepcopy(self.raw);raw[key]=value
            with self.assertRaises(ValueError):LegacyBinding.from_dict(raw)
        with self.assertRaises(ValueError):LegacyBinding(-1,'a'*64,0x21)
        with tempfile.TemporaryDirectory() as folder:
            p=Path(folder)/'bad.ffrinput.json';p.write_bytes(b' '*4097)
            with self.assertRaisesRegex(ValueError,'too large'):LegacyBinding.load(p)

    def test_real_sampler_path_normalizes_only_calibrated_legacy_chord(self):
        binding=LegacyBinding.from_dict(self.raw)
        released=LegacyPad(0,'a'*64,0,65535);held=LegacyPad(0,'a'*64,0x21,18000)
        frames=iter([released,held,held,None,held,released,held])
        class Reader:
            def read(self,index):self_test.assertEqual(index,0);return next(frames)
        self_test=self
        sampler=WindowsPracticeShortcut.__new__(WindowsPracticeShortcut)
        sampler.edge=ShortcutEdge();sampler.focused=False;sampler.xinput=None
        sampler.legacy_binding=binding;sampler.legacy=Reader()
        sampler.user=SimpleNamespace(GetAsyncKeyState=lambda key:0)
        sampler._foreground_matches=lambda:True
        self.assertEqual([sampler.poll() for _ in range(7)],
                         [None,'controller',None,None,None,None,'controller'])

    def test_legacy_reader_is_not_polled_outside_game_and_wrong_device_cannot_activate(self):
        binding=LegacyBinding.from_dict(self.raw);reads=[]
        pad=[LegacyPad(0,'b'*64,0x21,18000)];focused=[False]
        class Reader:
            def read(self,index):reads.append(index);return pad[0]
        sampler=WindowsPracticeShortcut.__new__(WindowsPracticeShortcut)
        sampler.edge=ShortcutEdge();sampler.focused=False;sampler.xinput=None
        sampler.legacy_binding=binding;sampler.legacy=Reader()
        sampler.user=SimpleNamespace(GetAsyncKeyState=lambda key:0)
        sampler._foreground_matches=lambda:focused[0]
        self.assertIsNone(sampler.poll());self.assertEqual(reads,[])
        focused[0]=True;self.assertIsNone(sampler.poll())
        pad[0]=LegacyPad(0,'a'*64,0x21,18000);self.assertIsNone(sampler.poll())
        pad[0]=LegacyPad(0,'a'*64,0,65535);self.assertIsNone(sampler.poll())
        pad[0]=LegacyPad(0,'a'*64,0x21,18000);self.assertEqual(sampler.poll(),'controller')
