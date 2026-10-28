import hashlib
from pathlib import Path
import struct
import tempfile
import unittest
from unittest.mock import patch

from ffr_helper.compatibility import SignatureProfile, prepare_signature_profile, signature_script


def executable():
    raw=bytearray(1024);raw[:2]=b'MZ';struct.pack_into('<I',raw,0x3c,0x80)
    raw[0x80:0x84]=b'PE\0\0';struct.pack_into('<HH',raw,0x84,0x8664,1)
    struct.pack_into('<H',raw,0x94,0xf0);struct.pack_into('<H',raw,0x98,0x20b)
    struct.pack_into('<IIIIIIHHI',raw,0x188+8,512,0x1000,512,512,0,0,0,0,0x60000020)
    raw[512:576]=bytes(range(64))
    return bytes(raw)


def profile(raw):
    return SignatureProfile(hashlib.sha256(raw).hexdigest(),0x1000,hashlib.sha256(raw[512:576]).hexdigest())


class CompatibilityTests(unittest.TestCase):
    def test_exact_unique_code_produces_pure_symbol_lookup(self):
        raw=executable();script=signature_script(raw,profile(raw)).decode()
        self.assertIn('00 01 02 03',script)
        try: from lupa import LuaRuntime
        except ImportError: self.skipTest('Optional Lua test runtime unavailable')
        lua=LuaRuntime();lua.execute(script)
        self.assertEqual(lua.globals().OnMatchFound(0x12345678),0x12345678)
        self.assertEqual(bytes.fromhex(lua.globals().Register()),raw[512:576])

    def test_wrong_build_code_or_architecture_is_refused(self):
        raw=executable();p=profile(raw)
        for value in [raw[:-1],raw[:512]+b'x'+raw[513:]]:
            with self.assertRaisesRegex(ValueError,'Unsupported game build'):signature_script(value,p)
        wrong=bytearray(raw);struct.pack_into('<H',wrong,0x84,0x14c)
        with self.assertRaisesRegex(ValueError,'Windows x64'):signature_script(bytes(wrong),profile(bytes(wrong)))
        with self.assertRaisesRegex(ValueError,'fingerprint'):signature_script(raw,SignatureProfile(p.game_sha256,p.rva,'0'*64))

    def test_ambiguous_code_and_nonexecutable_section_are_refused(self):
        raw=bytearray(executable());raw[640:704]=raw[512:576]
        with self.assertRaisesRegex(ValueError,'ambiguous'):signature_script(bytes(raw),profile(bytes(raw)))
        raw=bytearray(executable());struct.pack_into('<I',raw,0x188+36,0x40000040)
        with self.assertRaisesRegex(ValueError,'executable section'):signature_script(bytes(raw),profile(bytes(raw)))

    def test_preparer_refuses_to_replace_an_existing_override(self):
        with tempfile.TemporaryDirectory() as folder:
            root=Path(folder);exe=root/'game.exe';exe.write_bytes(b'fixture')
            with patch('ffr_helper.compatibility.signature_script',return_value=b'lookup'):
                target=prepare_signature_profile(exe,root/'output')
                self.assertEqual(prepare_signature_profile(exe,root/'output'),target)
                target.write_bytes(b'custom user lookup')
                with self.assertRaisesRegex(ValueError,'not replaced'):prepare_signature_profile(exe,root/'output')
                self.assertEqual(target.read_bytes(),b'custom user lookup')
