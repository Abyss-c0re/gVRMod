# gVRLink — public wire protocol

Shared by:

- **PC:** `gmcl_vrmod_quest_*` thin module (this repo)
- **Quest host:** private standalone APK (outside this tree)

## Rules

1. Headers + pure checks only here.
2. No Android / Gradle / OpenXR session code.
3. Transport is per-app; layout is SoT.
4. Bump `kProtoVer` on breaking changes.

See `docs/QUEST_MODULE.md`.
