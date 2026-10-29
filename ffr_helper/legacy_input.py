"""Calibrated read-only legacy WinMM input for the requested practice chord."""
from __future__ import annotations

from dataclasses import dataclass
import hashlib
import json
import os
from pathlib import Path
import re


@dataclass(frozen=True)
class LegacyPad:
    index: int
    capability_sha256: str
    buttons: int
    pov: int


@dataclass(frozen=True)
class LegacyBinding:
    index: int
    capability_sha256: str
    buttons: int
    pov: int = 18000

    def __post_init__(self):
        if (type(self.index) is not int or not 0 <= self.index < 16
                or type(self.buttons) is not int or not 0 < self.buttons <= 0xffffffff
                or self.buttons.bit_count() != 2 or type(self.pov) is not int or self.pov != 18000
                or not isinstance(self.capability_sha256, str)
                or re.fullmatch('[0-9a-f]{64}', self.capability_sha256) is None):
            raise ValueError('Invalid legacy-controller binding.')

    @classmethod
    def from_dict(cls, data):
        if (not isinstance(data, dict) or set(data) != {
                'format', 'version', 'index', 'capabilitySha256', 'buttons', 'pov', 'label'}
                or data.get('format') != 'ffr-helper-legacy-shortcut'
                or type(data.get('version')) is not int or data['version'] != 1
                or type(data.get('index')) is not int or not 0 <= data['index'] < 16
                or type(data.get('buttons')) is not int or not 0 < data['buttons'] <= 0xffffffff
                or data['buttons'].bit_count() != 2
                or type(data.get('pov')) is not int or data['pov'] != 18000
                or not isinstance(data.get('capabilitySha256'), str)
                or re.fullmatch('[0-9a-f]{64}', data['capabilitySha256']) is None
                or data.get('label') != 'R1 + Square + D-pad Down'):
            raise ValueError('Unsupported calibrated legacy-controller binding.')
        return cls(data['index'], data['capabilitySha256'], data['buttons'], data['pov'])

    @classmethod
    def load(cls, path: Path):
        with Path(path).open('rb') as handle:
            raw = handle.read(4097)
        if len(raw) > 4096:
            raise ValueError('Legacy-controller binding is too large.')
        return cls.from_dict(json.loads(raw))

    def held(self, pad: LegacyPad):
        return (pad.index == self.index and pad.capability_sha256 == self.capability_sha256
                and pad.buttons == self.buttons and pad.pov == self.pov)


class WindowsLegacyJoystick:
    """Only joyGetPosEx/joyGetDevCapsW; no capture, remapping or injected input.

    ABI follows the installed Windows SDK joystickapi.h. Only buttons and POV
    are requested; axis positions and unrelated keystrokes are not collected.
    A binding must come from deliberate calibration, never guessed button IDs.
    """

    def __init__(self, kernel=None):
        if os.name != 'nt':
            raise RuntimeError('Legacy joystick reading requires Windows.')
        import ctypes
        from ctypes import wintypes as w
        self.c = ctypes
        kernel = kernel or ctypes.WinDLL('kernel32', use_last_error=True)
        kernel.GetSystemDirectoryW.argtypes = [w.LPWSTR, w.UINT]
        kernel.GetSystemDirectoryW.restype = w.UINT
        system = ctypes.create_unicode_buffer(32768)
        size = kernel.GetSystemDirectoryW(system, len(system))
        if not 0 < size < len(system):
            raise RuntimeError('Cannot resolve the Windows system directory.')
        self.library = ctypes.WinDLL(str(Path(system.value) / 'winmm.dll'))

        class Position(ctypes.Structure):
            _fields_ = [(name, w.DWORD) for name in (
                'size', 'flags', 'x', 'y', 'z', 'r', 'u', 'v', 'buttons',
                'button_number', 'pov', 'reserved1', 'reserved2')]

        class Capabilities(ctypes.Structure):
            _fields_ = [('manufacturer', w.WORD), ('product', w.WORD), ('name', w.WCHAR * 32),
                        ('limits', w.UINT * 19), ('registry_key', w.WCHAR * 32), ('oem', w.WCHAR * 260)]

        if ctypes.sizeof(Position) != 52 or ctypes.sizeof(Capabilities) != 728:
            raise RuntimeError('Unsupported legacy joystick structure layout.')
        self.position_type, self.capabilities_type = Position, Capabilities
        self.library.joyGetPosEx.argtypes = [w.UINT, ctypes.POINTER(Position)]
        self.library.joyGetPosEx.restype = w.UINT
        self.library.joyGetDevCapsW.argtypes = [ctypes.c_size_t, ctypes.POINTER(Capabilities), w.UINT]
        self.library.joyGetDevCapsW.restype = w.UINT

    def read(self, index: int):
        if type(index) is not int or not 0 <= index < 16:
            raise ValueError('Legacy controller index must be between 0 and 15.')
        state = self.position_type()
        state.size = self.c.sizeof(state)
        state.flags = 0x40 | 0x80  # JOY_RETURNPOV | JOY_RETURNBUTTONS
        if self.library.joyGetPosEx(index, self.c.byref(state)) != 0:
            return None
        caps = self.capabilities_type()
        if self.library.joyGetDevCapsW(index, self.c.byref(caps), self.c.sizeof(caps)) != 0:
            return None
        # JOYCAPSW's first six UINTs are axis minima/maxima; wNumButtons is
        # entry 6, wCaps entry 15. This version requires a POV-equipped pad.
        if not 2 <= caps.limits[6] <= 32 or not caps.limits[15] & 0x10:
            return None
        if state.buttons >> caps.limits[6] or not (state.pov == 65535 or 0 <= state.pov < 36000):
            return None
        return LegacyPad(index, hashlib.sha256(bytes(caps)).hexdigest(), state.buttons, state.pov)
