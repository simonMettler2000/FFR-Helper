import unittest

from ffr_helper.practice_input import ShortcutEdge, ManualIntent, RETRY_BUTTONS


class PracticeInputTests(unittest.TestCase):
    def test_controller_chord_needs_release_and_is_one_edge_across_focus_changes(self):
        edge = ShortcutEdge()
        self.assertIsNone(edge.update(True, (RETRY_BUTTONS,)))
        self.assertIsNone(edge.update(True, (RETRY_BUTTONS,)))
        self.assertIsNone(edge.update(True, (0x0200 | 0x4000,)))
        self.assertEqual(edge.update(True, (RETRY_BUTTONS,)), 'controller')
        self.assertIsNone(edge.update(True, (RETRY_BUTTONS,)))
        self.assertIsNone(edge.update(False, (0,)))
        self.assertIsNone(edge.update(True, (RETRY_BUTTONS,)))
        self.assertIsNone(edge.update(True, (0,)))
        self.assertEqual(edge.update(True, (RETRY_BUTTONS,)), 'controller')

    def test_observed_disconnect_reconnect_does_not_activate_held_chord(self):
        edge = ShortcutEdge()
        self.assertIsNone(edge.update(True, (0, None)))
        self.assertIsNone(edge.update(True, (None, None)))
        self.assertIsNone(edge.update(True, (None, RETRY_BUTTONS)))
        self.assertIsNone(edge.update(True, (None, 0)))
        self.assertEqual(edge.update(True, (None, RETRY_BUTTONS)), 'controller')
        self.assertIsNone(edge.update(True, (None, RETRY_BUTTONS)))

    def test_keyboard_and_partial_controller_chords_do_not_generate_extra_edges(self):
        edge = ShortcutEdge()
        self.assertIsNone(edge.update(True, (), True))
        self.assertIsNone(edge.update(True, (), False))
        self.assertEqual(edge.update(True, (), True), 'keyboard')
        self.assertIsNone(edge.update(True, (), True))
        self.assertIsNone(edge.update(True, (0x0200,)))
        self.assertIsNone(edge.update(True, (0x4002,)))
        self.assertEqual(edge.update(True, (RETRY_BUTTONS,)), 'controller')

    def test_manual_intent_is_bound_expires_and_is_not_queued_from_unsafe_press(self):
        intent = ManualIntent(); source = (1, 'a' * 64)
        intent.press('controller', source, False, 10)
        self.assertFalse(intent.matches(source, True, True, 10.1))
        for changed, ready, focused, now in [((2, 'a' * 64), True, True, 10.5),
                ((1, 'b' * 64), True, True, 10.5), (source, False, True, 10.5),
                (source, True, False, 10.5), (source, True, True, 12.01)]:
            intent.press('controller', source, True, 10)
            self.assertTrue(intent.matches(source, True, True, 10.1))
            self.assertFalse(intent.matches(changed, ready, focused, now))
            self.assertFalse(intent.matches(source, True, True, 10.2))
        intent.press('keyboard', source, True, 10)
        self.assertTrue(intent.matches(source, True, True, 11))
        intent.clear()
        self.assertFalse(intent.matches(source, True, True, 11))
