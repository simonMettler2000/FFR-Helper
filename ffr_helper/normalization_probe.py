"""Explicit detached-object normalization comparison; no live load or reset API."""
import hashlib
import json
import math
import struct
import time
import uuid

from .abort_trial import AbortTrialBridge
from .playtime_plan import plan_opening_playtime

MAXIMUM=16*1024*1024
PHASES={'idle','running','pending-write','rejected','passed','failed'}
BOOLEANS=['available','loaderReady','fieldReady','attempted','passed','enginePassed','sourceValid','inputUnchanged',
          'cloneBeforeMatches','writeAttempted','writeReturned','cloneAfterMatches','liveUnchanged','rootChanged',
          'identityUncertain','pinsReleased','filesCommitted']
IDENTITY=['nonce','session','sequence','openingHash','captureTime','phase','expected','clone','normalizedSeconds']


def _hex(value,size):
    return isinstance(value,str) and len(value)==size and all(c in '0123456789abcdef' for c in value)


def _file(value,name):
    return (isinstance(value,dict) and set(value)=={'file','sha256','byteLength'} and value.get('file')==name
            and _hex(value.get('sha256'),64) and type(value.get('byteLength')) is int and 8<=value['byteLength']<=MAXIMUM)


def validate_normalization_probe(probe):
    keys=set(BOOLEANS)|{'operation','restorable','phase','nonce','session','openingHash','sequence','captureTime',
                        'retainedOwnedPins','normalizedSeconds','expected','clone','message'}
    if (not isinstance(probe,dict) or set(probe)!=keys or probe.get('operation')!='detached-playtime-normalization'
            or probe.get('restorable') is not False or any(type(probe.get(k)) is not bool for k in BOOLEANS)
            or not isinstance(probe.get('phase'),str) or probe['phase'] not in PHASES
            or type(probe.get('sequence')) is not int or not 0<=probe['sequence']<10**18
            or type(probe.get('captureTime')) is not int or not 0<=probe['captureTime']<=10**12
            or type(probe.get('retainedOwnedPins')) is not int or not 0<=probe['retainedOwnedPins']<=2
            or type(probe.get('normalizedSeconds')) not in (int,float) or not 0<=probe['normalizedSeconds']<=10**12
            or not math.isfinite(probe['normalizedSeconds']) or not isinstance(probe.get('message'),str) or len(probe['message'])>1000):
        raise ValueError('Invalid detached-normalization receipt.')
    if probe['phase']=='idle':
        if (probe['nonce']!='' or probe['session']!='' or probe['openingHash']!='' or probe['sequence'] or probe['captureTime']
                or any(probe[k] for k in ['passed','enginePassed','sourceValid','inputUnchanged','cloneBeforeMatches','writeAttempted',
                                          'writeReturned','cloneAfterMatches','liveUnchanged','rootChanged','identityUncertain','filesCommitted'])
                or probe['retainedOwnedPins'] or probe['normalizedSeconds'] or not probe['pinsReleased']):
            raise ValueError('Idle normalization contains an action identity.')
    elif (not _hex(probe['nonce'],32) or not _hex(probe['session'],32) or not _hex(probe['openingHash'],64)
          or probe['sequence']<1 or probe['captureTime']<1 or not probe['attempted']):
        raise ValueError('Detached-normalization action identity is incomplete.')
    if probe['filesCommitted']:
        if not _file(probe['expected'],'normalization-expected.gvas') or not _file(probe['clone'],'normalization-clone.gvas'):
            raise ValueError('Detached evidence has no fixed committed file pair.')
    elif probe['expected'] is not None or probe['clone'] is not None:
        raise ValueError('Uncommitted normalization cannot advertise evidence files.')
    required=['enginePassed','sourceValid','inputUnchanged','cloneBeforeMatches','writeReturned','cloneAfterMatches','liveUnchanged','pinsReleased','filesCommitted']
    if (probe['passed']!=(probe['phase']=='passed') or probe['passed'] and
            (not all(probe[k] for k in required) or probe['rootChanged'] or probe['identityUncertain'] or probe['retainedOwnedPins']
             or probe['expected']['sha256']!=probe['clone']['sha256'] or probe['expected']['byteLength']!=probe['clone']['byteLength'])):
        raise ValueError('Detached-normalization success claims are inconsistent.')
    return json.loads(json.dumps(probe,allow_nan=False))


