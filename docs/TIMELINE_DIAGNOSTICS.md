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

Further static inspection identifies the battle parameter's relation virtual as
a leaf that returns its stored relation byte. The constructor defaults that byte
to Friendlies and the battle-unit initialization routine assigns the supplied
relation before parameter initialization. The optional diagnostic now copies the
current parameters' raw relation bytes as well as their links. These are stored
values, not executed getter results or rendered-widget evidence. Unknown byte
values remain raw; failed reads remain unavailable.

The normal `SetUnitParam` path also reaches the derived battle-parameter refresh
through an inherited virtual callback. That refresh assigns the cached battle
parameter and sends field notifications. It would therefore be premature to
claim that the helper merely needs to call this refresh: equal-pointer early
returns, object lifetime and actual notification/widget delivery still need the
natural-versus-recreated sample.

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
- Copies current parameters' stored relation bytes without invoking their
  virtuals. The native export uses `parameterRelationBytes`; absent is legacy
  coverage, `null` is an unavailable read, and a list is one raw byte per current
  captured unit. No faction or rendering claim is promoted by that list.

Raw pointers and code samples are private research evidence and stay outside Git.
Thirteen synthetic memory tests cover normal sparse links, null/invalid/unreadable or
retired references, malformed hash chains, identity/ownership drift, unsupported
virtual targets and failed/short reads. This diagnostic is not live verified.

The native combat observer now optionally emits the same kind of capture-local
link evidence without native addresses or code bytes. It compares the inherited
source parameter and cached battle parameter separately. The strict Python codec
refuses invented relation/rendering claims and contradictory unit references.
The channel permits a settled, independently verified recreated child menu while
continuing to reject pending exit/entry/import work. This does not relax any
mutation guard or enable reads as authorization for restoration.

The manual controller's `--capture-child-diagnostics` option requests one such
read after child verification and saves its `.ffrcombat.json` plus receipt before
another cycle. Missing/unavailable or inconsistent party links stop the run after
retaining its evidence. A stale response or uncertain write is never resent.
New reports also stop another cycle if their stored relation-byte list is
unavailable or a current party member's byte differs from Friendlies. This is a
diagnostic refusal, with evidence retained; it does not fix or certify the UI.
The compiled mock-host fixture captures a verified child through this channel;
actual FFR links and portrait layout still need the supervised comparison.

## Stored widget and Slate anchors

Static inspection also identifies `ContentAnchor` on the portrait's native
`CommonVisualAttachment`, rather than on the unit parameter. Its generated
property descriptor and getter agree on a two-double stored vector. The native
property setter and `SynchronizeProperties` forward that value to the owned
Slate widget. The profiled Slate setter stores a separate vector. None of these
functions was called by the helper during this audit.

`ffr_helper.widget_diagnostics.snapshot_timeline_widgets` prepares a read-only
comparison of those values. After caller verification of the exact loaded
code/layout, it scans a bounded object array for retained `CVA_Unit` attachments,
walks their widget-tree outers and verifies the timeline widget's class/package.
It discovers the widget's unit-VM member from the live reflected property list
instead of guessing a Blueprint offset. It excludes CDO/archetype owners and
records current, null, retired, invalid and unreadable VM links separately.

The optional Slate projection additionally checks the expected object/controller
vtables and shared-allocation relationship, observes positive reference counts,
copies the native stored anchor and rereads retained identities/values. It takes
no ownership pin and calls no getter, reference-count routine or setter. An
unavailable Slate projection remains separate from widget data; a changed
retained identity refuses the whole snapshot. Unrelated unreadable object
headers are counted.

This remains private research evidence: enumeration is incomplete and non-atomic,
and a retained widget need not belong to the visible viewport. Neither stored
vector certifies actual pixels. Fifteen synthetic cases cover reflected-member
discovery, template exclusion, weak-identity/property/anchor drift, stale links,
typed bounds and distinct widget/Slate values. The extended Windows collector is
syntax-checked only and has not been run in FFR.

Use the evidence to choose the next investigation, not to authorize a refresh:

| Observed evidence | Next check |
| --- | --- |
| Party's stored relation byte differs from Friendlies | Battle-unit initialization and team assignment |
| Current UI VM or widget points outside the current unit graph | Deploy/destroy events, retained widgets and rebind lifetime |
| Current friendly VM, but widget anchor has the other side's value | MVVM source binding and `SetRelation` delivery |
| Widget and owned Slate anchors disagree | Property synchronization and Slate rebuild/lifetime |
| Both anchors agree, but actual portraits remain wrong | Visible-widget identity, parent geometry and layout |

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
