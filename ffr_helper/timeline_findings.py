"""Reduce private copied timeline evidence to bounded, address-free findings.

This neither establishes visible widget identity nor authorizes a repair/retry.
Inputs must come from the read-only readers in the same retained capture.
"""
from __future__ import annotations

import math


class TimelineAnalysisError(ValueError):
    pass


def analyze_timeline_widgets(links, widgets):
    """Explain stored-link disagreements without treating old widgets as current.

    The current-turn/index/identity projection selects candidates only. Even a
    selected candidate is not proven to be attached to the visible viewport.
    Returned row numbers and unit indices are local to this capture.
    """
    def report(value, kind):
        if (not isinstance(value, dict) or value.get('format') != kind
                or type(value.get('protocol')) is not int or value['protocol'] != 1
                or value.get('readOnly') is not True
                or any(value.get(key) is not False for key in
                       ('gameCalls', 'gameWrites', 'atomicSnapshot', 'restorable'))):
            raise TimelineAnalysisError('Unsupported read-only timeline evidence')

    def rows(value, key, bound):
        result = value.get(key)
        if (not isinstance(result, list) or len(result) > bound
                or any(not isinstance(row, dict) for row in result)):
            raise TimelineAnalysisError('Invalid bounded timeline evidence rows')
        return result

    def index(value):
        return type(value) is int and 0 <= value < 64

    def anchor(value):
        return (isinstance(value, list) and len(value) == 2
                and all(type(x) in (int, float) and abs(x) <= 1_000_000
                        and math.isfinite(x) for x in value))

    result = dict(format='ffr-helper-timeline-findings', protocol=1,
                  readOnly=True, gameCalls=False, gameWrites=False,
                  atomicSnapshot=False, restorable=False,
                  visibleViewportVerified=False, rootCauseConfirmed=False,
                  repairAuthorized=False, completeEnumeration=False,
                  unitLinkFindings=[], widgetFindings=[],
                  currentTurnCandidates=0, retainedOtherOrUnknown=0)
    units = {}
    if links is not None:
        report(links, 'ffr-helper-timeline-links')
        for unit in rows(links, 'units', 64):
            at = unit.get('index')
            raw = unit.get('storedRelationByte')
            if (not index(at) or at in units or type(unit.get('expectedParty')) is not bool
                    or raw is not None and (type(raw) is not int or not 0 <= raw <= 255)):
                raise TimelineAnalysisError('Invalid current unit evidence')
            units[at] = unit
        for ordinal, model in enumerate(rows(links, 'unitViewModels', 256)):
            at = model.get('currentUnitIndex')
            if at is not None and (not index(at) or at not in units):
                raise TimelineAnalysisError('Unknown current unit in VM evidence')
            findings = []
            if model.get('cachedParameterState') != 'current':
                findings.append('registry-vm-parameter-not-current')
            if model.get('sourceParameterMatchesCached') is False:
                findings.append('inherited-and-cached-parameter-disagree')
            if findings:
                result['unitLinkFindings'].append(dict(modelRow=ordinal,
                    currentUnitIndex=at, findings=findings))
    result['unitEvidenceAvailable'] = links is not None
    result['widgetEvidenceAvailable'] = widgets is not None
    if widgets is None:
        return result
    report(widgets, 'ffr-helper-timeline-widget-values')
    for ordinal, row in enumerate(rows(widgets, 'attachments', 256)):
        entry = row.get('entryBinding')
        if not isinstance(entry, dict):
            entry = {}
        turn = entry.get('turnBinding')
        if not isinstance(turn, dict):
            turn = {}
        membership = ('projection-unavailable' if entry.get('state') != 'captured'
                      or turn.get('state') != 'captured' else
                      'outside-current-turns' if turn.get('turnMatchesCurrentTimeline') is False else
                      'turn-membership-unknown' if turn.get('turnMatchesCurrentTimeline') is not True else
                      'entry-identity-mismatch' if turn.get('identityMatchesCurrentTurn') is False else
                      'entry-identity-unavailable' if turn.get('identityMatchesCurrentTurn') is not True else
                      'current-turn-candidate')
        findings = []
        unavailable = []
        at = row.get('currentUnitIndex')
        if at is not None and (not index(at) or links is not None and at not in units):
            raise TimelineAnalysisError('Invalid widget unit index')
        if membership == 'current-turn-candidate':
            result['currentTurnCandidates'] += 1
            if entry.get('cacheMatchesCurrentRegistry') is False:
                findings.append('entry-cache-disagrees-with-current-registry')
            elif entry.get('cacheMatchesCurrentRegistry') is not True:
                unavailable.append('current-registry-comparison')
            if entry.get('parameterViewModelMatchesWidget') is False:
                findings.append('portrait-source-disagrees-with-entry-cache')
            elif entry.get('parameterViewModelMatchesWidget') is not True:
                unavailable.append('entry-to-portrait-comparison')
            if row.get('viewModelState') != 'current':
                findings.append('portrait-vm-not-current')
            unit = units.get(at) if at is not None else None
            raw = unit.get('storedRelationByte') if unit is not None else None
            stored = row.get('storedContentAnchor')
            if raw is None or row.get('viewModelState') != 'current':
                unavailable.append('raw-relation-to-widget-anchor')
            elif not anchor(stored):
                raise TimelineAnalysisError('Invalid stored widget anchor')
            else:
                if unit['expectedParty'] and raw != 1:
                    findings.append('party-raw-relation-is-not-friendlies')
                # This is the statically observed SetRelation mapping applied
                # to a raw byte, not an assertion about a getter's return.
                if stored != [0.5, float(raw == 1)]:
                    findings.append('widget-anchor-disagrees-with-raw-relation-mapping')
            slate = row.get('slateAnchor')
            if not isinstance(slate, dict) or slate.get('state') != 'captured':
                unavailable.append('slate-anchor-comparison')
            elif slate.get('matchesWidget') is False:
                findings.append('slate-and-widget-stored-anchors-disagree')
            elif slate.get('matchesWidget') is not True:
                unavailable.append('slate-anchor-comparison')
        else:
            result['retainedOtherOrUnknown'] += 1
        result['widgetFindings'].append(dict(attachmentRow=ordinal,
            membership=membership, currentUnitIndex=at,
            findings=findings, unavailable=unavailable))
    return result
