# Stereo / head-tilt settings (Lua + C++)

**Status:** dialable · defaults **safe** · FOV crop opt-in only  
**Symptom:** light warp when rolling head (ear toward shoulder).  
**Not a fix:** `vrmod_swap_eyes` (content L↔R only; will not cure tilt warp).

## Two independent layers

| Layer | What it controls | Warp role |
|-------|------------------|-----------|
| **A. Eye placement** (Lua render) | Where each `RenderView` camera sits | Roll needs IPD along **head Right()**, not world right |
| **B. Submit UV** (C++ blit) | Which rect of the eye texture goes to OpenXR | Wrong crop = crossed / broken stereo |

Defaults keep **A = auto**, **B = safe** (full per-eye / Lua SBS bounds).  
Experimental FOV crop is **never** the default (it previously destroyed stereo).

---

## Lua convars (Vision / console)

| Cvar | Default | Meaning |
|------|---------|---------|
| `vrmod_stereo_eye_mode` | **0** | **0** auto (XR eyes if valid, else synthetic) · **1** force OpenXR eye poses · **2** force synthetic head-Right IPD |
| `vrmod_eyescale` | 0.5 | Synthetic IPD scale (mode 2 or auto fallback). Try **1.0** for full IPD |
| `vrmod_submit_crop` | **0** | Pushes to C++: **0** safe · **1** full · **2** FOV crop **experimental** |
| `vrmod_renderoffset` | 1 | SBS auto UV from projection (legacy auto-offset) |
| `vrmod_scalefactor` | 1 | UV border scale; nudge ±0.05 for edge warp |
| `vrmod_horizontaloffset` / `vrmod_verticaloffset` | 0 | Manual UV bias (with auto off or as extra) |
| `vrmod_swap_eyes` | **0** | Content half swap only — **leave off** unless stereo is truly inverted |

Live: crop + UV apply without VR restart. Eye mode applies next stereo frame.

---

## C++ backend (`VRMOD_SetSubmitCropMode`)

| Mode | Behavior |
|------|----------|
| **0 SAFE** | Per-eye / collector → full eye rect `inset…1-inset`. SBS → Lua `textureBounds` halves |
| **1 FULL** | Force full-eye UV (debug borders / “is crop the problem?”) |
| **2 FOV_CROP** | Asymmetric FOV tan crop on single-eye textures. **Opt-in only** — can look crossed |

API:

```
VRMOD_SetSubmitCropMode(n)  -- 0..2
VRMOD_GetSubmitCropMode()   -- current
```

Lua `ApplySubmitBounds` calls `SetSubmitCropMode` from `vrmod_submit_crop` every bounds refresh.

---

## Recommended dial order (Quest 3 / WiVRn)

1. Confirm `vrmod_swap_eyes 0` and `vrmod_submit_crop 0`.
2. **Roll test:** `vrmod_stereo_eye_mode 2` + `vrmod_eyescale 1`  
   If roll improves → placement was the soft wrap; stay here or set auto after.
3. If edges only: leave eyes auto, nudge `vrmod_scalefactor` / H-V offsets; keep crop 0.
4. **Only if still soft wrap and you accept risk:** `vrmod_submit_crop 2` for one short test, then **back to 0**.
5. Never leave crop 2 on as a “fix.”

---

## Creed

- Shared HMD **orientation** for both eyes (Source cannot do true per-eye rot).  
- IPD is **translation** along head right (or XR eye positions).  
- Backend crop is a **submit** knob, not a second pose stream.  
- Safe default always; experimental behind explicit cvar.
