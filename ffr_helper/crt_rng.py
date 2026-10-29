"""One profiled Windows CRT rand seed, never complete RNG or a restore format."""
import json

SUPPORTED_CRT_SHA256='dedb7611e1abc1ff3852dcac3d4c93a048d729e25b061973b9969f22698f14ca'


def validate_crt_rand_state(state):
    keys={'kind','readOnly','restorable','coverage','otherSourcesCovered','stableAcrossProgressionCopy','moduleSha256','stateIndex','seed'}
    if (not isinstance(state,dict) or set(state)!=keys or state.get('kind')!='partial-crt-rand-state'
            or state.get('readOnly') is not True or state.get('restorable') is not False
            or state.get('coverage')!='current-game-thread-crt-rand-only' or state.get('otherSourcesCovered') is not False
            or state.get('stableAcrossProgressionCopy') is not True or state.get('moduleSha256')!=SUPPORTED_CRT_SHA256
            or type(state.get('stateIndex')) is not int or state['stateIndex'] not in (0,1)
            or type(state.get('seed')) is not int or not 0<=state['seed']<=0xffffffff):
        raise ValueError('Unsupported partial CRT rand state; complete RNG/restoration is not established.')
    return json.loads(json.dumps(state,allow_nan=False))
