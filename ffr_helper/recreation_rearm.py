"""Explicitly rearm a verified untouched child; no game-state restoration."""
from __future__ import annotations

import copy
import hashlib
import json
import re
import time
import uuid

from .abort_trial import AbortTrialBridge
from .opening import MAX_PROGRESSION
from .opening_comparison import validate_complete_entry
from .field_anchor import validate_field_anchor
from .recreation_trial import validate_recreation, validate_source
from .storage import atomic_write


def _hex(value, size):
    return isinstance(value, str) and re.fullmatch('[0-9a-f]{'+str(size)+'}', value) is not None


def validate_rearm(probe):
    fields={'operation','restorable','supported','phase','nonce','session','sequence','openingHash','deadline',
            'generation','completedGeneration','archiveGeneration','archiveSha256','archiveError','message'}
    numbers=['sequence','deadline','generation','completedGeneration','archiveGeneration']
    if (not isinstance(probe,dict) or set(probe)!=fields or probe.get('operation')!='verified-child-rearm'
            or probe.get('restorable') is not False or type(probe.get('supported')) is not bool
            or probe.get('phase') not in {'idle','accepted','refused'}
            or any(type(probe.get(k)) is not int or not 0<=probe[k]<10**12 for k in numbers)
            or probe['generation']<1 or probe['completedGeneration']>=probe['generation']
            or probe['archiveGeneration']>probe['generation']
            or any(not isinstance(probe.get(k),str) or len(probe[k])>1000 for k in ['message','archiveError'])):
        raise ValueError('Invalid verified-child rearm receipt.')
    if (probe['archiveGeneration']==0 and probe['archiveSha256']
            or probe['archiveGeneration']>0 and not _hex(probe['archiveSha256'],64)):
        raise ValueError('Incomplete immutable cycle archive identity.')
    if probe['phase']=='idle':
        if any(probe[k] for k in ['nonce','session','sequence','openingHash','deadline','completedGeneration']):
            raise ValueError('Idle rearm receipt contains an operation identity.')
    elif (not _hex(probe['nonce'],32) or not _hex(probe['session'],32) or not _hex(probe['openingHash'],64)
            or probe['sequence']<2 or probe['deadline']<1):
        raise ValueError('Incomplete rearm request identity.')
    if probe['phase']=='accepted' and (probe['completedGeneration']+1!=probe['generation']
            or probe['archiveGeneration']<probe['completedGeneration']):
        raise ValueError('Rearm generation does not follow its verified completed cycle.')
    return copy.deepcopy(probe)


