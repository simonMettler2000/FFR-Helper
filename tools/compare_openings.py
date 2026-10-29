"""Compare retained encounter/progression components without operating FFR."""
import argparse
from pathlib import Path
import sys
sys.path.insert(0,str(Path(__file__).resolve().parents[1]))
from ffr_helper.opening import load_opening
from ffr_helper.opening_comparison import compare_openings,export_opening_comparison


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('source',type=Path);parser.add_argument('observed',type=Path)
    parser.add_argument('--output',type=Path)
    args=parser.parse_args();source=load_opening(args.source);observed=load_opening(args.observed)
    result=(export_opening_comparison(args.output,source,observed) if args.output else compare_openings(source,observed))
    print(f"Encounter fields equal={result['encounterEntryEqual']}; changed fields={len(result['changedEntryFields'])}; raw progression equal={result['rawProgressionEqual']}.")
    normalized=result['normalizedProgressionEqual']
    print('Normalized progression comparison='+('unavailable: source clock coverage unsupported' if normalized is None else str(normalized))+'.')
    print('Component equality cannot verify a restart, independent provenance or complete state restoration.')


if __name__=='__main__':main()
