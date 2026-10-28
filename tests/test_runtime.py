"""Recorder isolation, expiry and detached battle observations; no live game."""
import json
from pathlib import Path
import tempfile
import time
import unittest

from ffr_helper.runtime import RuntimeRecorder, NativeEntryRecorder, export_report, prepare_recorder, validate_report

try:
    from lupa import LuaRuntime
except ImportError:
    LuaRuntime = None


def sample_report(recorder):
    return dict(protocol=1, adapter='ffr-helper-runtime-observer', session=recorder.session,
                request=recorder.request, time=int(time.time()), format='ffr-helper-runtime-report',
                version=1, kind='diagnostic-observation', readOnly=True, restorable=False,
                objects={}, events=[], missingState=['RNG and full native battle state'])


class RecorderProtocolTests(unittest.TestCase):
    def test_native_and_lua_producer_identities_cannot_be_confused(self):
        with tempfile.TemporaryDirectory() as folder:
            (Path(folder)/'connection').mkdir()
            native = NativeEntryRecorder(Path(folder))
            native.capture()
            report = sample_report(native)
            report['adapter'] = 'ffr-helper-native-entry-observer'
            path = Path(folder)/'connection/runtime-report.json'
            path.write_text(json.dumps(report))
            self.assertIsNotNone(native.report())
            report['adapter'] = 'ffr-helper-runtime-observer'
            path.write_text(json.dumps(report))
            self.assertIsNone(native.report())

    def test_report_matches_current_request_session_and_time(self):
        with tempfile.TemporaryDirectory() as folder:
            (Path(folder)/'connection').mkdir()
            recorder = RuntimeRecorder(Path(folder))
            recorder.capture()
            path = Path(folder)/'connection/runtime-report.json'
            original = sample_report(recorder)
            path.write_text(json.dumps(original))
            self.assertEqual(recorder.report(), original)
            for key, value in [('request', 'f'*32), ('session', '0'*32), ('time', int(time.time())-60),
                               ('time', True), ('readOnly', False), ('restorable', True)]:
                changed = dict(original, **{key: value})
                path.write_text(json.dumps(changed))
                self.assertIsNone(recorder.report(), key)

    def test_reports_cannot_be_exported_as_saves_or_checkpoints(self):
        with tempfile.TemporaryDirectory() as folder:
            (Path(folder)/'connection').mkdir()
            recorder = RuntimeRecorder(Path(folder))
            report = sample_report(recorder)
            for name in ['GameSaveData10.sav', 'sample.ffrsnapshot']:
                path = Path(folder)/name
                with self.assertRaises(ValueError):
                    export_report(path, report)
                self.assertFalse(path.exists())
            export_report(Path(folder)/'sample.ffrruntime.json', report)
            with self.assertRaises(ValueError):
                validate_report(dict(report, restorable=True))

    def test_oversized_and_nonfinite_reports_are_rejected(self):
        with tempfile.TemporaryDirectory() as folder:
            (Path(folder)/'connection').mkdir()
            recorder = RuntimeRecorder(Path(folder))
            report = sample_report(recorder)
            report['objects'] = {'test': float('nan')}
            with self.assertRaises(ValueError): validate_report(report)
            report['objects'] = {'test': 'x'*524288}
            with self.assertRaises(ValueError): validate_report(report)
            path = Path(folder)/'connection/runtime-report.json'
            path.write_bytes(b' '*524289)
            self.assertIsNone(recorder.report())
            path.write_text('['*1500+'0'+']'*1500)
            self.assertIsNone(recorder.report())

    def test_install_is_separate_from_field_save_and_never_overwrites(self):
        with tempfile.TemporaryDirectory() as folder:
            mods = Path(folder)/'Mods'
            mods.mkdir()
            field = mods/'FFRHelper/Scripts'
            field.mkdir(parents=True)
            existing = field/'main.lua'
            existing.write_text('Existing field-save adapter')
            target = prepare_recorder(mods)
            self.assertEqual(target.name, 'FFRHelperRuntime')
            self.assertTrue((target/'enabled.txt').exists())
            self.assertEqual(existing.read_text(), 'Existing field-save adapter')
            with self.assertRaises(ValueError): prepare_recorder(mods)
            recorder = RuntimeRecorder(target)
            recorder.capture()
            self.assertFalse((target/'connection/control.txt').exists())
            recorder.disconnect()
            self.assertEqual((target/'connection/observe.txt').read_text().splitlines()[2], '0')