class NormalizationProbeBridge:
    def __init__(self,recorder):
        self.control=AbortTrialBridge(recorder);self.request=None;self.source=None;self.plan=None

    def status(self):
        status=self.control.status()
        if status is None:return None
        try:validate_normalization_probe(status.get('normalizationProbe'))
        except (ValueError,TypeError,KeyError,RecursionError):return None
        return status

    @staticmethod
    def safe_boundary(status,sequence,digest):
        if status is None or status.get('phase')!='idle':return False
        probe=status.get('normalizationProbe',{});guard=status.get('guard',{})
        return (probe.get('available') is True and probe.get('loaderReady') is True and probe.get('fieldReady') is True
                and probe.get('attempted') is False and probe.get('phase')=='idle'
                and type(status.get('probeTime')) is int and -2<=time.time()-status['probeTime']<=2
                and all(guard.get(k) is True for k in ['gameThread','matchingWorld','ordinaryEncounter','fieldReady'])
                and guard.get('battleActive') is False and all(type(guard.get(k)) is int and guard[k]==0 for k in ['units','waitingJobs','workingJobs'])
                and type(sequence) is int and 0<sequence<10**18 and _hex(digest,64)
                and guard.get('sequence')==sequence and guard.get('openingHash')==digest)

    def send(self,report):
        if self.request is not None:raise ValueError('Detached normalization was already requested; no automatic retry.')
        source=self.control.recorder.opening(report);status=self.status()
        if source is None or not self.safe_boundary(status,source.metadata['sequence'],source.metadata['sha256']):
            raise ValueError('Wait at the matching cleared field with initialized loader and supported clocks.')
        self.plan=plan_opening_playtime(source);self.source=source
        self.request=dict(nonce=uuid.uuid4().hex,deadline=int(time.time())+8,sequence=source.metadata['sequence'],sha256=source.metadata['sha256'])
        self.control.request=self.request;self.control._write('probe-normalize');return source

    def receipt(self):
        status=self.status()
        if status is None or self.request is None:return None
        probe=validate_normalization_probe(status['normalizationProbe'])
        if (probe['nonce']!=self.request['nonce'] or probe['session']!=self.control.recorder.session
                or probe['sequence']!=self.request['sequence'] or probe['openingHash']!=self.request['sha256']
                or probe['phase'] not in {'rejected','passed','failed'} or not -2<=time.time()-probe['captureTime']<=5):return None
        return probe

    def evidence(self,probe):
        from .codec import SaveDocument
        probe=validate_normalization_probe(probe);current=self.receipt()
        if current is None or not probe['passed'] or any(current[k]!=probe[k] for k in IDENTITY):
            raise ValueError('A fresh matching passed normalization receipt is required.')
        blobs={}
        for key,name in [('expected','normalization-expected.gvas'),('clone','normalization-clone.gvas')]:
            with (self.control.recorder.connection/name).open('rb') as handle:raw=handle.read(MAXIMUM+1)
            descriptor=probe[key]
            if len(raw)!=descriptor['byteLength'] or hashlib.sha256(raw).hexdigest()!=descriptor['sha256']:
                raise ValueError('Detached evidence changed or failed verification.')
            document=SaveDocument.decode(raw)
            if document.envelope is not None or document.suffix!=bytes(4):raise ValueError('Detached evidence is not an in-memory GVAS object.')
            scalar=document.root['Player'].struct()['PlayTime']
            if scalar.kind.name!='FloatProperty' or len(scalar.raw)!=4 or struct.unpack('<f',scalar.raw)[0]!=probe['normalizedSeconds']:
                raise ValueError('Detached serialized time does not match its receipt.')
            blobs[key]=raw
        if self.plan is None or blobs['expected']!=self.plan.normalized_progression or blobs['clone']!=blobs['expected']:
            raise ValueError('Native normalization differs from the independent source-preserving plan.')
        after=self.receipt()
        if after is None or any(after[k]!=probe[k] for k in IDENTITY):raise ValueError('Normalization receipt changed while evidence was read.')
        return blobs
