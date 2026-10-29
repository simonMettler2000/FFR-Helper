"""Client for the detached engine import comparison; no restoration commands."""
from __future__ import annotations

import time
import uuid

from .abort_trial import AbortTrialBridge


class ImportProbeBridge:
    def __init__(self, recorder):
        self.bridge = AbortTrialBridge(recorder)
        self.request = None

    def status(self):
        status = self.bridge.status()
        if status is None:
            return None
        probe = status.get('importProbe')
        if (not isinstance(probe, dict) or probe.get('restorable') is not False
                or probe.get('boundaryPolicy') != 'ordinary-idle-menu'
                or probe.get('phase') not in {'idle', 'running', 'rejected', 'passed', 'failed'}
                or any(type(probe.get(key)) is not bool for key in
                       ['available', 'loaderReady', 'passed', 'cloneMatches', 'liveUnchanged', 'inputUnchanged', 'pinsReleased'])
                or not isinstance(probe.get('message'), str) or len(probe['message']) > 1000
                or type(probe.get('byteLength')) is not int or not 0 <= probe['byteLength'] <= 16*1024*1024):
            return None
        return status

    @staticmethod
    def safe_guard(status,sequence,digest):
        guard=status.get('guard',{})
        return (status.get('importProbe',{}).get('boundaryPolicy')=='ordinary-idle-menu'
            and type(status.get('probeTime')) is int and -2<=time.time()-status['probeTime']<=2
            and all(guard.get(k) is True for k in ['gameThread','matchingWorld','ordinaryEncounter','battleActive','commandMenu'])
            and type(guard.get('units')) is int and 0<guard['units']<=64
            and all(type(guard.get(k)) is int and guard[k]==0 for k in ['waitingJobs','workingJobs'])
            and type(guard.get('friendlyActions')) is int and guard['friendlyActions']>=0
            and type(guard.get('sequence')) is int and guard['sequence']==sequence and guard.get('openingHash')==digest)

    def send(self, report):
        if self.request is not None:
            raise ValueError('An import probe was already requested; no automatic retry is allowed.')
        capture = self.bridge.recorder.opening(report)
        status = self.status()
        if capture is None or status is None or status['phase'] != 'idle':
            raise ValueError('The separate import probe runtime is unavailable or an abort trial has started.')
        probe = status['importProbe']
        if not probe['available'] or not probe['loaderReady'] or probe['phase'] != 'idle':
            raise ValueError('The import probe requires the verified, initialized engine loader.')
        sequence, digest = capture.metadata['sequence'], capture.metadata['sha256']
        if not self.safe_guard(status, sequence, digest):
            raise ValueError('The import probe requires a fresh matching ordinary idle command menu.')
        # Store before I/O: an uncertain command write must not be resent.
        self.request = dict(nonce=uuid.uuid4().hex, deadline=int(time.time())+8, sequence=sequence, sha256=digest)
        self.bridge.request = self.request
        self.bridge._write('probe-import')
        return capture

    def receipt(self):
        status = self.status()
        if status is None or self.request is None:
            return None
        probe = status['importProbe']
        if (probe.get('nonce') != self.request['nonce'] or probe.get('session') != self.bridge.recorder.session
                or type(probe.get('sequence')) is not int or probe['sequence'] != self.request['sequence']
                or probe.get('openingHash') != self.request['sha256'] or probe['phase'] not in {'rejected', 'passed', 'failed'}):
            return None
        passed = all(probe[key] for key in ['cloneMatches', 'liveUnchanged', 'inputUnchanged', 'pinsReleased'])
        if probe['passed'] != (probe['phase'] == 'passed') or (probe['passed'] and (not passed or probe['byteLength'] < 8)):
            raise ValueError('The import-probe receipt contains inconsistent success claims.')
        return probe
