"""A profiled engine-global seed sample; concurrent draws/other RNG stay missing."""
import json


def validate_engine_global_rng(state):
    keys={'kind','readOnly','restorable','coverage','otherSourcesCovered','atomicSnapshot','matchingSamplesAroundProgressionCopy','seed'}
    if (not isinstance(state,dict) or set(state)!=keys or state.get('kind')!='partial-engine-global-rng-state'
            or state.get('readOnly') is not True or state.get('restorable') is not False
            or state.get('coverage')!='single-profiled-global-seed-only' or state.get('otherSourcesCovered') is not False
            or state.get('atomicSnapshot') is not False or state.get('matchingSamplesAroundProgressionCopy') is not True
            or type(state.get('seed')) is not int or not 0<=state['seed']<=0xffffffff):
        raise ValueError('Invalid partial engine RNG sample; concurrent/full RNG restoration is not established.')
    return json.loads(json.dumps(state,allow_nan=False))
