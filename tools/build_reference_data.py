"""Build the compact, spoiler-aware reference from the locally inspected game.

Run from this repository beside the existing guide folder. No saves are inputs.
"""
import json
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
GUIDE = ROOT.parent / 'guide'


def main():
    tables = json.loads((GUIDE / 'sources/tables.json').read_text(encoding='utf8'))
    battle = json.loads((GUIDE / 'sources/battle-calculator-data.json').read_text(encoding='utf8'))
    catalog = json.loads((GUIDE / 'catalog.json').read_text(encoding='utf8'))
    chapters = {}
    for place in catalog['locations']:
        for item in place['chests'] + place['pickups']:
            item_id = str(item.get('item', -1))
            chapters[item_id] = min(chapters.get(item_id, 9), place['chapter'])
        for entry in place['enemies']:
            enemy = catalog['enemies'][str(entry['enemy'])]
            for key in ['dropNormal', 'dropRare', 'stealNormal', 'stealRare']:
                item_id = str(enemy[key])
                chapters[item_id] = min(chapters.get(item_id, 9), place['chapter'])
    characters = {}
    native_names = ['Rain', 'Lasswell', 'Fina', 'Lid', 'Nichol', 'Darkfina', 'Jake', 'Sakura']
    for character, native_name in zip(battle['characters'], native_names):
        rows = tables[f'Unit/LevelParameter/Playable/DT_UnitLevelParameter{native_name}']['rows']
        total, levels = 0, []
        for row, stats in zip(rows, character['levels']):
            total += row['RequiredExperience']
            levels.append(dict(level=row['Level'], cost=row['RequiredExperience'], total=total,
                               stats=stats, skills=[i for i in row['AddSkills'] if i > 0]))
        characters[str(character['id'])] = {k: character[k] for k in ['name', 'chapter', 'weaponType', 'armorTypes', 'sex', 'additionalForm']}
        characters[str(character['id'])]['levels'] = levels
    visions = {}
    for key, kind in [('Item/Vision/DT_VisionSynchroMasteryData', 'mrp'),
                      ('Item/Vision/DT_VisionAwakeningMasteryData', 'awakening')]:
        for row in tables[key]['rows']:
            item_id = str(row['id'])
            vision = visions.setdefault(item_id, dict(name=catalog['items'].get(item_id, {}).get('name', 'Vision ' + item_id)))
            vision.setdefault(kind, []).append(dict(level=row['level'], cost=row['masteryPoint']))
    for vision in visions.values():
        for kind in ['mrp', 'awakening']:
            total = 0
            for row in sorted(vision.get(kind, []), key=lambda r: r['level']):
                total += row['cost']
                row['total'] = total
    items = {}
    for item_id, item in catalog['items'].items():
        gear = battle['equipment'].get(item_id)
        chapter = gear['chapter'] if gear else chapters.get(item_id, 9)
        items[item_id] = dict(name=item['name'], chapter=chapter, gear=gear)
    result = dict(build=24250053, appId=3259780, characters=characters, items=items, visions=visions)
    out = ROOT / 'ffr_helper/data/reference.json'
    out.parent.mkdir(parents=True, exist_ok=True)
    out.write_text(json.dumps(result, ensure_ascii=False, separators=(',', ':')), encoding='utf8')
    print('Reference built:', len(items), 'items;', len(visions), 'visions;', out.stat().st_size, 'bytes')


if __name__ == '__main__':
    main()