@unittest.skipIf(LuaRuntime is None, 'Lua recorder tests require the optional lupa dependency')
class NativeRecorderTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.root = Path(self.temp.name)
        mods = self.root/'Mods'
        mods.mkdir()
        self.target = prepare_recorder(mods)
        self.recorder = RuntimeRecorder(self.target)
        self.lua = LuaRuntime()
        self.lua.execute('''
            hooks = {}; registered = 0; reads = 0; unitHp=123; ambiguous=false; functionsLoaded=true
            function remote(value) return {get=function() return value end} end
            function arr(values)
                return {
                    GetArrayNum=function() return #values end,
                    ForEach=function(self, callback)
                        for index,value in ipairs(values) do callback(index, remote(value)) end
                    end
                }
            end
            local world={IsValid=function() return true end}
            function object(className, name, properties)
                properties = properties or {}
                properties.IsValid=function() return true end
                properties.GetWorld=function() return world end
                properties.GetFName=function() return {ToString=function() return name or className.."_0" end} end
                properties.IsA=function(self, class) return class=="/Script/mucha."..className end
                return properties
            end
            local parameter=object("CPP_BtlUnitParameter", nil, {
                GetUnitId=function() reads=reads+1;return 12345 end,
                GetHitPoint=function() reads=reads+1;return unitHp end,
                GetLevel=function() return 1 end
            })
            local ai=object("CPP_BtlUnitAI", nil, {m_SkillId=1,m_ScheduledSkillId=2,m_StatusChangedSkillId=3,
                m_TargetList=arr({}),m_SkillStackData=arr({}),GetTurnNum=function() return 1 end})
            local unit=object("CPP_BtlUnit", nil, {m_pUnitParameter=parameter,m_pAI=ai})
            local managers={
                CPP_BtlManager=object("CPP_BtlManager", nil, {
                    m_pUnitManager={m_UnitList=arr({unit}),m_pTimelineScheduler={m_AdditionalTimeline=arr({1,2})}},
                    m_pBattleJobManager={m_WaitingJobList=arr({}),m_WorkingJobList=arr({})}
                }),
                CPP_UI_BattleSubsystem=object("CPP_UI_BattleSubsystem", nil, {bDuringBattle=true,BtlPhaseType=1}),
                CPP_MuchaSaveDataManager=object("CPP_MuchaSaveDataManager", nil, {
                    m_pGameSaveData=object("CPP_MuchaGameSaveData", nil, {
                        unitParameterList=arr({1,2}),inventory=arr({1,2,3}),visionList=arr({1})
                    })
                })
            }
            function FindAllOf(name)
                local actual=managers[name]
                if not actual then return {} end
                if ambiguous then return {actual,actual} end
                return {object(name,"Default__"..name), actual}
            end
            function StaticFindObject(name)
                return {IsValid=function() return functionsLoaded end}
            end
            function RegisterHook(path, pre, post)
                registered=registered+1;hooks[path]={pre=pre,post=post};return 1,2
            end
            function LoopInGameThreadWithDelay(ms, callback) testPoll=callback end
            function opening()
                local entry={battleGroupId=15,battleStageId=2,enemiesUnitIdList=arr({444}),
                    friendliesUnitIdList=arr({1001}),canEscape=true}
                hooks["/Script/mucha.CPP_MapManager:Encount"].pre(remote(nil),remote(entry))
                -- Mutating the source later must not alter the event's detached data.
                entry.battleGroupId=999
            end
        ''')
        self.lua.execute('dofile(...)', str(self.target/'Scripts/main.lua').replace('\\', '/'))

    def tearDown(self):
        self.temp.cleanup()

    def poll(self):
        self.lua.globals().testPoll()

    def test_disconnected_recorder_does_not_hook_or_observe(self):
        self.poll()
        self.assertEqual(self.lua.globals().registered, 0)
        self.assertEqual(self.lua.globals().reads, 0)
        self.assertFalse((self.target/'connection/runtime-report.json').exists())

    def test_report_reads_current_values_and_records_detached_opening(self):
        self.recorder.heartbeat()
        self.poll()
        self.assertIsNotNone(self.recorder.status())
        self.lua.globals().opening()
        self.recorder.capture()
        self.poll()
        report = self.recorder.report()
        self.assertIsNotNone(report)
        self.assertFalse(report['restorable'])
        self.assertEqual(report['events'][0]['entry']['battleGroupId'], 15)
        self.assertEqual(report['events'][0]['entry']['enemiesUnitIdList'], [444])
        self.assertEqual(report['objects']['battle']['units'][0]['values']['GetHitPoint'], 123)
        self.assertEqual(report['objects']['progression']['inventoryCount'], 3)
        self.assertEqual(report['objects']['battle']['additionalTimelineCount'], 2)
        self.lua.globals().unitHp = 99
        self.recorder.capture()
        self.poll()
        self.assertEqual(self.recorder.report()['objects']['battle']['units'][0]['values']['GetHitPoint'], 99)

    def test_hooks_never_override_returns_and_stop_recording_after_expiry(self):
        self.recorder.heartbeat()
        self.poll()
        hooks = self.lua.globals().hooks
        self.assertEqual(self.lua.globals().registered, 3)
        self.recorder.disconnect()
        for _, hook in hooks.items():
            self.assertIsNone(hook['pre']())
            self.assertIsNone(hook['post']())
        self.lua.globals().opening()
        self.recorder.capture()
        self.poll()
        self.assertEqual(self.recorder.report()['events'], [])
        self.assertEqual(self.lua.globals().registered, 3)
        self.assertFalse((self.target/'connection/control.txt').exists())

    def test_ambiguous_world_objects_are_reported_without_selection(self):
        self.lua.globals().ambiguous = True
        self.recorder.capture()
        self.poll()
        report = self.recorder.report()
        self.assertEqual(report['objects']['battle']['availability'], 'ambiguous')
        self.assertNotIn('units', report['objects']['battle'])

    def test_missing_reflection_does_not_break_the_observer_protocol(self):
        self.lua.execute('function FindAllOf(name) error("Unavailable reflection") end')
        self.recorder.capture()
        self.poll()
        self.assertIsNotNone(self.recorder.status())
        self.assertEqual(self.recorder.report()['objects']['battle']['availability'], 'unavailable')

    def test_event_buffer_is_bounded_and_new_sessions_clear_history(self):
        self.recorder.heartbeat()
        self.poll()
        for _ in range(40): self.lua.globals().opening()
        self.recorder.capture()
        self.poll()
        report = self.recorder.report()
        self.assertEqual(len(report['events']), 32)
        self.assertEqual(report['events'][0]['sequence'], 9)
        other = RuntimeRecorder(self.target)
        other.capture()
        self.poll()
        self.assertEqual(other.report()['events'], [])
        self.assertIsNone(self.recorder.report())

    def test_hook_discovery_retries_when_functions_load_late(self):
        self.lua.globals().functionsLoaded = False
        self.recorder.heartbeat()
        self.poll()
        self.assertEqual(self.lua.globals().registered, 0)
        self.lua.globals().functionsLoaded = True
        # Advance the recorder's time while refreshing the desktop lease in kind.
        self.lua.execute('local original=os.time;os.time=function() return original()+6 end')
        from unittest.mock import patch
        with patch('ffr_helper.runtime.time.time', return_value=time.time()+6):
            self.recorder.heartbeat()
            self.poll()
        self.assertEqual(self.lua.globals().registered, 3)


if __name__ == '__main__':
    unittest.main()
