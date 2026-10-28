"""Cumulative EXP/MRP arithmetic, using native per-level costs."""
from bisect import bisect_right
from importlib.resources import files
import json

INT_MAX = 2**31 - 1


def reference_data():
    return json.loads(files('ffr_helper').joinpath('data/reference.json').read_text(encoding='utf8'))


def checked_total(value):
    if isinstance(value, bool) or not isinstance(value, int) or not 0 <= value <= INT_MAX:
        raise ValueError('Total must be an integer between 0 and 2,147,483,647.')
    return value


def level_at(total: int, curve: list[dict]):
    checked_total(total)
    if not curve:
        raise ValueError('No progression curve is available.')
    ordered = sorted(curve, key=lambda row: row['level'])
    index = max(0, bisect_right([r['total'] for r in ordered], total) - 1)
    current = ordered[index]
    next_row = ordered[index + 1] if index + 1 < len(ordered) else None
    return dict(level=current['level'], total=total, start=current['total'],
                nextTotal=next_row['total'] if next_row else None,
                remaining=next_row['total'] - total if next_row else 0,
                stats=current.get('stats'))


def route_totals(start_exp: int, start_mrp: int, rows: list[dict]):
    exp, mrp = checked_total(start_exp), checked_total(start_mrp)
    for row in rows:
        count = checked_total(row['count'])
        exp += count * checked_total(row['exp'])
        mrp += count * checked_total(row['mrp'])
        checked_total(exp)
        checked_total(mrp)
    return exp, mrp
