"""Partial map-unit cache metadata; never a coherent progression/restore claim."""
import json
from .unit_components import UNIT_FIELDS


def validate_map_unit_cache(cache):
    keys={'kind','source','coverage','readOnly','restorable','coherentWithProgression','units'}
    if (not isinstance(cache,dict) or set(cache)!=keys or cache.get('kind')!='partial-map-unit-cache'
            or cache.get('source')!='map-manager-unit-parameter-storage' or cache.get('coverage')!='raw-parameter-components-only'
            or cache.get('readOnly') is not True or cache.get('restorable') is not False or cache.get('coherentWithProgression') is not False
            or not isinstance(cache.get('units'),list) or len(cache['units'])>64):
        raise ValueError('Invalid partial map-unit cache coverage.')
    budget=2048
    integer=lambda v:type(v) is int and -(2**31)<=v<2**31
    for index,unit in enumerate(cache['units']):
        if (not isinstance(unit,dict) or set(unit)!={'index','raw','accessories','passives','continuousEffectCount'}
                or type(unit.get('index')) is not int or unit['index']!=index or not isinstance(unit.get('raw'),dict)
                or set(unit['raw'])!=UNIT_FIELDS or any(not integer(v) for v in unit['raw'].values()) or unit['raw']['unitId']<0
                or type(unit.get('continuousEffectCount')) is not int or not 0<=unit['continuousEffectCount']<=1024):
            raise ValueError('Invalid raw map-unit component.')
        for key,maximum in [('accessories',16),('passives',128)]:
            values=unit[key]
            if not isinstance(values,list) or len(values)>maximum or any(not integer(v) for v in values):
                raise ValueError('Invalid bounded map-unit equipment/passives.')
            budget-=len(values)
            if budget<0:raise ValueError('Map-unit cache exceeds its aggregate value budget.')
    raw=json.dumps(cache,allow_nan=False,separators=(',',':')).encode('utf8')
    if len(raw)>48*1024:raise ValueError('Map-unit cache exceeds its metadata budget.')
    return json.loads(raw)
