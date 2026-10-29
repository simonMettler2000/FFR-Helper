"""Bounded partial combat observations. These files can never be restored."""
from __future__ import annotations

import json
from pathlib import Path
import re
import time
import uuid

from .game_build import GAME_SHA256

MAX_COMBAT_REPORT = 512*1024
RAW_FIELDS = {'unitId', 'baseLevel', 'hpBase', 'mp', 'stagger', 'limitBurst', 'maxHp', 'maxMp', 'maxStagger',
              'attack', 'defence', 'intelligence', 'mind', 'divine', 'agility', 'weaponId', 'armorId', 'visionId',
              'temporaryHp', 'actionCount', 'thinkCount', 'receivedDamageCount', 'breakCount', 'duplicationId'}
EFFECT_FIELDS={'effectId','endActionCount','addProbability','activationProbability','conditionParam1','conditionParam2',
               'conditionParam3','lifeTurn','effectType','applyTiming','proceedTiming','unremovable','conditionType','statusCondition','executingReaction'}
EFFECT_BYTES={'effectType','applyTiming','proceedTiming','conditionType','statusCondition'}
MISSING = ['Complete scheduler state and native callbacks', 'Continuous effects and status durations',
           'Resistance maps and calculated modifiers', 'Shared AI state, phases and other native counters',
           'Native RNG state and draw order', 'Actor lifecycle and world restoration', 'Verified restoration path']


def _integer(value, minimum=-(2**31), maximum=2**31-1):
    return type(value) is int and minimum <= value <= maximum


def _hex(value, size):
    return isinstance(value, str) and re.fullmatch('[0-9a-f]{'+str(size)+'}', value) is not None


