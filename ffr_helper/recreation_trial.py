"""Explicit same-setup/current-field experiment. No rollback or restore API."""
from __future__ import annotations

import copy
import re
import time
import uuid

from .opening import OpeningCapture
from .opening_comparison import validate_complete_entry
from .field_anchor import validate_field_anchor
from .storage import atomic_write

PHASES={'idle','checking','armed','invoking','returned','verified','refused','uncertain'}
FALSE_FLAGS={'restorable','restoresInvoked','rngRestored','effectGraphsVerified','fullCacheCoherenceVerified'}
FLAGS=FALSE_FLAGS|{'available','entryAttempted','inputTransferred','nativeReturned','cleanupVerified',
                   'newEncounterVerified','firstMenuProgressionUnchanged'}


def validate_source(source):
    source.validate()
    entry=validate_complete_entry(source.metadata['entry'])
    if (source.metadata.get('fieldAnchor') is None or source.metadata.get('mapUnitCache') is None
            or not entry['friendliesUnitIdList'] or not entry['enemiesUnitIdList']
            or entry['battleEventId']!=-1 or not entry['canEscape'] or entry['preemptiveRelation']!=0
            or any(entry[k] for k in ['skipIntro','isResultSkipOnWin','isResultSkipOnLose','isStartupByBattleTest','isBounty'])):
        raise ValueError('Recreation requires a complete ordinary opening and recorded field/cache components.')


def _hex(value, length):
    return isinstance(value,str) and re.fullmatch('[0-9a-f]{'+str(length)+'}',value) is not None


def validate_recreation(probe):
    fields=FLAGS|{'operation','phase','nonce','session','sourceSequence','sourceHash','deadline','childSequence','entryFieldAnchor','message'}
    if (not isinstance(probe,dict) or set(probe)!=fields or probe.get('operation')!='same-setup-current-field'
            or any(type(probe.get(k)) is not bool for k in FLAGS)
            or any(probe[k] for k in FALSE_FLAGS) or probe.get('phase') not in PHASES
            or any(type(probe.get(k)) is not int or not 0<=probe[k]<10**12 for k in ['sourceSequence','childSequence','deadline'])
            or not isinstance(probe.get('message'),str) or len(probe['message'])>1000):
        raise ValueError('Invalid current-field recreation receipt.')
    if probe['entryFieldAnchor'] is not None:validate_field_anchor(probe['entryFieldAnchor'])
    if probe['phase'] in {'armed','invoking','returned','verified'} and probe['entryFieldAnchor'] is None:
        raise ValueError('Prepared recreation field anchor is missing.')
    if probe['phase']=='idle':
        if (probe['nonce'] or probe['session'] or probe['sourceHash'] or probe['sourceSequence'] or probe['childSequence']
                or probe['deadline'] or probe['entryFieldAnchor'] is not None or any(probe[k] for k in ['entryAttempted','inputTransferred','nativeReturned',
                                                               'newEncounterVerified','firstMenuProgressionUnchanged'])):
            raise ValueError('Idle recreation contains an action identity.')
    elif (not _hex(probe['nonce'],32) or not _hex(probe['session'],32) or not _hex(probe['sourceHash'],64)
          or probe['sourceSequence']<1 or probe['deadline']<1):
        raise ValueError('Recreation source identity is incomplete.')
    if (probe['entryAttempted']!=probe['inputTransferred'] or probe['nativeReturned'] and not probe['entryAttempted']
            or probe['entryAttempted'] and probe['phase'] not in {'returned','verified','uncertain'}
            or probe['phase'] in {'returned','verified'} and not probe['nativeReturned']
            or probe['newEncounterVerified']!=(probe['phase']=='verified')
            or probe['firstMenuProgressionUnchanged']!=probe['newEncounterVerified']
            or probe['newEncounterVerified'] and not all(probe[k] for k in
                ['entryAttempted','nativeReturned','cleanupVerified','firstMenuProgressionUnchanged'])
            or probe['phase']=='verified' and not probe['newEncounterVerified']
            or probe['newEncounterVerified'] and probe['childSequence']!=probe['sourceSequence']+1):
        raise ValueError('Recreation outcome is inconsistent.')
    return copy.deepcopy(probe)


