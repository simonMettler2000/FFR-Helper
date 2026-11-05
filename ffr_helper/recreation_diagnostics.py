"""Conservative party-evidence gate before another opening-recreation cycle."""
from __future__ import annotations

from collections import Counter


class RecreationDiagnosticError(RuntimeError):
    pass


def require_recreated_party_links(friendlies, observation):
    """Require every opening party member in an already validated combat report.

    This checks copied actor/parameter/UI-registry links only. It cannot certify
    portrait placement, a visible widget, complete resources or restoration.
    Legacy absence of raw relation bytes remains distinct from an explicit
    unavailable read. The caller must retain the report before invoking the gate.
    """
    def integer(value, low, high):
        return type(value) is int and low <= value <= high

    if (not isinstance(friendlies, list) or not 0 < len(friendlies) <= 16
            or any(not integer(value, 1, 2**31-1) for value in friendlies)
            or len(set(friendlies)) != len(friendlies)):
        raise RecreationDiagnosticError('Opening party identities are unavailable or ambiguous; no further cycle.')
    if not isinstance(observation, dict):
        raise RecreationDiagnosticError('Child combat evidence is unavailable; no further cycle.')
    units = observation.get('units')
    if not isinstance(units, list) or not 0 < len(units) <= 64:
        raise RecreationDiagnosticError('Child units are unavailable; no further cycle.')
    ids = []
    for index, unit in enumerate(units):
        if (not isinstance(unit, dict) or type(unit.get('index')) is not int or unit['index'] != index
                or not isinstance(unit.get('raw'), dict)
                or not integer(unit['raw'].get('unitId'), 1, 2**31-1)):
            raise RecreationDiagnosticError('Child unit identities are malformed; no further cycle.')
        ids.append(unit['raw']['unitId'])
    counts = Counter(ids)
    if any(counts[identity] != 1 for identity in friendlies):
        raise RecreationDiagnosticError('Every opening party member must appear exactly once in child combat evidence; no further cycle.')
    expected = set(friendlies)
    party_indices = {index for index, identity in enumerate(ids) if identity in expected}
    timeline = observation.get('timelineUnitLinks')
    if not isinstance(timeline, dict):
        raise RecreationDiagnosticError('Child timeline links are unavailable; no further cycle.')
    links = timeline.get('links')
    missing = timeline.get('unitsWithoutCurrentViewModel')
    if (not isinstance(links, list) or len(links) > 256 or not isinstance(missing, list)
            or len(missing) > len(units)
            or any(not integer(value, 0, len(units)-1) for value in missing)):
        raise RecreationDiagnosticError('Child timeline coverage is malformed; no further cycle.')
    if party_indices.intersection(missing):
        raise RecreationDiagnosticError('Child party view models are missing; evidence saved and no further cycle.')
    represented = set()
    for link in links:
        if (not isinstance(link, dict)
                or not integer(link.get('sourceUnit'), -1, len(units)-1)
                or not integer(link.get('cachedUnit'), -1, len(units)-1)
                or type(link.get('sourceMatchesCached')) is not bool):
            raise RecreationDiagnosticError('Child timeline links are malformed; no further cycle.')
        source, cached = link['sourceUnit'], link['cachedUnit']
        cached_id = link.get('cachedUnitId')
        if cached_id is not None and not integer(cached_id, 1, 2**31-1):
            raise RecreationDiagnosticError('Child cached unit identity is malformed; no further cycle.')
        touches_party = source in party_indices or cached in party_indices or cached_id in expected
        if touches_party:
            if (link.get('cachedParameterState') != 'current' or cached not in party_indices
                    or source != cached or not link['sourceMatchesCached'] or cached_id != ids[cached]):
                raise RecreationDiagnosticError('Child party registry contains a stale or mismatched parameter link; evidence saved and no further cycle.')
            represented.add(cached)
    if represented != party_indices:
        raise RecreationDiagnosticError('Child timeline does not independently represent every opening party member; no further cycle.')
    if 'parameterRelationBytes' in timeline:
        raw = timeline['parameterRelationBytes']
        if (not isinstance(raw, list) or len(raw) != len(units)
                or any(not integer(value, 0, 255) for value in raw)):
            raise RecreationDiagnosticError('Child stored relation bytes are unavailable or malformed; evidence saved and no further cycle.')
        if any(raw[index] != 1 for index in party_indices):
            raise RecreationDiagnosticError('Child stored party relation bytes differ from Friendlies; no further cycle. Getter/rendering behavior is unverified.')
