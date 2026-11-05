# Timeline portrait investigation

One supervised untouched-opening recreation produced a responsive new encounter,
but party portraits appeared below the timeline alongside enemies. A later natural
**normal** encounter showed party portraits above it. Combat impact is unknown;
this remains a blocker for declaring the recreation correct.

## Static finding, 5 November 2026

Read-only inspection of the installed build's cooked timeline assets recovered
the portrait widget's `SetRelation` Blueprint and its MVVM binding. The widget
sets `ContentAnchor` to `(0.5, double(InRelation == Friendlies))`. `Friendlies` is
relation value 1; `None` is 0 and `Enemies` is 2. Its source binding is
`CPP_VM_BtlUnitParam.GetRelation`.

The native getter reads the view model's `BtlUnitParam` reference. If null or
flagged invalid, it returns `None`; otherwise it dispatches the parameter's
relation virtual. This makes the unit/view-model link and binding notification
path relevant to the layout discrepancy. It **does not establish the cause**:
there is no retained recreated unit-VM/widget snapshot from the earlier trial.
Do not force the widget's anchor or infer faction from timeline entry bytes 8/9;
those bytes also carry timeline kind/current-next information.

## Prepared read-only diagnostic

`ffr_helper.timeline_diagnostics.snapshot_timeline_links` accepts a bounded reader
after the caller verifies the exact image/layout and current battle boundary. It
does not open a process, invoke a getter, write memory or inject input itself.
The private Windows collector can supply query/read-only process access.

The snapshot:

- Joins current battle actors to their owned parameters and expected party IDs.
- Follows only occupied hash chains in the UI unit-view-model map, including
  sparse maps with holes. It checks map keys against each VM's unique ID.
- Distinguishes current, null, invalid, outside-current, unsupported and
  unreadable cached parameters. A failed host read never invents a `None` result.
- Records the relation virtual's code bytes and image-relative target for private
  review, without executing them or claiming the actual returned relation.
- Rereads copied headers, arrays, rows and values to reject observed drift. The
  result explicitly remains non-atomic and non-restorable.

Raw pointers and code samples are private research evidence and stay outside Git.
Nine synthetic memory tests cover normal sparse links, null/invalid/unreadable or
retired references, malformed hash chains, identity/ownership drift, unsupported
virtual targets and failed/short reads. This diagnostic is not live verified.

## Next supervised comparison

Capture the same data at a natural untouched ordinary first menu and immediately
after one recreation. Retain the independently exported opening for each sample,
not an earlier chapter's party list. Compare each VM's cached parameter against
the actual current actors, then inspect the rendered portraits/anchors and MVVM
binding state. If all parameter links remain current, inspect the relation virtual
and notification path before proposing a UI refresh. If links are absent or stale,
trace battle-end/start ownership and deploy/rebind notifications first.

Do not select attacks, use item-cost retry routes or modify save slots to diagnose
this issue. Keep the installed loader disabled until supervised testing resumes.