def validate_observation(data):
    keys = {'kind', 'restorable', 'readOnly', 'referenceEncoding', 'battleTurn', 'selectedUnit',
            'targetUnits', 'additionalTimeline', 'units', 'missingState'}
    if (not isinstance(data, dict) or set(data)-{'unitRelations','continuousEffects'} != keys or data.get('kind') != 'partial-combat-observation'
            or data.get('restorable') is not False or data.get('readOnly') is not True
            or data.get('referenceEncoding') != 'capture-local-unit-index; -1 is null'
            or data.get('missingState') != MISSING or not isinstance(data.get('units'), list)
            or not 1 <= len(data['units']) <= 64 or not _integer(data.get('battleTurn'), 0)):
        raise ValueError('Unsupported or misleading partial combat observation.')
    count = len(data['units'])
    if not _integer(data.get('selectedUnit'), 0, count-1):
        raise ValueError('The selected unit is not in this capture.')
    references = stacks = 0

    def targets(values, maximum):
        nonlocal references
        if (not isinstance(values, list) or len(values) > maximum
                or any(not _integer(value, -1, count-1) for value in values)):
            raise ValueError('Combat observation has an unresolved or invalid actor reference.')
        references += len(values)
        if references > 4096:
            raise ValueError('Combat observation exceeds its reference budget.')

    targets(data['targetUnits'], 64)
    targets(data['additionalTimeline'], 256)
    for index, unit in enumerate(data['units']):
        if (not isinstance(unit, dict) or set(unit) != {'index', 'raw', 'accessories', 'passives', 'continuousEffectCount', 'ai'}
                or type(unit.get('index')) is not int or unit['index'] != index
                or not isinstance(unit.get('raw'), dict) or set(unit['raw']) != RAW_FIELDS
                or any(not _integer(value) for value in unit['raw'].values()) or unit['raw']['unitId'] < 0
                or not _integer(unit.get('continuousEffectCount'), 0, 1024)):
            raise ValueError('Invalid detached combat unit or raw parameter component.')
        for key, maximum in [('accessories', 16), ('passives', 128)]:
            if not isinstance(unit[key], list) or len(unit[key]) > maximum or any(not _integer(v) for v in unit[key]):
                raise ValueError('Invalid combat equipment/passive collection.')
        ai = unit['ai']
        if ai is None:
            continue
        if (not isinstance(ai, dict) or set(ai) != {'skillId', 'scheduledSkillId', 'statusChangedSkillId', 'targets', 'stack'}
                or any(not _integer(ai.get(k)) for k in ['skillId', 'scheduledSkillId', 'statusChangedSkillId'])
                or not isinstance(ai.get('stack'), list)):
            raise ValueError('Invalid partial AI observation.')
        targets(ai['targets'], 64)
        stacks += len(ai['stack'])
        if stacks > 256:
            raise ValueError('Combat observation exceeds its AI stack budget.')
        for item in ai['stack']:
            if not isinstance(item, dict) or set(item) != {'skillId', 'targets'} or not _integer(item.get('skillId')):
                raise ValueError('Invalid detached AI stack entry.')
            targets(item['targets'], 64)
    relations=data.get('unitRelations')
    if relations is not None:
        if (not isinstance(relations,dict) or set(relations)!={'coverage','units'}
                or relations.get('coverage')!='capture-local-links-only' or not isinstance(relations.get('units'),list)
                or len(relations['units'])!=count):
            raise ValueError('Invalid optional capture-local unit relation graph.')
        for index,unit in enumerate(relations['units']):
            if (not isinstance(unit,dict) or set(unit)!={'index','vision','visionOwner','lastDamageDealer','damageSources','activeSkillTargets'}
                    or type(unit.get('index')) is not int or unit['index']!=index):
                raise ValueError('Invalid capture-local unit relation identity.')
            targets([unit['vision'],unit['visionOwner'],unit['lastDamageDealer']],3)
            targets(unit['damageSources'],64)
            targets(unit['activeSkillTargets'],64)
    continuous=data.get('continuousEffects')
    if continuous is not None:
        if (not isinstance(continuous,dict) or set(continuous)!={'coverage','unitEffects','effects'}
                or continuous.get('coverage')!='base-continuous-effect-components-only' or not isinstance(continuous.get('effects'),list)
                or len(continuous['effects'])>256 or not isinstance(continuous.get('unitEffects'),list) or len(continuous['unitEffects'])!=count):
            raise ValueError('Invalid optional continuous-effect base-component graph.')
        pool_count=len(continuous['effects']);seen=set();parameter_count=0
        for index,values in enumerate(continuous['unitEffects']):
            if (not isinstance(values,list) or len(values)!=data['units'][index]['continuousEffectCount'] or len(values)>1024
                    or any(not _integer(v,0,pool_count-1) for v in values)):
                raise ValueError('Continuous-effect pool indices or per-unit counts do not match.')
            references+=len(values);seen.update(values)
        if seen!=set(range(pool_count)):
            raise ValueError('Continuous-effect pool has an unreferenced object.')
        for index,effect in enumerate(continuous['effects']):
            if (not isinstance(effect,dict) or set(effect)!={'index','sourceUnit','targetUnit','raw','parameters'}
                    or type(effect.get('index')) is not int or effect['index']!=index or not isinstance(effect.get('raw'),dict)
                    or set(effect['raw'])!=EFFECT_FIELDS or any(not _integer(v) for v in effect['raw'].values())
                    or any(not _integer(effect['raw'][k],0,255) for k in EFFECT_BYTES)
                    or any(not _integer(effect['raw'][k],0,1) for k in ['unremovable','executingReaction'])
                    or not isinstance(effect.get('parameters'),list) or len(effect['parameters'])>128
                    or any(not _integer(v) for v in effect['parameters'])):
                raise ValueError('Invalid continuous-effect base scalar or parameter data.')
            parameter_count+=len(effect['parameters'])
            if parameter_count>4096:raise ValueError('Continuous effects exceed their parameter budget.')
            targets([effect['sourceUnit'],effect['targetUnit']],2)
        if references>4096:raise ValueError('Combat observation exceeds its reference budget.')
    return data


def validate_combat_report(report):
    keys = {'protocol', 'version', 'adapter', 'format', 'kind', 'readOnly', 'restorable', 'gameSha256', 'session',
            'time', 'available', 'request', 'sequence', 'openingHash', 'captureNumber', 'captureTime', 'phase', 'message', 'observation'}
    if (not isinstance(report, dict) or set(report) != keys or type(report.get('protocol')) is not int or report['protocol'] != 1
            or type(report.get('version')) is not int or report['version'] != 1
            or report.get('adapter') != 'ffr-helper-native-combat-observer' or report.get('format') != 'ffr-helper-combat-report'
            or report.get('kind') != 'diagnostic-observation' or report.get('readOnly') is not True or report.get('restorable') is not False
            or report.get('gameSha256') != GAME_SHA256 or not _hex(report.get('session'), 32)
            or not _integer(report.get('time'), 1, 2**63-1) or type(report.get('available')) is not bool
            or not isinstance(report.get('message'), str) or len(report['message']) > 1000
            or report.get('phase') not in {'idle', 'captured', 'rejected', 'failed', 'limit'}):
        raise ValueError('Unsupported combat report; this is not a restorable state.')
    if report['phase'] == 'idle':
        if (report['request'] != '' or report['openingHash'] != '' or report['observation'] is not None
                or any(not _integer(report[k], 0, 0) for k in ['sequence', 'captureNumber', 'captureTime'])):
            raise ValueError('Idle combat status contains an unexpected capture.')
    else:
        if (not _hex(report.get('request'), 32) or not _hex(report.get('openingHash'), 64)
                or not _integer(report.get('sequence'), 1, 10**18-1) or not _integer(report.get('captureNumber'), 1, 2**64-1)
                or not _integer(report.get('captureTime'), 1, report['time']+2)):
            raise ValueError('Invalid combat capture identity or timestamp.')
        if report['phase'] == 'captured':
            validate_observation(report['observation'])
        elif report['observation'] is not None:
            raise ValueError('A failed capture cannot contain accepted combat state.')
    if len(json.dumps(report, allow_nan=False).encode('utf8')) > MAX_COMBAT_REPORT:
        raise ValueError('Combat report exceeds its size limit.')
    return report


