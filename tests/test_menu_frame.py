import unittest
from ffr_helper.menu_frame import validate_menu_frame,menu_frame_is_recent


def sample():
    return dict(kind='read-only-menu-frame',readOnly=True,restorable=False,boundaryReviewed=False,atomicSnapshot=False,
        openingSequence=1,sampleTime=100,gameThread=True,matchingWorld=True,ordinaryEncounter=True,
        battleActive=True,commandMenu=True,fieldReady=False,units=2,waitingJobs=0,workingJobs=0,turnBeginnings=1)


class MenuFrameTests(unittest.TestCase):
    def test_turn_beginning_is_not_completed_command_count(self):
        data=sample();result=validate_menu_frame(data)
        self.assertEqual(result['turnBeginnings'],1);self.assertTrue(result['commandMenu'])
        self.assertFalse(result['boundaryReviewed']);self.assertIsNot(data,result)

    def test_worker_heartbeat_cannot_refresh_an_old_game_frame(self):
        data=sample()
        self.assertTrue(menu_frame_is_recent(data,102));self.assertFalse(menu_frame_is_recent(data,103))
        self.assertFalse(menu_frame_is_recent(data,97));self.assertFalse(menu_frame_is_recent(None,100))

    def test_bounds_flags_and_completeness_claims_are_rejected(self):
        for key,value in [('openingSequence',True),('sampleTime',False),('units',65),('waitingJobs',-1),
                          ('turnBeginnings',6400001),('boundaryReviewed',True),('restorable',True),
                          ('atomicSnapshot',True),('gameThread',False),('ordinaryEncounter',1)]:
            with self.subTest(key=key,value=value),self.assertRaises(ValueError):validate_menu_frame(dict(sample(),**{key:value}))
        with self.assertRaises(ValueError):validate_menu_frame(dict(sample(),safeToRetry=True))

    def test_inconsistent_menu_or_field_state_is_rejected(self):
        for changes in [dict(battleActive=False),dict(units=0),dict(turnBeginnings=-1),dict(fieldReady=True)]:
            with self.assertRaises(ValueError):validate_menu_frame(dict(sample(),**changes))
        field=dict(sample(),battleActive=False,commandMenu=False,fieldReady=True,units=0,turnBeginnings=-1)
        self.assertEqual(validate_menu_frame(field),field)
        self.assertEqual(validate_menu_frame(dict(field,managersReleased=True)),dict(field,managersReleased=True))
        for changes in [dict(managersReleased=1),dict(managersReleased=True),dict(managersReleased=True,fieldReady=False)]:
            with self.assertRaises(ValueError):validate_menu_frame(dict(sample(),**changes))
