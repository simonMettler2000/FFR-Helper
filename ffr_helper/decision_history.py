"""Validate observational handler counts; these never authorize a practice action."""
from __future__ import annotations


def validate_decision_history(data):
    if data is None:
        return None
    expected={'kind','readOnly','restorable','coverageReviewed','commandBoundaryVerified',
              'gameThreadFreshnessVerified','sequence','trackedFromNativeEntry',
              'handlerEntries','foreignThreadObserved','overflow'}
    if type(data) is not dict or set(data)!=expected:
        raise ValueError('Unsupported decision-handler history.')
    if data['kind']!='decision-handler-entry-history':
        raise ValueError('Unexpected decision-handler history kind.')
    for name,value in [('readOnly',True),('restorable',False),('coverageReviewed',False),
                       ('commandBoundaryVerified',False),('gameThreadFreshnessVerified',False),
                       ('trackedFromNativeEntry',True)]:
        if data[name] is not value:
            raise ValueError('Handler history cannot claim a verified command boundary or restoration.')
    if type(data['sequence']) is not int or not 1<=data['sequence']<=2**64-1:
        raise ValueError('Invalid opening sequence in handler history.')
    if type(data['handlerEntries']) is not int or not 0<=data['handlerEntries']<=1000000:
        raise ValueError('Invalid decision-handler entry count.')
    for name in ['foreignThreadObserved','overflow']:
        if type(data[name]) is not bool:
            raise ValueError('Invalid handler-history coverage flag.')
    if data['overflow'] and data['handlerEntries']!=1000000:
        raise ValueError('Inconsistent handler-history overflow.')
    return dict(data)
