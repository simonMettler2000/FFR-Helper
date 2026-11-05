# UE4SS redistribution: revisit before installer or release work

**5 November WIP publication:** UE4SS remains a separate upstream download. The
README links the exact compatible official archive and its checksum; this
repository adds no UE4SS binary bundle or automatic downloader/installer. The
notes below remain relevant before changing that policy.

Recorded 29 October 2026 at the user's request. **Bring this issue back to the
user when revisiting automatic installation, bundling UE4SS or publishing a
release.** This is a record of licensing findings, not legal clearance.

The installed `UE4SS.dll` came from the community UE4SS release, rather than a
DLL copied from FFR. The pinned UE4SS revision `cdbc0b66` has an MIT license.
That license permits redistribution of the software it covers when the copyright
and permission notices are retained:

- [Pinned UE4SS license](https://raw.githubusercontent.com/UE4SS-RE/RE-UE4SS/cdbc0b66/LICENSE)

However, UE4SS's official contributing documentation explicitly says UEPseudo
code is subject to Epic Games' licensing terms. The pinned revision includes
UEPseudo as its `deps/first/Unreal` dependency. Consequently, the root MIT notice
alone does not establish redistribution rights for every part of a compiled DLL:

- [UE4SS licensing notes](https://docs.ue4ss.com/dev/contributing.html#license)
- [Pinned dependency declaration](https://raw.githubusercontent.com/UE4SS-RE/RE-UE4SS/cdbc0b66/.gitmodules)
- [Unreal Engine EULA](https://www.unrealengine.com/eula/unreal)

FFR Helper's current packages do not include UE4SS itself. The preferred approach
pending a complete review is to keep that prerequisite separate: accept an
existing compatible installation or fetch a pinned, checksum-verified official
release during an explicit installation action. That can still be automated,
but downloading separately does not waive applicable license obligations.

Before changing that policy, review the exact release's compiled dependencies,
proxy loader and accompanying notices; obtain clarification or qualified legal
review where the redistribution rights remain uncertain. Do not include game
executables, game assets or proprietary Epic DLLs without applicable permission.
This note does not clear the other libraries or game-derived data in the project.

No license analysis can guarantee that a takedown notice will never be submitted.
GitHub processes qualifying notices; a takedown is not itself a final court
ruling that infringement occurred:

- [GitHub DMCA takedown policy](https://docs.github.com/en/site-policy/content-removal-policies/dmca-takedown-policy)

Open decision: whether a future installer should download UE4SS on demand or
whether bundling a specifically reviewed release is permitted. Raise this with
the user before implementing or publishing that distribution change.
