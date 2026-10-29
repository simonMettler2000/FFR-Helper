"""Validate partial Skill.Begin observations without granting retry authority."""


def validate_skill_history(data):
    if data is None:return None
    expected={'kind','readOnly','restorable','coverageReviewed','trackedFromNativeEntry','sequence',
              'skillBeginEvents','foreignThreadObserved','overflow'}
    if type(data) is not dict or set(data)!=expected or data.get('kind')!='skill-begin-event-history':
        raise ValueError('Unsupported skill-event history.')
    for key,value in [('readOnly',True),('restorable',False),('coverageReviewed',False),('trackedFromNativeEntry',True)]:
        if data[key] is not value:raise ValueError('Skill history cannot claim reviewed or restorable state.')
    for key,low,high in [('sequence',1,2**64-1),('skillBeginEvents',0,1000000)]:
        if type(data[key]) is not int or not low<=data[key]<=high:raise ValueError('Invalid skill-history count.')
    if any(type(data[k]) is not bool for k in ['foreignThreadObserved','overflow']):
        raise ValueError('Invalid skill-history coverage flag.')
    if data['overflow'] and data['skillBeginEvents']!=1000000:raise ValueError('Invalid skill-history overflow.')
    return dict(data)