class RecreationRearmBridge:
    def __init__(self, previous_recreation):
        self.previous=previous_recreation
        self.abort=previous_recreation.abort
        self.recorder=self.abort.recorder
        self.request=None
        self.child=None
        self.completed_generation=None
        self.archive=None
        self.archive_progression=None

    @staticmethod
    def probe(status):
        return validate_rearm(status.get('rearmTrial'))

    def can_prepare(self,status,child):
        probe=self.probe(status)
        recreation=validate_recreation(status.get('recreationTrial'))
        return (probe['supported'] and probe['archiveGeneration']==probe['generation']
                and not probe['archiveError'] and status.get('phase')=='manager-cleared'
                and recreation['phase']=='verified' and self.previous.matching(status)
                and recreation['childSequence']==child.metadata['sequence']
                and AbortTrialBridge.safe_guard(status,child.metadata['sequence'],child.metadata['sha256']))

    def _archive(self,status,child):
        probe=self.probe(status);source=self.previous.source
        directory=self.recorder.connection/'cycle-history'/self.recorder.session/f"generation-{probe['generation']}"
        with (directory/'manifest.json').open('rb') as handle:
            raw=handle.read(131073)
        if len(raw)>131072 or hashlib.sha256(raw).hexdigest()!=probe['archiveSha256']:
            raise ValueError('Immutable cycle manifest changed or is too large.')
        manifest=json.loads(raw)
        if not isinstance(manifest,dict):
            raise ValueError('Invalid immutable cycle manifest.')
        validate_complete_entry(manifest.get('entry'))
        validate_field_anchor(manifest.get('sourceFieldAnchor'))
        validate_field_anchor(manifest.get('childFieldAnchor'))
        expected=dict(format='ffr-helper-verified-opening-cycle',protocol=1,restorable=False,
            generation=probe['generation'],session=self.recorder.session,
            sourceSequence=source.metadata['sequence'],childSequence=child.metadata['sequence'],
            openingHash=source.metadata['sha256'],recreationNonce=self.previous.request['nonce'],
            abortNonce=self.abort.request['nonce'],byteLength=len(source.progression),
            entry=source.metadata['entry'],sourceFieldAnchor=source.metadata['fieldAnchor'],
            childFieldAnchor=child.metadata['fieldAnchor'],progressionUnchanged=True,
            childMenuVerified=True,fullRestorationVerified=False)
        # JSON bool and integer equality alone is insufficient (True == 1).
        if (not isinstance(manifest,dict) or manifest!=expected or set(manifest)!=set(expected)
                or any(type(manifest[k]) is not type(v) for k,v in expected.items())):
            raise ValueError('Immutable cycle manifest does not match the independently compared child.')
        with (directory/'progression.gvas').open('rb') as handle:
            progression=handle.read(MAX_PROGRESSION+1)
        if progression!=source.progression or progression!=child.progression:
            raise ValueError('Immutable cycle progression differs from the retained opening/child.')
        self.archive=bytes(raw);self.archive_progression=bytes(progression)
        return probe['generation']

    def prepare(self,status,child):
        if self.request is not None:
            raise ValueError('A rearm request was already attempted; no automatic resend is allowed.')
        validate_source(child)
        current=self.abort.status()
        if current is None or not self.can_prepare(status,child) or not self.can_prepare(current,child):
            raise ValueError('A fresh untouched verified child and immutable cycle archive are required.')
        self.previous.compare_child(current,child)
        generation=self._archive(current,child)
        self.child=copy.deepcopy(child);self.completed_generation=generation
        self.request=dict(nonce=uuid.uuid4().hex,deadline=int(time.time())+8,
            sequence=child.metadata['sequence'],sha256=child.metadata['sha256'])
        r=self.request
        text='\n'.join(['FFR_HELPER_ABORT_OPENING_V1',self.recorder.session,r['nonce'],str(r['deadline']),
                         str(r['sequence']),r['sha256'],'rearm',''])
        # Mark before I/O so an uncertain write can never be automatically sent again.
        atomic_write(self.recorder.connection/'abort-request.txt',text.encode('ascii'))

    def accepted(self,status):
        if self.request is None:
            raise ValueError('No rearm request has been sent.')
        current=self.abort.status()
        if current is None:
            return False
        for frame in [status,current]:
            probe=self.probe(frame);r=self.request
            if probe['phase']=='refused' and probe['nonce']==r['nonce']:
                raise ValueError(probe['message'])
            if probe['phase']!='accepted' or probe['nonce']!=r['nonce']:
                return False
            if (probe['session']!=self.recorder.session or probe['sequence']!=r['sequence']
                    or probe['openingHash']!=r['sha256'] or probe['deadline']!=r['deadline']
                    or probe['completedGeneration']!=self.completed_generation
                    or probe['generation']!=self.completed_generation+1
                    or probe['archiveGeneration']!=self.completed_generation
                    or hashlib.sha256(self.archive).hexdigest()!=probe['archiveSha256']
                    or frame.get('phase')!='idle' or not frame.get('available')
                    or validate_recreation(frame.get('recreationTrial'))['phase']!='idle'
                    or not AbortTrialBridge.safe_guard(frame,r['sequence'],r['sha256'])):
                raise ValueError('Rearm acknowledgment or current child context changed; no new abort requested.')
        return True
