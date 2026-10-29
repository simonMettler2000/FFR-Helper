"""Shared validation for partial live position/direction evidence."""
import math


def validate_field_anchor(anchor):
    if (not isinstance(anchor, dict)
            or set(anchor) != {'kind', 'restorable', 'source', 'coverage', 'sameWorldChecked', 'mapId', 'position', 'direction'}
            or anchor.get('kind') != 'partial-field-anchor' or anchor.get('restorable') is not False
            or anchor.get('source') != 'live-player-root-transform' or anchor.get('coverage') != 'position-and-direction-only'
            or anchor.get('sameWorldChecked') is not True or type(anchor.get('mapId')) is not int
            or not 0 <= anchor['mapId'] < 2**31 or type(anchor.get('direction')) is not int
            or not 0 <= anchor['direction'] <= 8 or not isinstance(anchor.get('position'), list) or len(anchor['position']) != 3
            or any(type(v) not in (int, float) or abs(v) > 1e12 or not math.isfinite(v) for v in anchor['position'])):
        raise ValueError('Invalid partial field anchor; it cannot represent a complete world state.')
    return dict(anchor, position=list(anchor['position']))
