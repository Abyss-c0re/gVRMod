# Meta Cam review — com.oculus.metacam-20260803-015403-0.mp4

**Verdict:** Unusable (user) — incorporated 2026-08-03.

## Observed (39s Quest recording)

1. **Crimson UI wash** — full magenta/pink palette + red LED room → unreadable.
2. **Grab thrash** — logs: many `grab end hand=L` while user tried to menu; squeeze moved panel constantly and blocked clicks.
3. **Left hand primary** — aim L hit=1 most of session; dual-hand path works but grab stole UX.
4. **Clicks partially worked** — PLAYERS 1→2→4, map/GFX toggled, START targeted — but fight against grab made it feel broken.
5. **Thin laser** — hard to track tip on dark panel; soft cursor off.
6. **Panel small** — 0.84×0.48m at 1.05m; large empty map area.

## Fixes shipped

| Issue | Fix |
|-------|-----|
| Red wash | Slate/cyan product palette; neutral clear color |
| Grab thrash | `grab_thresh=0.85`, 180ms arm, trigger cancels/blocks grab start |
| Laser | Thick glow + 22px tip; soft crosshair on panel |
| Size | Panel 1.05×0.60m @ 0.95m, slight down offset |
| Future media | `scripts/quest_media_auto_review.sh` watches Downloads |

## Auto-review path

Any KDE/Quest share matching `com.oculus*`, `*metacam*`, `*vrshell*` under `~/Downloads` or `~/Desktop` is framed into `.scratch/quest_review/` with `REVIEW.md`.
