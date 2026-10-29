"""Preview clock normalization from a verified opening; no game connection or writes."""
import argparse
from pathlib import Path
import sys

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from ffr_helper.opening import load_opening
from ffr_helper.playtime_plan import export_playtime_plan, plan_opening_playtime


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('opening', type=Path)
    parser.add_argument('--output', type=Path, required=True, help='Receipt ending in .ffrplaytime.json')
    args = parser.parse_args()
    try:
        plan = plan_opening_playtime(load_opening(args.opening))
        export_playtime_plan(args.output, plan)
    except (ValueError, OSError) as error:
        raise SystemExit('Playtime preview unavailable: '+str(error)) from None
    print('Offline preview written. Original opening preserved; no game clock or save slot was changed.')


if __name__ == '__main__':
    main()
