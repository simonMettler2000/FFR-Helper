"""Controller lifecycle regression: a preemptive opening must not select/send."""
import copy
from contextlib import redirect_stdout
import io
import json
from pathlib import Path
import tempfile
import time
import unittest
from unittest.mock import patch
from ffr_helper.abort_trial import AbortTrialBridge
from test_opening import sample_opening
from tools import run_import_probe


class ImportControllerTests(unittest.TestCase):
    def test_preemptive_and_setup_frames_do_not_select_or_send_import(self):
        capture=sample_opening();capture.metadata['sequence']=2
        now=int(time.time());digest=capture.metadata['sha256'];sent=[];seen=[]
        ordinary=dict(gameThread=True,matchingWorld=True,ordinaryEncounter=True,battleActive=True,commandMenu=True,
            units=10,waitingJobs=0,workingJobs=0,friendlyActions=0,sequence=2,openingHash=digest)
        guards=[dict(ordinary,ordinaryEncounter=False,friendlyActions=1,sequence=1),
                dict(ordinary,commandMenu=False,friendlyActions=1),ordinary]
        class Recorder:
            def __init__(self,directory):self.request='0'*32;self.closed=False;self.heartbeats=0;seen.append(self)
            def heartbeat(self):self.heartbeats+=1
            def report(self):return {'testSequence':2}
            def disconnect(self):self.closed=True
        class Bridge:
            def __init__(self,recorder):self.index=0;self.bridge=AbortTrialBridge(recorder)
            def status(self):
                guard=copy.deepcopy(guards[min(self.index,len(guards)-1)]);self.index+=1
                return dict(probeTime=now,guard=guard,importProbe=dict(available=True,loaderReady=True,phase='idle',message='Ready'))
            def send(self,report):sent.append(report);return capture
            def receipt(self):return dict(passed=True,message='Matched') if sent else None
        with tempfile.TemporaryDirectory() as folder:
            output=Path(folder)/'result'
            argv=['run_import_probe.py',str(Path(folder)/'FFRHelperBattlePractice'),'--output',str(output),'--probe-import']
            with patch.object(run_import_probe,'NativeEntryRecorder',Recorder),patch.object(run_import_probe,'ImportProbeBridge',Bridge),patch.object(run_import_probe.sys,'argv',argv),patch.object(run_import_probe.time,'sleep'),redirect_stdout(io.StringIO()):
                run_import_probe.main()
            self.assertEqual(sent,[{'testSequence':2}]);self.assertTrue(seen[0].closed)
            self.assertEqual(json.loads((output/'import-receipt.json').read_text())['passed'],True)
            self.assertTrue((output/'opening.ffropening').exists())

    def test_nonzero_counter_at_supported_menu_still_refuses_without_sending(self):
        capture=sample_opening();sent=[];closed=[]
        class Recorder:
            def __init__(self,directory):pass
            def heartbeat(self):pass
            def disconnect(self):closed.append(True)
        class Bridge:
            def __init__(self,recorder):pass
            def status(self):
                return dict(importProbe=dict(available=True,loaderReady=True,phase='idle',message='Ready'),
                    guard=dict(ordinaryEncounter=True,commandMenu=True,sequence=1,openingHash=capture.metadata['sha256'],friendlyActions=1))
            def send(self,report):sent.append(report)
        with tempfile.TemporaryDirectory() as folder:
            output=Path(folder)/'result'
            argv=['run_import_probe.py',str(Path(folder)/'FFRHelperBattlePractice'),'--output',str(output),'--probe-import']
            with patch.object(run_import_probe,'NativeEntryRecorder',Recorder),patch.object(run_import_probe,'ImportProbeBridge',Bridge),patch.object(run_import_probe.sys,'argv',argv),redirect_stdout(io.StringIO()),self.assertRaisesRegex(RuntimeError,'no import request was sent'):
                run_import_probe.main()
            self.assertEqual(sent,[]);self.assertEqual(closed,[True]);self.assertFalse((output/'opening.ffropening').exists())
