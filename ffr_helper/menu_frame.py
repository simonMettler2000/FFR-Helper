"""Read-only menu sampling; recency never authorizes a practice operation."""
from __future__ import annotations
import time


def validate_menu_frame(data):
    if data is None:return None
    expected={'kind','readOnly','restorable','boundaryReviewed','atomicSnapshot','openingSequence',
              'sampleTime','gameThread','matchingWorld','ordinaryEncounter','battleActive','commandMenu',
              'fieldReady','units','waitingJobs','workingJobs','turnBeginnings'}
    if type(data) is not dict or set(data)-{'managersReleased'}!=expected or data.get('kind')!='read-only-menu-frame':
        raise ValueError('Unsupported read-only menu frame.')
    for key,value in [('readOnly',True),('restorable',False),('boundaryReviewed',False),
                      ('atomicSnapshot',False),('gameThread',True),('matchingWorld',True)]:
        if data[key] is not value:raise ValueError('Menu frames cannot claim reviewed or restorable state.')
    for key in ['ordinaryEncounter','battleActive','commandMenu','fieldReady']:
        if type(data[key]) is not bool:raise ValueError('Invalid menu-frame flag.')
    for key,low,high in [('openingSequence',1,2**64-1),('sampleTime',1,2**63-1),('units',0,64),
                         ('waitingJobs',0,1024),('workingJobs',0,1024),('turnBeginnings',-1,6400000)]:
        if type(data[key]) is not int or not low<=data[key]<=high:raise ValueError('Invalid menu-frame counter.')
    if data['commandMenu'] and (not data['battleActive'] or not data['units'] or data['turnBeginnings']<0 or data['fieldReady']):
        raise ValueError('Inconsistent command-menu observation.')
    if data['fieldReady'] and (data['battleActive'] or data['units'] or data['waitingJobs'] or data['workingJobs']):
        raise ValueError('Inconsistent cleared-field observation.')
    if 'managersReleased' in data:
        if type(data['managersReleased']) is not bool:raise ValueError('Invalid released-manager flag.')
        if data['managersReleased'] and (not data['fieldReady'] or data['turnBeginnings']!=-1):
            raise ValueError('Released managers require an inactive verified field shape.')
    return dict(data)


def menu_frame_is_recent(data,now=None):
    frame=validate_menu_frame(data)
    return frame is not None and -2 <= (time.time() if now is None else now)-frame['sampleTime'] <= 2
