import json
from pathlib import Path
import shutil
import tempfile
import time
import unittest

from ffr_helper.bridge import FieldSaveBridge

try:
    from lupa import LuaRuntime
except ImportError:
    LuaRuntime = None


class ProtocolTests(unittest.TestCase):
    def test_status_requires_current_session_and_fresh_heartbeat(self):
        with tempfile.TemporaryDirectory() as folder:
            root = Path(folder)
            (root/'connection').mkdir()
            bridge = FieldSaveBridge(root)
            bridge.heartbeat()
            values = dict(protocol=1, adapter='ffr-helper-field-save', session=bridge.session,
                          time=int(time.time()), ready=True, enabled=False)
            status = root/'connection/status.json'
            status.write_text(json.dumps(values))
            self.assertIsNotNone(bridge.status())
            values['session'] = 'another-session'
            status.write_text(json.dumps(values))
            self.assertIsNone(bridge.status())
            values['session'], values['time'] = bridge.session, int(time.time())-20
            status.write_text(json.dumps(values))
            self.assertIsNone(bridge.status())

    def test_close_writes_a_disabled_lease(self):
        with tempfile.TemporaryDirectory() as folder:
            root = Path(folder)
            (root/'connection').mkdir()
            bridge = FieldSaveBridge(root)
            bridge.requested = True
            bridge.heartbeat()
            self.assertTrue((root/'connection/control.txt').read_text().endswith('1\n'))
            bridge.stop()
            self.assertTrue((root/'connection/control.txt').read_text().endswith('0\n'))


@unittest.skipIf(LuaRuntime is None, 'Lua adapter tests require the optional lupa test dependency')
class LuaTests(unittest.TestCase):
    def setup_runtime(self, folder, count=1):
        root = Path(folder)/'FFRHelper'
        shutil.copytree(Path(__file__).resolve().parents[1]/'bridge/FFRHelper', root)
        (root/'connection').mkdir()
        runtime = LuaRuntime()
        runtime.execute('''
            testPost = nil
            function FindAllOf(name)
                local fn = {
                    IsValid = function() return true end,
                    GetFName = function() return {ToString=function() return "IsTouchSavePoint" end} end,
                    GetFullName = function() return "Function /Script/mucha.TestLibrary:IsTouchSavePoint" end,
                    GetOuter = function() return {GetFName=function() return {ToString=function() return "TestLibrary" end} end} end
                }
                local list = {}
                for index=1,testCount do list[index]=fn end
                return list
            end
            function RegisterHook(path, pre, post) testPath=path;testPost=post;return 1,2 end
            function LoopInGameThreadWithDelay(ms, callback) testPoll=callback end
        ''')
        runtime.globals().testCount = count
        runtime.execute('dofile(...)', str(root/'Scripts/main.lua').replace('\\', '/'))
        return root, runtime

    def test_hook_defaults_off_and_expires_when_the_app_disappears(self):
        with tempfile.TemporaryDirectory() as folder:
            root, runtime = self.setup_runtime(folder)
            bridge = FieldSaveBridge(root)
            bridge.heartbeat()
            runtime.globals().testPoll()
            self.assertTrue(bridge.status()['ready'])
            self.assertIsNone(runtime.globals().testPost())
            self.assertEqual(runtime.globals().testPath, '/Script/mucha.TestLibrary:IsTouchSavePoint')
            bridge.requested = True
            bridge.heartbeat()
            runtime.globals().testPoll()
            self.assertTrue(runtime.globals().testPost())
            runtime.execute('local originalTime=os.time;os.time=function() return originalTime()+30 end')
            self.assertIsNone(runtime.globals().testPost())

    def test_ambiguous_game_functions_are_not_hooked(self):
        with tempfile.TemporaryDirectory() as folder:
            root, runtime = self.setup_runtime(folder, count=2)
            bridge = FieldSaveBridge(root)
            bridge.heartbeat()
            runtime.globals().testPoll()
            self.assertFalse(bridge.status()['ready'])
            self.assertIsNone(runtime.globals().testPost)

    def test_function_discovery_retries_after_the_game_loads_its_library(self):
        with tempfile.TemporaryDirectory() as folder:
            root, runtime = self.setup_runtime(folder, count=0)
            bridge = FieldSaveBridge(root)
            bridge.heartbeat()
            runtime.globals().testPoll()
            self.assertFalse(bridge.status()['ready'])
            runtime.globals().testCount = 1
            runtime.execute('local originalTime=os.time;os.time=function() return originalTime()+6 end')
            runtime.globals().testPoll()
            self.assertIsNotNone(runtime.globals().testPost)


if __name__ == '__main__':
    unittest.main()