class RecreationTrialBridge:
    def __init__(self, abort_bridge):
        self.abort=abort_bridge
        self.recorder=abort_bridge.recorder
        self.request=None
        self.source=None
        self.commit_attempted=False
        self.entry_field=None

    def probe(self,status):
        return validate_recreation(status.get('recreationTrial'))

    def _field(self,status):
        if not self.abort._matching(status) or status['phase']!='manager-cleared':
            return False
        guard=status['guard'];native=status.get('nativeTaskExit',{})
        return (status.get('openingBoundaryPolicy')=='ordinary-first-menu-force-end-task'
                and type(status.get('probeTime')) is int and -2<=time.time()-status['probeTime']<=2
                and all(guard.get(k) is True for k in ['gameThread','matchingWorld','ordinaryEncounter','fieldReady',
                                                       'managersReleased','openingHistoryUntouched'])
                and guard.get('battleActive') is False and guard.get('commandMenu') is False
                and all(type(guard.get(k)) is int and guard[k]==0 for k in ['units','waitingJobs','workingJobs'])
                and type(guard.get('sequence')) is int and guard['sequence']==self.abort.request['sequence']
                and guard.get('openingHash')==self.abort.request['sha256']
                and native.get('uncertain') is False
                and all(native.get(k) is True for k in ['endIssued','forceEndFlagObserved','uiEndIssued','uiClosedObserved',
                                                        'resultTaskObserved','forceEndResultObserved']))

    def _write(self,phase):
        r=self.request
        text='\n'.join(['FFR_HELPER_ABORT_OPENING_V1',self.recorder.session,r['nonce'],str(r['deadline']),
                         str(r['sequence']),r['sha256'],phase,''])
        atomic_write(self.recorder.connection/'abort-request.txt',text.encode('ascii'))

    def prepare(self,status,source):
        if self.request is not None:
            raise ValueError('Recreation was already requested; no retry is allowed.')
        validate_source(source)
        current=self.abort.status()
        if current is None or not self._field(status) or not self._field(current):
            raise ValueError('Fresh matching verified exit/field receipts are required.')
        probe=self.probe(current)
        if not probe['available'] or probe['phase']!='idle':
            raise ValueError('The separate recreation experiment is unavailable or already used.')
        if (source.metadata['sequence']!=self.abort.request['sequence']
                or source.metadata['sha256']!=self.abort.request['sha256']
                or source.metadata.get('fieldAnchor') is None or source.metadata.get('mapUnitCache') is None
                or self.abort.progression(current)!=source.progression
                or self.abort.progression(current,after=True)!=source.progression):
            raise ValueError('Recreation needs the unchanged captured source and complete recorded field/cache components.')
        self.source=OpeningCapture(copy.deepcopy(source.metadata),bytes(source.progression))
        self.request=dict(nonce=uuid.uuid4().hex,deadline=int(time.time())+8,
                          sequence=source.metadata['sequence'],sha256=source.metadata['sha256'])
        self._write('prepare-recreate')

    def matching(self,status):
        probe=self.probe(status)
        r=self.request
        return bool(r and probe['session']==self.recorder.session and probe['nonce']==r['nonce']
                    and probe['sourceSequence']==r['sequence'] and probe['sourceHash']==r['sha256']
                    and probe['deadline']==r['deadline']
                    and (self.entry_field is None or probe['entryFieldAnchor']==self.entry_field))

    def commit(self,status):
        if self.commit_attempted:
            raise ValueError('Recreation commit was already attempted; no retry is allowed.')
        current=self.abort.status()
        if (current is None or not self.matching(status) or not self.matching(current)
                or self.probe(status)['phase']!='armed' or self.probe(current)['phase']!='armed'
                or not self._field(current) or time.time()>self.request['deadline']):
            raise ValueError('Recreation preparation expired or changed; no entry requested.')
        if (self.abort.progression(current)!=self.source.progression
                or self.abort.progression(current,after=True)!=self.source.progression):
            raise ValueError('Recreation recovery/source bytes changed.')
        field=self.probe(current)['entryFieldAnchor'];source_field=self.source.metadata['fieldAnchor']
        if field!=self.probe(status)['entryFieldAnchor'] or field['mapId']!=source_field['mapId']:
            raise ValueError('Prepared entry field/map changed; no entry requested.')
        self.entry_field=copy.deepcopy(field)
        self.commit_attempted=True
        self._write('commit-recreate')

    def compare_child(self,status,child):
        if not self.commit_attempted or not self.matching(status) or self.probe(status)['phase']!='verified':
            raise ValueError('A matching verified recreation receipt is required.')
        child.validate();source=self.source
        if (child.metadata['sequence']!=source.metadata['sequence']+1
                or child.metadata['sequence']!=self.probe(status)['childSequence']
                or child.metadata['gameSha256']!=source.metadata['gameSha256']
                or child.metadata['entry']!=source.metadata['entry'] or child.progression!=source.progression
                or child.metadata.get('fieldAnchor')!=self.entry_field
                or child.metadata.get('mapUnitCache')!=source.metadata.get('mapUnitCache')):
            raise ValueError('The child capture does not match the retained setup/progression/recorded field components.')
        return dict(currentFieldRecreationVerified=True,setupUnchanged=True,progressionBytesUnchanged=True,
                    firstMenuProgressionUnchanged=True,recordedMapCacheComponentsUnchanged=True,
                    childMatchesPreparedExitField=True,
                    originalFieldAnchorUnchanged=self.entry_field==source.metadata['fieldAnchor'],worldPositionRestored=False,
                    sourceSequence=source.metadata['sequence'],childSequence=child.metadata['sequence'],
                    battleRestartImplemented=False,progressionRestored=False,rngRestored=False,
                    effectGraphsVerified=False,fullCacheCoherenceVerified=False)
