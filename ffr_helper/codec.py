"""Lossless, bounded reader/editor for FFR's UE 5.6 tagged saves.

Only requested property values are rebuilt. Unknown properties, custom-version
records, native struct bytes and property flags survive byte-for-byte.
"""
from __future__ import annotations

from dataclasses import dataclass, field
import hashlib
import hmac
import struct

from cryptography.hazmat.primitives.ciphers import Cipher, algorithms, modes

# Game-format constant from DefaultSaveGameUtility.ini, not an account credential.
FORMAT_KEY = b"wwrqPkdwzPvgfchQ8kTYHVbn4umTf3d7"
MAX_FILE = 64 * 1024 * 1024
NONE = b"\x05\x00\x00\x00None\0"


class SaveFormatError(ValueError):
    pass


def i32(value: int) -> bytes:
    if isinstance(value, bool) or not isinstance(value, int) or not -2**31 <= value < 2**31:
        raise ValueError("Value must fit a signed 32-bit integer.")
    return struct.pack("<i", value)


def fstring(value: str) -> bytes:
    if value.isascii():
        raw = value.encode() + b"\0"
        return i32(len(raw)) + raw
    raw = value.encode("utf-16-le") + b"\0\0"
    return i32(-len(raw) // 2) + raw


class Reader:
    def __init__(self, data: bytes, position: int = 0):
        self.data, self.pos = data, position

    def take(self, count: int) -> bytes:
        if count < 0 or self.pos + count > len(self.data):
            raise SaveFormatError(f"Truncated save data at byte {self.pos}.")
        start = self.pos
        self.pos += count
        return self.data[start:self.pos]

    def number(self, fmt="<i"):
        return struct.unpack(fmt, self.take(struct.calcsize(fmt)))[0]

    def string(self) -> str:
        size = self.number()
        if not -1_000_000 <= size <= 1_000_000:
            raise SaveFormatError("Invalid Unreal string length.")
        if not size:
            return ""
        raw = self.take(abs(size) * (2 if size < 0 else 1))
        ending = b"\0\0" if size < 0 else b"\0"
        if not raw.endswith(ending):
            raise SaveFormatError("Unterminated Unreal string.")
        try:
            return raw[:-len(ending)].decode("utf-16-le" if size < 0 else "utf8")
        except UnicodeError as error:
            raise SaveFormatError("Invalid Unreal string encoding.") from error


@dataclass(frozen=True)
class TypeNode:
    name: str
    children: tuple[TypeNode, ...] = ()

    @classmethod
    def read(cls, reader: Reader, depth=0):
        if depth > 12:
            raise SaveFormatError("Property type is nested too deeply.")
        name, count = reader.string(), reader.number()
        if not 0 <= count <= 8:
            raise SaveFormatError("Unsupported property type descriptor.")
        return cls(name, tuple(cls.read(reader, depth + 1) for _ in range(count)))

    def encode(self):
        return fstring(self.name) + i32(len(self.children)) + b"".join(c.encode() for c in self.children)


@dataclass
class Property:
    name: str
    kind: TypeNode
    header: bytes
    size_offset: int
    raw: bytes
    flags: int = 0
    _struct: PropertyList | None = field(default=None, repr=False)
    _array: list[PropertyList] | None = field(default=None, repr=False)

    def encode(self) -> bytes:
        value = self.raw
        if self._struct is not None:
            value = self._struct.encode()
        if self._array is not None:
            value = i32(len(self._array)) + b"".join(r.encode() for r in self._array)
        header = bytearray(self.header)
        struct.pack_into("<I", header, self.size_offset, len(value))
        return bytes(header) + value

    def integer(self) -> int:
        if self.kind.name != "IntProperty" or len(self.raw) != 4:
            raise SaveFormatError(f"{self.name} is not an IntProperty.")
        return struct.unpack("<i", self.raw)[0]

    def set_integer(self, value: int):
        self.integer()
        self.raw = i32(value)

    def struct(self) -> PropertyList:
        if self.kind.name != "StructProperty" or self.flags & 8:
            raise SaveFormatError(f"{self.name} is not a tagged struct.")
        if self._struct is None:
            reader = Reader(self.raw)
            self._struct = PropertyList.read(reader)
            if reader.pos != len(self.raw):
                raise SaveFormatError(f"Unexpected trailing struct data in {self.name}.")
        return self._struct

    def structs(self) -> list[PropertyList]:
        if self.kind.name != "ArrayProperty" or self.kind.children[0].name != "StructProperty":
            raise SaveFormatError(f"{self.name} is not a struct array.")
        if self._array is None:
            reader = Reader(self.raw)
            count = reader.number()
            if not 0 <= count <= 100_000:
                raise SaveFormatError("Invalid struct-array length.")
            self._array = [PropertyList.read(reader) for _ in range(count)]
            if reader.pos != len(self.raw):
                raise SaveFormatError(f"Unexpected trailing array data in {self.name}.")
        return self._array

    def integers(self) -> list[int]:
        if self.kind.name != "ArrayProperty" or self.kind.children[0].name != "IntProperty":
            raise SaveFormatError(f"{self.name} is not an integer array.")
        reader = Reader(self.raw)
        count = reader.number()
        if count < 0 or 4 + count * 4 != len(self.raw):
            raise SaveFormatError("Invalid integer array.")
        return [reader.number() for _ in range(count)]

    def set_integers(self, values: list[int]):
        self.integers()
        self.raw = i32(len(values)) + b"".join(i32(v) for v in values)

    def integer_map(self) -> dict[int, int]:
        if self.kind.name != "MapProperty" or [c.name for c in self.kind.children] != ["IntProperty", "IntProperty"]:
            raise SaveFormatError(f"{self.name} is not an integer map.")
        reader = Reader(self.raw)
        removed = reader.number()
        if not 0 <= removed <= 100_000:
            raise SaveFormatError("Invalid removed-key map.")
        reader.take(removed * 4)
        count = reader.number()
        if not 0 <= count <= 100_000 or reader.pos + count * 8 != len(self.raw):
            raise SaveFormatError("Invalid integer map.")
        result = {}
        for _ in range(count):
            key, value = reader.number(), reader.number()
            if key in result:
                raise SaveFormatError("Duplicate integer-map key.")
            result[key] = value
        return result

    def set_integer_map(self, values: dict[int, int]):
        self.integer_map()
        reader = Reader(self.raw)
        reader.take(reader.number() * 4)
        prefix = self.raw[:reader.pos]
        self.raw = prefix + i32(len(values)) + b"".join(i32(k) + i32(v) for k, v in values.items())


@dataclass
class PropertyList:
    properties: list[Property]
    terminator: bytes = NONE

    def __getitem__(self, name: str) -> Property:
        matches = [p for p in self.properties if p.name.casefold() == name.casefold()]
        if len(matches) != 1:
            raise SaveFormatError(f"Missing or ambiguous save property: {name}.")
        return matches[0]

    def encode(self):
        return b"".join(p.encode() for p in self.properties) + self.terminator

    @classmethod
    def read(cls, reader: Reader):
        properties = []
        for _ in range(100_000):
            start = reader.pos
            name = reader.string()
            if name == "None":
                return cls(properties, reader.data[start:reader.pos])
            kind = TypeNode.read(reader)
            size_offset = reader.pos - start
            size, flags = reader.number("<I"), reader.number("<B")
            if flags & ~0x1F or flags & 4:
                raise SaveFormatError("Unsupported property extension flags; this save was not modified.")
            if flags & 1:
                reader.take(4)
            if flags & 2:
                reader.take(16)
            header = reader.data[start:reader.pos]
            properties.append(Property(name, kind, header, size_offset, reader.take(size), flags))
        raise SaveFormatError("Too many save properties.")


@dataclass
class SaveDocument:
    prefix: bytes
    root: PropertyList
    suffix: bytes
    envelope: bytes | None
    original_plaintext: bytes

    @classmethod
    def decode(cls, data: bytes):
        if not 4 <= len(data) <= MAX_FILE:
            raise SaveFormatError("Save file has an invalid size.")
        envelope = None
        if data.startswith(b"SVHD"):
            if len(data) < 48 or (len(data) - 32) % 16 or data[4:8] != b"\x01\0\x01\0":
                raise SaveFormatError("Unsupported FFR save envelope version.")
            if not hmac.compare_digest(hashlib.sha1(data[32:]).digest(), data[8:28]):
                raise SaveFormatError("Save checksum does not match; refusing to open damaged data.")
            envelope = data[:32]
            decryptor = Cipher(algorithms.AES(FORMAT_KEY), modes.ECB()).decryptor()
            data = decryptor.update(data[32:]) + decryptor.finalize()
        reader = Reader(data)
        if reader.take(4) != b"GVAS" or reader.number() != 3:
            raise SaveFormatError("This is not a supported FFR GVAS save.")
        _, ue5 = reader.number(), reader.number()
        engine = [reader.number("<H") for _ in range(3)]
        reader.take(4)
        reader.string()
        custom_format, count = reader.number(), reader.number()
        if engine[:2] != [5, 6] or ue5 != 1017 or custom_format != 3 or not 0 <= count <= 10_000:
            raise SaveFormatError("Unsupported save engine/version. No data was changed.")
        reader.take(count * 20)
        save_class = reader.string()
        if save_class != "/Script/mucha.CPP_MuchaGameSaveData":
            raise SaveFormatError("Select a GameSaveData slot, rather than settings or input data.")
        if reader.number("<B") != 0:
            raise SaveFormatError("Unsupported UObject serialization flags.")
        prefix = data[:reader.pos]
        root = PropertyList.read(reader)
        suffix = data[reader.pos:]
        # SaveGameToMemory appends a zero u32 after the root None marker;
        # the FFR writer then adds 1–16 zero bytes for AES alignment.
        if envelope is not None and (not 5 <= len(suffix) <= 20 or any(suffix)):
            raise SaveFormatError("Unexpected data after the FFR save object.")
        result = cls(prefix, root, suffix, envelope, data)
        if result.plaintext() != data:
            raise SaveFormatError("Lossless read verification failed.")
        return result

    def plaintext(self):
        return self.prefix + self.root.encode() + self.suffix

    def encode(self):
        logical = self.prefix + self.root.encode()
        if self.envelope is None:
            return logical + self.suffix
        logical += bytes(4)
        padding = bytes(16 - len(logical) % 16)
        encryptor = Cipher(algorithms.AES(FORMAT_KEY), modes.ECB()).encryptor()
        encrypted = encryptor.update(logical + padding) + encryptor.finalize()
        envelope = self.envelope[:8] + hashlib.sha1(encrypted).digest() + self.envelope[28:32]
        return envelope + encrypted


def integer_property(name: str, value: int) -> Property:
    kind = TypeNode("IntProperty")
    beginning = fstring(name) + kind.encode()
    return Property(name, kind, beginning + i32(4) + b"\0", len(beginning), i32(value))
