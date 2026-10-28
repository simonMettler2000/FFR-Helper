"""Generate a read-only UE4SS signature override from an exact verified build."""
from dataclasses import dataclass
import hashlib
from pathlib import Path
import struct
from .game_build import GAME_SHA256


@dataclass(frozen=True)
class SignatureProfile:
    game_sha256: str
    rva: int
    code_sha256: str


FFR_STATIC_CONSTRUCT = SignatureProfile(
    GAME_SHA256,
    0x15bce60,
    '7ede0ed5808b58862ead7bcd3a2c2589d0c5a804acf8380d96317bc91edb4f27',
)


def signature_script(raw: bytes, profile=FFR_STATIC_CONSTRUCT):
    if hashlib.sha256(raw).hexdigest() != profile.game_sha256:
        raise ValueError('Unsupported game build; no compatibility files were written.')
    try:
        nt = struct.unpack_from('<I', raw, 0x3c)[0]
        if raw[:2] != b'MZ' or raw[nt:nt+4] != b'PE\0\0':
            raise ValueError('Invalid game executable.')
        machine, count = struct.unpack_from('<HH', raw, nt+4)
        size = struct.unpack_from('<H', raw, nt+20)[0]
        if machine != 0x8664 or struct.unpack_from('<H', raw, nt+24)[0] != 0x20b or not 1 <= count <= 128:
            raise ValueError('The compatibility profile requires a Windows x64 executable.')
        positions = []
        for index in range(count):
            entry = nt+24+size+40*index
            _, rva, length, position, _, _, _, _, flags = struct.unpack_from('<IIIIIIHHI', raw, entry+8)
            if rva <= profile.rva and profile.rva+64 <= rva+length and flags & 0x20000000:
                positions.append(position+profile.rva-rva)
        if len(positions) != 1:
            raise ValueError('The profiled function is outside a unique executable section.')
        code = raw[positions[0]:positions[0]+64]
    except struct.error as error:
        raise ValueError('Truncated executable metadata.') from error
    if (len(code) != 64 or hashlib.sha256(code).hexdigest() != profile.code_sha256
            or raw.count(code) != 1):
        raise ValueError('The profiled function fingerprint is missing or ambiguous.')
    # Extract bytes locally; no game instruction bytes are shipped in the repo.
    pattern = ' '.join(f'{byte:02X}' for byte in code)
    return ('-- FFR build 24250053 only. Symbol lookup; no memory writes or game calls.\n'
            '-- Generated after full executable and function fingerprint checks.\n'
            'function Register()\n    return "'+pattern+'"\nend\n\n'
            'function OnMatchFound(MatchAddress)\n    return MatchAddress\nend\n').encode('ascii')


def prepare_signature_profile(executable: Path, output: Path):
    raw = Path(executable).read_bytes()
    script = signature_script(raw)
    destination = Path(output)/'UE4SS_Signatures/StaticConstructObject.lua'
    if destination.exists():
        if destination.read_bytes() != script:
            raise ValueError('An existing signature differs; it was not replaced.')
        return destination
    destination.parent.mkdir(parents=True, exist_ok=True)
    with destination.open('xb') as handle:
        handle.write(script)
    return destination
