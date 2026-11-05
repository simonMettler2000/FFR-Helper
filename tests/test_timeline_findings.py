import copy
import json
import unittest

from ffr_helper.timeline_findings import analyze_timeline_widgets, TimelineAnalysisError


class TimelineFindingsTests(unittest.TestCase):
    def setUp(self):
        flags=dict(protocol=1,readOnly=True,gameCalls=False,gameWrites=False,
                   atomicSnapshot=False,restorable=False)
        self.links=dict(flags,format='ffr-helper-timeline-links',
            units=[dict(index=0,expectedParty=True,storedRelationByte=1)],
            unitViewModels=[dict(currentUnitIndex=0,cachedParameterState='current',
                                 sourceParameterMatchesCached=True)])
        self.row=dict(privateWidget=0x12345678,privateViewModel=0x23456789,
            currentUnitIndex=0,viewModelState='current',storedContentAnchor=[0.5,1.0],
            entryBinding=dict(state='captured',cacheMatchesCurrentRegistry=True,
                parameterViewModelMatchesWidget=True,
                turnBinding=dict(state='captured',turnMatchesCurrentTimeline=True,
                                 identityMatchesCurrentTurn=True)),
            slateAnchor=dict(state='captured',matchesWidget=True))
        self.widgets=dict(flags,format='ffr-helper-timeline-widget-values',attachments=[self.row])

    def test_matching_current_candidate_never_proves_visibility_or_authorizes_repair(self):
        result=analyze_timeline_widgets(self.links,self.widgets)
        self.assertEqual(result['currentTurnCandidates'],1)
        self.assertEqual(result['widgetFindings'][0]['findings'],[])
        self.assertEqual(result['widgetFindings'][0]['unavailable'],[])
        for key in ['visibleViewportVerified','rootCauseConfirmed','repairAuthorized','restorable',
                    'completeEnumeration','atomicSnapshot','gameCalls','gameWrites']:
            self.assertIs(result[key],False)
        text=json.dumps(result)
        self.assertNotIn(str(0x12345678),text);self.assertNotIn(str(0x23456789),text)

    def test_retained_bad_widgets_cannot_be_promoted_to_current_faults(self):
        old=copy.deepcopy(self.row)
        old['entryBinding']['turnBinding']['turnMatchesCurrentTimeline']=False
        old['entryBinding']['cacheMatchesCurrentRegistry']=False
        old['slateAnchor']['matchesWidget']=False;old['storedContentAnchor']=[0.5,0.0]
        self.widgets['attachments'].append(old)
        result=analyze_timeline_widgets(self.links,self.widgets)
        self.assertEqual(result['currentTurnCandidates'],1);self.assertEqual(result['retainedOtherOrUnknown'],1)
        self.assertEqual(result['widgetFindings'][1]['membership'],'outside-current-turns')
        self.assertEqual(result['widgetFindings'][1]['findings'],[])

    def test_cache_binding_and_anchor_disagreements_remain_distinct(self):
        self.row['entryBinding']['cacheMatchesCurrentRegistry']=False
        self.row['entryBinding']['parameterViewModelMatchesWidget']=False
        self.row['storedContentAnchor']=[0.5,0.0];self.row['slateAnchor']['matchesWidget']=False
        findings=analyze_timeline_widgets(self.links,self.widgets)['widgetFindings'][0]['findings']
        self.assertEqual(findings,['entry-cache-disagrees-with-current-registry',
            'portrait-source-disagrees-with-entry-cache',
            'widget-anchor-disagrees-with-raw-relation-mapping',
            'slate-and-widget-stored-anchors-disagree'])

    def test_raw_party_relation_is_reported_separately_from_consistent_anchor_storage(self):
        self.links['units'][0]['storedRelationByte']=2
        self.row['storedContentAnchor']=[0.5,0.0]
        findings=analyze_timeline_widgets(self.links,self.widgets)['widgetFindings'][0]['findings']
        self.assertEqual(findings,['party-raw-relation-is-not-friendlies'])

    def test_missing_projection_or_raw_coverage_never_means_matching(self):
        for turn,expected in [({},'projection-unavailable'),
                (dict(state='captured',turnMatchesCurrentTimeline=True),'entry-identity-unavailable'),
                (dict(state='captured',turnMatchesCurrentTimeline=True,identityMatchesCurrentTurn=False),
                 'entry-identity-mismatch')]:
            self.row['entryBinding']['turnBinding']=turn
            result=analyze_timeline_widgets(self.links,self.widgets)
            self.assertEqual(result['currentTurnCandidates'],0)
            self.assertEqual(result['widgetFindings'][0]['membership'],expected)
        self.assertFalse(analyze_timeline_widgets(None,None)['widgetEvidenceAvailable'])
        self.row['entryBinding']['turnBinding']=dict(state='captured',turnMatchesCurrentTimeline=True,
                                                   identityMatchesCurrentTurn=True)
        self.links['units'][0].pop('storedRelationByte');self.row['slateAnchor']=None
        result=analyze_timeline_widgets(self.links,self.widgets)
        self.assertEqual(result['widgetFindings'][0]['unavailable'],
                         ['raw-relation-to-widget-anchor','slate-anchor-comparison'])

    def test_unit_registry_faults_remain_available_without_widget_capture(self):
        self.links['unitViewModels'][0].update(cachedParameterState='outside-current',
            currentUnitIndex=None,sourceParameterMatchesCached=False)
        result=analyze_timeline_widgets(self.links,None)
        self.assertEqual(result['unitLinkFindings'][0]['findings'],
            ['registry-vm-parameter-not-current','inherited-and-cached-parameter-disagree'])
        self.assertFalse(result['widgetEvidenceAvailable'])

    def test_bounds_and_untrusted_restore_claims_are_refused(self):
        for edit in [lambda:self.links.update(gameWrites=True),
                     lambda:self.links['units'][0].update(index=True),
                     lambda:self.links['units'].append(copy.deepcopy(self.links['units'][0])),
                     lambda:self.widgets.update(attachments=[self.row]*257),
                     lambda:self.row.update(storedContentAnchor=[float('nan'),0.0])]:
            with self.subTest(edit=edit):
                self.setUp();edit()
                with self.assertRaises(TimelineAnalysisError):analyze_timeline_widgets(self.links,self.widgets)
