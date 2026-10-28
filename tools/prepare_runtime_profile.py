"""Stage the exact-build symbol-lookup fix, without changing the game installation."""
import argparse
from pathlib import Path
import sys

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from ffr_helper.compatibility import prepare_signature_profile

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--exe', type=Path, required=True)
parser.add_argument('--output', type=Path, default=Path('local/runtime-profile'))
args = parser.parse_args()
print('Prepared compatibility lookup:', prepare_signature_profile(args.exe, args.output))
print('Not installed. This file only helps UE4SS locate the verified engine function.')