def export_combat(path: Path, report):
    from .storage import atomic_write
    validate_combat_report(report)
    if report['phase'] != 'captured' or not str(path).lower().endswith('.ffrcombat.json'):
        raise ValueError('Export a captured diagnostic as .ffrcombat.json; it cannot be a save state.')
    atomic_write(Path(path), json.dumps(report, ensure_ascii=False, allow_nan=False, separators=(',', ':')).encode('utf8'))


def load_combat(path: Path):
    with Path(path).open('rb') as handle:
        raw = handle.read(MAX_COMBAT_REPORT+1)
    if len(raw) > MAX_COMBAT_REPORT:
        raise ValueError('Combat report exceeds its size limit.')
    report = validate_combat_report(json.loads(raw))
    if report['phase'] != 'captured':
        raise ValueError('This file contains no captured combat observation.')
    return report


class CombatObservationBridge:
    """One explicit read request; use a new bridge on the same recorder for another frame."""
    def __init__(self, recorder):
        from .abort_trial import AbortTrialBridge
        self.recorder = recorder
        self.control = AbortTrialBridge(recorder)
        self.request = None

    def status(self):
        try:
            report = validate_combat_report(self.recorder._read('combat-report.json', MAX_COMBAT_REPORT))
            if report['session'] != self.recorder.session or not -2 <= time.time()-report['time'] <= 5:
                return None
            return report
        except (OSError, ValueError, TypeError, KeyError, RecursionError):
            return None

    @staticmethod
    def safe_boundary(status, sequence, digest):
        if (status is None or not status.get('available') or status.get('phase') != 'idle'
                or status.get('importProbe', {}).get('phase', 'idle') != 'idle'
                or status.get('normalizationProbe', {}).get('attempted', False)
                or not _integer(sequence, 1, 10**18-1) or not _hex(digest, 64)):
            return False
        guard = status.get('guard', {})
        return (type(status.get('probeTime')) is int and -2 <= time.time()-status['probeTime'] <= 2
                and all(guard.get(k) is True for k in ['gameThread', 'matchingWorld', 'ordinaryEncounter', 'battleActive', 'commandMenu'])
                and _integer(guard.get('units'), 1, 64) and _integer(guard.get('friendlyActions'), 0)
                and all(type(guard.get(k)) is int and guard[k] == 0 for k in ['waitingJobs', 'workingJobs'])
                and type(guard.get('sequence')) is int and guard['sequence'] == sequence and guard.get('openingHash') == digest)

    def send(self, opening_report):
        from .storage import atomic_write
        if self.request is not None:
            raise ValueError('A combat observation was already requested; no automatic retry is allowed.')
        opening = self.recorder.opening(opening_report)
        status = self.status()
        if opening is None or status is None or not status['available']:
            raise ValueError('The separate combat observation channel is unavailable.')
        sequence, digest = opening.metadata['sequence'], opening.metadata['sha256']
        if not self.safe_boundary(self.control.status(), sequence, digest):
            raise ValueError('Wait at the matching ordinary player-command menu with idle queues.')
        self.request = dict(nonce=uuid.uuid4().hex, deadline=int(time.time())+8, sequence=sequence, sha256=digest)
        raw = ('FFR_HELPER_COMBAT_OBSERVATION_V1\n'+self.recorder.session+'\n'+self.request['nonce']+'\n'
               +str(self.request['deadline'])+'\n'+str(sequence)+'\n'+digest+'\n')
        atomic_write(self.recorder.connection/'combat-request.txt', raw.encode('ascii'))
        return opening

    def receipt(self):
        report = self.status()
        request = self.request
        if (report is None or request is None or report['request'] != request['nonce'] or report['sequence'] != request['sequence']
                or report['openingHash'] != request['sha256'] or report['phase'] == 'idle'):
            return None
        if not -2 <= time.time()-report['captureTime'] <= 5:
            return None
        return report
