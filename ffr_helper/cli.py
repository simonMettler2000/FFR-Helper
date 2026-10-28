"""Command-line companion; mutations default to a new output file."""
import argparse
import json
from pathlib import Path

from .editor import SaveEditor
from .progression import reference_data, level_at
from .storage import CheckpointStore, write_save


def main():
    parser = argparse.ArgumentParser(prog='ffr-helper-cli')
    parser.add_argument('save', type=Path)
    parser.add_argument('--exp', nargs=2, action='append', type=int, metavar=('CHARACTER_ID', 'TOTAL'))
    parser.add_argument('--mrp', nargs=3, action='append', type=int, metavar=('VISION_ID', 'CHARACTER_ID', 'TOTAL'))
    parser.add_argument('--item', nargs=2, action='append', type=int, metavar=('ITEM_ID', 'QUANTITY'))
    parser.add_argument('--output', type=Path)
    args = parser.parse_args()
    try:
        editor = SaveEditor.open(args.save.read_bytes())
        reference = reference_data()
        if (args.exp or args.mrp or args.item) and not args.output:
            parser.error('Edits require --output. The source file is never overwritten implicitly.')
        for uid, total in args.exp or []:
            editor.set_experience(uid, total)
        for vision, uid, total in args.mrp or []:
            editor.set_mrp(vision, uid, total)
        for item, count in args.item or []:
            if str(item) not in reference['items']:
                raise ValueError('Item ID is not present in the supported game data.')
            editor.set_item(item, count)
        if args.output:
            write_save(args.output, editor.export(), CheckpointStore(),
                       live_slot=args.output.resolve().parent == args.save.resolve().parent)
        print(json.dumps(dict(characters={str(uid): dict(exp=row['Experience'].integer(),
                            level=level_at(row['Experience'].integer(), reference['characters'][str(uid)]['levels'])['level'])
                            for uid, row in editor.units().items() if str(uid) in reference['characters']},
                            inventoryRecords=len(editor.inventory()),
                            visionRecords=len(editor.root['visionList'].structs()),
                            output=str(args.output) if args.output else None), indent=2))
    except (ValueError, OSError, RuntimeError) as error:
        parser.exit(1, str(error) + '\n')


if __name__ == '__main__':
    main()
