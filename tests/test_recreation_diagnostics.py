import copy
import unittest

from ffr_helper.recreation_diagnostics import require_recreated_party_links, RecreationDiagnosticError


class RecreationDiagnosticTests(unittest.TestCase):
    def setUp(self):
        self.party=[10,20]
        self.observation=dict(units=[dict(index=i,raw=dict(unitId=identity))
                                    for i,identity in enumerate([10,20,30])],
            timelineUnitLinks=dict(unitsWithoutCurrentViewModel=[],parameterRelationBytes=[1,1,2],
                links=[dict(sourceUnit=i,cachedUnit=i,sourceMatchesCached=True,
                            cachedUnitId=identity,cachedParameterState='current')
                       for i,identity in enumerate([10,20,30])]))

    def test_every_party_member_with_current_source_and_cache_is_required(self):
        require_recreated_party_links(self.party,self.observation)
        original=copy.deepcopy(self.observation)
        self.assertIsNone(require_recreated_party_links(self.party,self.observation))
        self.assertEqual(original,self.observation)

    def test_some_party_members_is_insufficient_even_with_no_reported_missing_vm(self):
        self.observation['units'][1]['raw']['unitId']=40
        self.observation['timelineUnitLinks']['links'][1]['cachedUnitId']=40
        with self.assertRaisesRegex(RecreationDiagnosticError,'exactly once'):
            require_recreated_party_links(self.party,self.observation)

    def test_duplicate_party_id_is_ambiguous_instead_of_selecting_one_row(self):
        self.observation['units'][2]['raw']['unitId']=10
        with self.assertRaisesRegex(RecreationDiagnosticError,'exactly once'):
            require_recreated_party_links(self.party,self.observation)

    def test_incomplete_registry_cannot_be_hidden_by_an_empty_missing_list(self):
        self.observation['timelineUnitLinks']['links'].pop(0)
        with self.assertRaisesRegex(RecreationDiagnosticError,'independently represent every'):
            require_recreated_party_links(self.party,self.observation)

    def test_stale_inherited_party_source_is_refused_even_with_another_good_link(self):
        self.observation['timelineUnitLinks']['links'].append(dict(sourceUnit=0,cachedUnit=-1,
            sourceMatchesCached=False,cachedUnitId=99,cachedParameterState='outside-current'))
        with self.assertRaisesRegex(RecreationDiagnosticError,'stale or mismatched'):
            require_recreated_party_links(self.party,self.observation)

    def test_retired_cache_with_reused_party_id_is_refused_even_without_current_source(self):
        self.observation['timelineUnitLinks']['links'].append(dict(sourceUnit=-1,cachedUnit=-1,
            sourceMatchesCached=True,cachedUnitId=10,cachedParameterState='outside-current'))
        with self.assertRaisesRegex(RecreationDiagnosticError,'stale or mismatched'):
            require_recreated_party_links(self.party,self.observation)

    def test_cross_party_source_cache_mismatch_and_reported_missing_member_are_refused(self):
        self.observation['timelineUnitLinks']['links'][0].update(sourceUnit=1,sourceMatchesCached=False)
        with self.assertRaises(RecreationDiagnosticError):require_recreated_party_links(self.party,self.observation)
        self.setUp();self.observation['timelineUnitLinks']['unitsWithoutCurrentViewModel']=[1]
        with self.assertRaises(RecreationDiagnosticError):require_recreated_party_links(self.party,self.observation)

    def test_legacy_absence_differs_from_unavailable_partial_or_unexpected_relation_bytes(self):
        timeline=self.observation['timelineUnitLinks'];timeline.pop('parameterRelationBytes')
        require_recreated_party_links(self.party,self.observation)
        for raw in [None,[],[1,1],[1,0,2],[1,True,2],[1,1,256]]:
            with self.subTest(raw=raw),self.assertRaises(RecreationDiagnosticError):
                timeline['parameterRelationBytes']=raw
                require_recreated_party_links(self.party,self.observation)

    def test_unknown_enemy_links_do_not_claim_party_faults_or_rendering_verification(self):
        self.observation['timelineUnitLinks']['links'][2].update(sourceUnit=-1,cachedUnit=-1,
            cachedUnitId=None,cachedParameterState='null',sourceMatchesCached=True)
        self.observation['timelineUnitLinks']['unitsWithoutCurrentViewModel']=[2]
        require_recreated_party_links(self.party,self.observation)

    def test_typed_and_bounded_party_and_unit_evidence_is_required(self):
        for party in [[],[10,10],[True,20],[10,-1],[10,2**31],list(range(1,18))]:
            with self.subTest(party=party),self.assertRaises(RecreationDiagnosticError):
                require_recreated_party_links(party,self.observation)
        for mutate in [lambda:self.observation['units'][0].update(index=True),
                       lambda:self.observation.update(timelineUnitLinks=None),
                       lambda:self.observation['timelineUnitLinks'].update(links=[]),
                       lambda:self.observation['timelineUnitLinks'].update(unitsWithoutCurrentViewModel=[True])]:
            self.setUp();mutate()
            with self.assertRaises(RecreationDiagnosticError):require_recreated_party_links(self.party,self.observation)
