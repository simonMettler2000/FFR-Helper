"""Read-only Windows practice shortcuts; no input injection or suppression."""
from __future__ import annotations

from dataclasses import dataclass
import os
from pathlib import Path

# Steam Input/XInput: PlayStation R1 / Square / D-pad Down map to RB / X / Down.
RETRY_BUTTONS = 0x0200 | 0x4000 | 0x0002
CONTROLLER_SHORTCUT = 'R1 + Square + D-pad Down (RB + X + D-pad Down)'
KEYBOARD_SHORTCUT = 'Ctrl + Shift + F8'


class ShortcutEdge:
    """A held shortcut never activates on startup, focus return or reconnect."""

    def __init__(self):
        self.released = False
        self.connected = None

    def update(self, focused: bool, buttons: tuple[int | None, ...], keyboard: bool = False):
        if not focused:
            self.released = False
            self.connected = None
            return None
        connected = tuple(index for index, mask in enumerate(buttons) if mask is not None)
        if connected != self.connected:
            self.connected = connected
            self.released = False
        controller = any(mask is not None and mask & RETRY_BUTTONS == RETRY_BUTTONS for mask in buttons)
        down = controller or keyboard
        if not down:
            self.released = True
            return None
        if not self.released:
            return None
        self.released = False
        return 'controller' if controller else 'keyboard'


@dataclass
class ManualIntent:
    """Short-lived intent belongs to one current source; it never queues a retry."""

    source: tuple[int, str] | None = None
    until: float = 0
    shortcut: str | None = None

    def press(self, shortcut, source, ready, now):
        self.clear()
        if shortcut and ready:
            self.source, self.until, self.shortcut = source, now + 2, shortcut

    def matches(self, source, ready, focused, now):
        if not ready or not focused or now > self.until or source != self.source:
            self.clear()
            return False
        return self.source is not None

    def clear(self):
        self.source, self.until, self.shortcut = None, 0, None


class WindowsPracticeShortcut:
    """Poll only the two configured chords, with an exact foreground exe gate.

    XInput is loaded only from the Windows system directory. DirectInput-only
    controllers need Steam Input or the keyboard shortcut. No hooks, remapping,
    game calls, input synthesis or gameplay-memory reads/writes are performed.
    """

    def __init__(self, executable: Path):
        if os.name != 'nt':
            raise RuntimeError('This experimental live shortcut requires Windows.')
        import ctypes
        from ctypes import wintypes
        self.c = ctypes
        self.expected = os.path.normcase(os.path.abspath(executable))
        self.edge = ShortcutEdge()
        self.focused = False
        self.user = ctypes.WinDLL('user32', use_last_error=True)
        self.kernel = ctypes.WinDLL('kernel32', use_last_error=True)
        self.user.GetForegroundWindow.restype = wintypes.HWND
        self.user.GetWindowThreadProcessId.argtypes = [wintypes.HWND, ctypes.POINTER(wintypes.DWORD)]
        self.user.GetWindowThreadProcessId.restype = wintypes.DWORD
        self.user.GetAsyncKeyState.argtypes = [ctypes.c_int]
        self.user.GetAsyncKeyState.restype = ctypes.c_short
        self.kernel.OpenProcess.argtypes = [wintypes.DWORD, wintypes.BOOL, wintypes.DWORD]
        self.kernel.OpenProcess.restype = wintypes.HANDLE
        self.kernel.CloseHandle.argtypes = [wintypes.HANDLE]
        self.kernel.QueryFullProcessImageNameW.argtypes = [wintypes.HANDLE, wintypes.DWORD, wintypes.LPWSTR, ctypes.POINTER(wintypes.DWORD)]
        self.kernel.QueryFullProcessImageNameW.restype = wintypes.BOOL
        self.kernel.GetSystemDirectoryW.argtypes = [wintypes.LPWSTR, wintypes.UINT]
        self.kernel.GetSystemDirectoryW.restype = wintypes.UINT
        system_dir = ctypes.create_unicode_buffer(32768)
        size = self.kernel.GetSystemDirectoryW(system_dir, len(system_dir))
        if not 0 < size < len(system_dir):
            raise RuntimeError('Cannot resolve the Windows system directory.')

        class Gamepad(ctypes.Structure):
            _fields_ = [('buttons', wintypes.WORD), ('left_trigger', ctypes.c_ubyte),
                        ('right_trigger', ctypes.c_ubyte), ('left_x', ctypes.c_short),
                        ('left_y', ctypes.c_short), ('right_x', ctypes.c_short), ('right_y', ctypes.c_short)]

        class State(ctypes.Structure):
            _fields_ = [('packet', wintypes.DWORD), ('gamepad', Gamepad)]

        if ctypes.sizeof(Gamepad) != 12 or ctypes.sizeof(State) != 16:
            raise RuntimeError('Unsupported XInput structure layout.')
        self.state_type = State
        self.xinput = None
        for name in ('xinput1_4.dll', 'xinput1_3.dll', 'xinput9_1_0.dll'):
            try:
                library = ctypes.WinDLL(str(Path(system_dir.value) / name))
                library.XInputGetState.argtypes = [wintypes.DWORD, ctypes.POINTER(State)]
                library.XInputGetState.restype = wintypes.DWORD
                self.xinput = library
                break
            except (OSError, AttributeError):
                continue

    def _foreground_matches(self):
        from ctypes import wintypes
        window = self.user.GetForegroundWindow()
        if not window:
            return False
        pid = wintypes.DWORD()
        self.user.GetWindowThreadProcessId(window, self.c.byref(pid))
        handle = self.kernel.OpenProcess(0x1000, False, pid.value)
        if not handle:
            return False
        try:
            path = self.c.create_unicode_buffer(32768)
            length = wintypes.DWORD(len(path))
            return (bool(self.kernel.QueryFullProcessImageNameW(handle, 0, path, self.c.byref(length)))
                    and os.path.normcase(os.path.abspath(path.value)) == self.expected)
        finally:
            self.kernel.CloseHandle(handle)

    def poll(self):
        self.focused = self._foreground_matches()
        if not self.focused:
            return self.edge.update(False, ())
        buttons = [None] * 4
        if self.xinput is not None:
            for index in range(4):
                state = self.state_type()
                if self.xinput.XInputGetState(index, self.c.byref(state)) == 0:
                    buttons[index] = state.gamepad.buttons
        keyboard = all(self.user.GetAsyncKeyState(key) & 0x8000 for key in (0x11, 0x10, 0x77))
        return self.edge.update(True, tuple(buttons), keyboard)
