"""Windows reader sharing must not turn an atomic update into a partial file."""
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch

from ffr_helper.storage import atomic_write, game_is_running


def sharing_error():
    error = PermissionError(13, 'Simulated Windows sharing violation')
    error.winerror = 32
    return error


class AtomicWriteTests(unittest.TestCase):
    def test_restricted_host_never_treats_filtered_processes_as_a_closed_game(self):
        with patch('ffr_helper.storage.sys.platform', 'win32'), patch('ffr_helper.storage._windows_is_app_container', return_value=True):
            with self.assertRaisesRegex(RuntimeError, 'restricted host cannot verify'):
                game_is_running()

    def test_temporary_reader_lock_retries_without_truncating_the_old_file(self):
        with tempfile.TemporaryDirectory() as folder:
            path = Path(folder)/'observe.txt'
            path.write_bytes(b'old complete lease')
            import os
            replace = os.replace
            attempts = []
            def locked(source, destination):
                attempts.append(source)
                self.assertEqual(path.read_bytes(), b'old complete lease')
                if len(attempts) < 3:
                    raise sharing_error()
                replace(source, destination)
            with patch('ffr_helper.storage.os.replace', side_effect=locked), patch('ffr_helper.storage.time.sleep') as sleep:
                atomic_write(path, b'new complete lease')
            self.assertEqual(path.read_bytes(), b'new complete lease')
            self.assertEqual(len(set(attempts)), 1)
            self.assertEqual(sleep.call_count, 2)
            self.assertEqual(list(Path(folder).iterdir()), [path])

    def test_persistent_lock_fails_without_damaging_the_destination(self):
        with tempfile.TemporaryDirectory() as folder:
            path = Path(folder)/'observe.txt'
            path.write_bytes(b'original')
            with patch('ffr_helper.storage.os.replace', side_effect=sharing_error()) as replace, patch('ffr_helper.storage.time.sleep'):
                with self.assertRaises(PermissionError): atomic_write(path, b'new')
            self.assertEqual(replace.call_count, 6)
            self.assertEqual(path.read_bytes(), b'original')
            self.assertEqual(list(Path(folder).iterdir()), [path])


if __name__ == '__main__':
    unittest.main()
