"""Inspect verified opening coverage offline without exposing encounter/save values."""
import argparse
from pathlib import Path
import sys
import zipfile

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from ffr_helper.opening import load_opening, summarize_opening


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('opening', type=Path)
    args = parser.parse_args()
    try:
        summary = summarize_opening(load_opening(args.opening))
    except (ValueError, OSError, KeyError, zipfile.BadZipFile) as error:
        raise SystemExit('Opening inspection unavailable: '+str(error)) from None
    print(summary)


if __name__ == '__main__':
    main()
