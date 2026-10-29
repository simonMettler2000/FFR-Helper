"""Explicit, short-lived client for the experimental opening-abort DLL."""
from __future__ import annotations

import hashlib
from pathlib import Path
import re
import time
import uuid

from .codec import SaveDocument
from .opening import MAX_PROGRESSION
from .storage import atomic_write

PHASES = {'idle', 'recovery-pending', 'armed', 'abort-sent', 'manager-cleared', 'rejected', 'timed-out'}


class AbortTrialBridge:
    """An abort experiment, not a battle restart or a save-state restore API."""

    def __init__(self, recorder):
        self.recorder = recorder
        self.request = None
        self.commit_attempted = False

    def status(self):
        try:
            data = self.recorder._read('abort-status.json', 16384)
            if (not isinstance(data, dict) or data.get('protocol') != 1
                    or data.get('adapter') != 'ffr-helper-opening-abort-trial'
                    or data.get('format') != 'ffr-helper-abort-trial-status'
                    or data.get('runtimeMode') != 'experimental-abort-test'
                    or data.get('readOnly') is not False or data.get('restorable') is not False
                    or data.get('session') != self.recorder.session
                    or type(data.get('time')) is not int or not -2 <= time.time()-data['time'] <= 5
                    or type(data.get('available')) is not bool or data.get('phase') not in PHASES
                    or not isinstance(data.get('guard'), dict)
                    or not isinstance(data.get('message'), str) or len(data['message']) > 1000):
                return None
            return data
        except (OSError, ValueError, TypeError, KeyError, RecursionError):
            return None

    @staticmethod
    def safe_guard(status, sequence, digest):
        guard = status.get('guard', {})
        return (status.get('openingBoundaryPolicy')=='ordinary-first-menu-event-history'
                and type(status.get('probeTime')) is int and -2 <= time.time()-status['probeTime'] <= 2
                and all(guard.get(key) is True for key in
                        ['gameThread', 'matchingWorld', 'ordinaryEncounter', 'battleActive', 'commandMenu'])
                and type(guard.get('units')) is int and 0 < guard['units'] <= 64
                and all(type(guard.get(key)) is int and guard[key] == 0 for key in ['waitingJobs', 'workingJobs'])
                and type(guard.get('friendlyActions')) is int and guard['friendlyActions']>=0
                and guard.get('openingHistoryUntouched') is True
                and type(guard.get('sequence')) is int and guard['sequence'] == sequence
                and guard.get('openingHash') == digest)

    def _write(self, phase):
        request = self.request
        text = ('FFR_HELPER_ABORT_OPENING_V1\n'+self.recorder.session+'\n'+request['nonce']+'\n'
                +str(request['deadline'])+'\n'+str(request['sequence'])+'\n'+request['sha256']+'\n'+phase+'\n')
        atomic_write(self.recorder.connection/'abort-request.txt', text.encode('ascii'))

    def prepare(self, report):
        if self.request is not None:
            raise ValueError('An abort trial was already requested; no automatic retry is allowed.')
        capture = self.recorder.opening(report)
        status = self.status()
        if (capture is None or status is None or not status['available'] or status['phase'] != 'idle'
                or status.get('importProbe', {}).get('phase', 'idle') != 'idle'
                or status.get('normalizationProbe', {}).get('attempted', False)):
            raise ValueError('The separate experimental abort runtime is not ready.')
        sequence, digest = capture.metadata['sequence'], capture.metadata['sha256']
        if not self.safe_guard(status, sequence, digest):
            raise ValueError('Wait at the first player-command menu of the captured ordinary encounter.')
        self.request = dict(nonce=uuid.uuid4().hex, deadline=int(time.time())+8, sequence=sequence, sha256=digest)
        self._write('prepare')
        return capture

    def _matching(self, status):
        request = self.request
        current = self.status()
        return (request is not None and status is not None and current is not None
                and all(frame.get('nonce') == request['nonce'] and frame.get('trialSession') == self.recorder.session
                        and frame.get('sequence') == request['sequence'] for frame in [status, current])
                and status.get('phase') == current.get('phase'))

    def progression(self, status, after=False):
        if not self._matching(status):
            raise ValueError('A fresh matching abort-trial receipt is required.')
        key, filename = ('after', 'abort-after.gvas') if after else ('recovery', 'abort-recovery.gvas')
        current = self.status()
        if current is None or not self._matching(current):
            raise ValueError('The abort-trial receipt changed.')
        metadata = current.get(key, {})
        if (metadata.get('kind') != 'progression-only' or metadata.get('restorable') is not False
                or metadata.get('file') != filename or type(metadata.get('byteLength')) is not int
                or not 8 <= metadata['byteLength'] <= MAX_PROGRESSION
                or not isinstance(metadata.get('sha256'), str)
                or re.fullmatch('[0-9a-f]{64}', metadata['sha256']) is None):
            raise ValueError('The progression-only receipt is incomplete.')
        with (self.recorder.connection/filename).open('rb') as handle:
            raw = handle.read(MAX_PROGRESSION+1)
        if len(raw) != metadata['byteLength'] or hashlib.sha256(raw).hexdigest() != metadata['sha256']:
            raise ValueError('Progression recovery bytes changed or failed verification.')
        document = SaveDocument.decode(raw)
        if document.envelope is not None or document.suffix != bytes(4):
            raise ValueError('Unsupported progression recovery object.')
        for name in ['player', 'unitParameterList', 'inventory', 'visionList']:
            document.root[name]
        return raw

    def commit(self, status):
        if self.commit_attempted:
            raise ValueError('An abort commit was already attempted; check the outcome instead of retrying.')
        if not self._matching(status) or status['phase'] != 'armed' or not status.get('available'):
            raise ValueError('A prepared, current abort-trial receipt is required.')
        current = self.status()
        if current is None or not self._matching(current) or current['phase'] != 'armed':
            raise ValueError('The prepared abort-trial receipt changed.')
        if time.time() > self.request['deadline'] or not self.safe_guard(current, self.request['sequence'], self.request['sha256']):
            raise ValueError('The trial expired or the battle changed; no abort was requested.')
        recovery = self.progression(status)
        # Mark before I/O. An uncertain write can never cause an automatic resend.
        self.commit_attempted = True
        self._write('commit')
        return recovery


def compare_progression(before: bytes, after: bytes):
    """Compare progression/counters; do not infer complete combat restoration."""
    from .editor import SaveEditor
    a, b = SaveEditor.open(before), SaveEditor.open(after)
    def mrp(editor):
        return {(row['ID'].integer(), uid): value for row in editor.root['visionList'].structs()
                for uid, value in row['synchroPoints'].integer_map().items()}
    def unit_values(editor, key):
        return {uid: row[key].integer() for uid, row in editor.units().items()}
    def player(editor):
        return {key: editor.root['player'].struct()[key].integer()
                for key in ['currentMoney', 'EscapeNum', 'WinNum', 'StealSuccessNum']}
    return dict(countersUnchanged=player(a) == player(b), inventoryUnchanged=a.inventory() == b.inventory(),
                expUnchanged=unit_values(a, 'Experience') == unit_values(b, 'Experience'),
                mrpUnchanged=mrp(a) == mrp(b),
                hpUnchanged=unit_values(a, 'HitPoint') == unit_values(b, 'HitPoint'),
                mpUnchanged=unit_values(a, 'MagicPoint') == unit_values(b, 'MagicPoint'),
                restoresInvoked=False)
