"""Offline component comparisons; equal captures do not prove a battle restart."""
import hashlib
import json
import math
from pathlib import Path
import struct

from .opening import OpeningCapture
from .playtime_plan import plan_opening_playtime
from .storage import atomic_write

ARRAYS=['friendliesUnitIdList','friendliesLocationIdList','enemiesUnitIdList','enemiesLocationIdList']
FLAGS=['canEscape','isResultSkipOnWin','isResultSkipOnLose','isNotifyPreemtiveAttack',
       'isStartupByBattleTest','isContinueBGM','isBounty','skipIntro']
INTEGERS=['battleGroupId','battleStageId','battleEventId','ap','regionId']
KEYS=set(ARRAYS+FLAGS+INTEGERS+['addPassiveDataList','bgmCueName','preemptiveRelation','probabilityOfSuccessfulEscape'])


def _integer(value):return type(value) is int and -(2**31)<=value<2**31


def validate_complete_entry(entry):
    """The complete captured entry struct, not complete combat/world state."""
    if not isinstance(entry,dict) or set(entry)!=KEYS:
        raise ValueError('Comparison requires every captured encounter-entry field; this legacy/partial entry has insufficient coverage.')
    for key in ARRAYS:
        if not isinstance(entry[key],list) or len(entry[key])>64 or any(not _integer(v) for v in entry[key]):
            raise ValueError('Invalid bounded encounter-entry ID/location array.')
    groups=entry['addPassiveDataList']
    if (not isinstance(groups,list) or len(groups)>64 or any(not isinstance(g,list) or len(g)>128
            or any(not _integer(v) for v in g) for g in groups)):
        raise ValueError('Invalid bounded nested encounter passives.')
    if any(type(entry[k]) is not bool for k in FLAGS) or any(not _integer(entry[k]) for k in INTEGERS):
        raise ValueError('Invalid typed encounter flags or scalar IDs.')
    if type(entry['preemptiveRelation']) is not int or not 0<=entry['preemptiveRelation']<=255:
        raise ValueError('Invalid encounter preemption byte.')
    probability=entry['probabilityOfSuccessfulEscape']
    if (type(probability) not in (int,float) or not -3.4028234663852886e38<=probability<=3.4028234663852886e38
            or not math.isfinite(probability)):
        raise ValueError('Invalid encounter probability float.')
    try:
        if not math.isfinite(struct.unpack('<f',struct.pack('<f',probability))[0]):raise ValueError('Unbounded encounter probability.')
    except (OverflowError,struct.error) as error:raise ValueError('Unbounded encounter probability.') from error
    name=entry['bgmCueName']
    if not isinstance(name,str) or '\0' in name:
        raise ValueError('Invalid encounter BGM string.')
    try:
        if len(name.encode('utf-16le'))>510:raise ValueError('Encounter BGM exceeds the native FString bound.')
        name.encode('utf8')
    except UnicodeError as error:raise ValueError('Encounter BGM contains invalid Unicode.') from error
    return json.loads(json.dumps(entry,allow_nan=False))


def _metadata_hash(capture):
    return hashlib.sha256(json.dumps(capture.metadata,sort_keys=True,allow_nan=False,separators=(',',':')).encode('utf8')).hexdigest()


def compare_openings(source:OpeningCapture,observed:OpeningCapture):
    if not isinstance(source,OpeningCapture) or not isinstance(observed,OpeningCapture):
        raise ValueError('Two validated native opening captures are required.')
    source.validate();observed.validate()
    before=validate_complete_entry(source.metadata['entry']);after=validate_complete_entry(observed.metadata['entry'])
    changed=[]
    for key in sorted(KEYS):
        equal=(struct.pack('<f',before[key])==struct.pack('<f',after[key]) if key=='probabilityOfSuccessfulEscape' else before[key]==after[key])
        if not equal:changed.append(key)
    expected_hash=normalized_equal=None
    try:
        plan=plan_opening_playtime(source)
        expected_hash=hashlib.sha256(plan.normalized_progression).hexdigest()
        normalized_equal=plan.normalized_progression==observed.progression
    except ValueError:
        # Legacy diagnostics stay inspectable without inventing a clock basis.
        pass
    a=source.metadata.get('fieldAnchor');b=observed.metadata.get('fieldAnchor');field=None
    if a is not None and b is not None:
        field=dict(mapIdEqual=a['mapId']==b['mapId'],positionEqual=a['position']==b['position'],directionEqual=a['direction']==b['direction'])
    a_cache=source.metadata.get('mapUnitCache');b_cache=observed.metadata.get('mapUnitCache')
    a_rng=source.metadata.get('crtRandState');b_rng=observed.metadata.get('crtRandState')
    return dict(format='ffr-helper-opening-comparison',version=1,kind='offline-component-comparison',restorable=False,
        restartVerified=False,independentCaptureVerified=False,gameSha256=source.metadata['gameSha256'],
        sourceSha256=source.metadata['sha256'],observedSha256=observed.metadata['sha256'],
        sourceMetadataSha256=_metadata_hash(source),observedMetadataSha256=_metadata_hash(observed),
        sourceSequence=source.metadata['sequence'],observedSequence=observed.metadata['sequence'],
        encounterEntryEqual=not changed,changedEntryFields=changed,
        rawProgressionEqual=source.progression==observed.progression,
        sourceNormalizationSupported=expected_hash is not None,expectedNormalizedSha256=expected_hash,
        normalizedProgressionEqual=normalized_equal,fieldAnchorComparison=field,
        mapUnitCacheEqual=(a_cache==b_cache if a_cache is not None and b_cache is not None else None),
        crtRandStateEqual=(a_rng==b_rng if a_rng is not None and b_rng is not None else None),
        missingState=['Evidence that a native restart actually executed','Independent capture provenance',
                      'Complete live resources, scheduler, AI, effects and RNG','World/lifecycle restoration and external effects'])


def export_opening_comparison(path:Path,source:OpeningCapture,observed:OpeningCapture):
    if not str(path).lower().endswith('.ffrcomparison.json'):
        raise ValueError('Use .ffrcomparison.json; this receipt is not a save state.')
    report=compare_openings(source,observed)
    atomic_write(Path(path),json.dumps(report,indent=2,allow_nan=False).encode('utf8'))
    return report
