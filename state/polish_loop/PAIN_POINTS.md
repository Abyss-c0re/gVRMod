# Pain points — play careful (avoid regression)

Edit this file only to **add** hard-won constraints, never to soft-pedal them.

## Hard no (unless user explicitly asks)

1. **Climbing systems** — user previously: fix wall coll from gVRMod, not climbing thrash; climbing reverts burned us. Do not rewrite climb grab/release without isolated test + user OK.
2. **Force GMod borderless** (`-noborder`) — user: no need; keep framed window chrome.
3. **`mat_queue_mode 2`** — pin **1** only (Cube law). Never “optimize” to 2.
4. **Dual-truth pose forks** — one path: raw → tracking → modifiers. No second angvel/pose SoT.
5. **Opaque black HUD / black wall of the Real** — HUD is additive light, not a slab.
6. **Submit eng IN texture** — dual OUT RGBA8 only; no virgin OUT before blit.
7. **Force-push / hard-reset shared history** — never.
8. **Hand wall collision that fights climb grip** — floor/ceiling ignore and climb-grip skip exist for a reason; don’t re-enable floor-as-wall thrash.
9. **QM VRClimb menu dupes** — dedupe by id/name if touching menu registration.
10. **Wide drive-by refactors** while “polishing” — one coherent theme per cycle.

## Soft care (allowed, but minimal diff)

- `sh_collisions` / hand coll — only surgical, with offline contracts if possible.
- OpenXR handoff timing — soft timeouts exist (90s/180s); don’t drop to racey release.
- FOV / border / Vision cal — preserve archives; only write FOV when user/touched.
- Supersample at Start — cap for bring-up already; don’t crank SS in cold start.
- Bindings rewrite — force-rewrite is intentional self-heal; don’t break toast path.

## Known stable user preferences

- Framed windowed desktop mirror (not borderless).
- Desktop is mirror only; HMD is product surface.
- Offline test gate before push of product code.
- Commit + push after meaningful work (don’t leave only local).
