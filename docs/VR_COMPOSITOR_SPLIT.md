# VR compositor split — GMod paints, Cube (or peer) owns XR

**Status:** design SoT · partial product today (Cube holds XR until `take_xr`)  
**Goal:** minimize what GMod loads for VR; enhance / buffer textures outside GMod; sync at submit; prepare for mat_queue 2; matrix rain on launch blend.

---

## Your idea (validated shape)

```text
┌──────────────────────┐         GPU texture / fence
│  GMod (thin client)  │ ──────────────────────────► ┌─────────────────────────┐
│  · map + gameplay    │   dual OUT RGBA (or SBS)    │  XR compositor process  │
│  · dual RenderView   │   NO WaitFrame / EndFrame   │  · OpenXR session       │
│  · mat_queue 1 or 2  │                             │  · queue / enhance      │
│  · no XR thrash      │ ◄──── ready / pose / time ──│  · matrix rain blend    │
└──────────────────────┘                             │  · EndFrame to HMD      │
                                                     └─────────────────────────┘
```

**Sync point = texture submission**, not “load module on worker then merge GL handles.”

---

## What already exists

| Piece | Role today |
|-------|------------|
| **Cube launcher** | Separate process; owns OpenXR + passthrough panel until GMod `take_xr` |
| **gVRMod module** | Inside GMod; WaitFrame + paint + Collect + Submit (full XR in-process) |
| **Handoff** | File markers + fade + ambient; Cube releases XR when GMod takes it |

So process-level split is **half-built**. Full split means GMod **stops being the OpenXR app** during play and only **produces frames**.

---

## Thread vs process (important)

| Approach | Benefit | Risk |
|----------|---------|------|
| **Worker thread inside GMod** for XR | Little | GL context ownership, mat_queue races, SEGV |
| **Separate process** (Cube / `cube_xr_compositor`) | Real isolation; mq2 freer for engine | IPC + shared GPU texture design |
| **Async CPU prep only** | Easy wins | No GL/XR handles off main |

**Prefer process (or Cube extended), not a second GMod thread that owns OpenXR.**

---

## Texture pipeline (target)

1. **GMod** paints stereo into **engine dual OUT** (or shared GL/D3D texture).  
2. **Signal fence** “frame N ready” (no blocking WaitFrame on GMod if compositor paces).  
3. **Compositor** dequeues, optional **enhance** (sharpen / tone / rain overlay), **double-buffer**.  
4. **xrEndFrame** with predicted display time owned by compositor.  
5. GMod only needs **pose/time** feedback for next view (or use last pose + reprojection later).

**Buffering:** 1–2 frames in compositor is the sweet spot. More = latency.  
**Enhance:** only in compositor GL context, never on mat_queue workers.

---

## mat_queue_mode 2 (honest path)

Today product pins **mq=1** and forbids dual nested `RenderView` under 2 (CThread).

| Step | Backend prep |
|------|----------------|
| 1 | GMod paints **left only** or true single-pass (already `mq2_single_pass`) |
| 2 | Engine finishes; **no** mid-frame FBO rebind from module |
| 3 | Compositor **copies** last complete RT (same idea as CollectEyes staging) |
| 4 | All OpenXR + blit + gl* live **only** in compositor process |

**Benefit of split for mq2:** engine workers never see `glFinish` / swapchain / WaitFrame.  
**Still required:** GMod must not thrash `mat_queue_mode` mid-session; dual paint under 2 stays banned until proven.

Backend hooks to keep (module side stays thin):

- `CollectEyes` / staging (already) → becomes “export frame to compositor”  
- `SetKnownSubmitSize` — size SoT  
- **No** WaitFrame on engine path in full split  

---

## Phases

| Phase | Ship |
|-------|------|
| **P0 now** | Cube holds XR; GMod full module; fade + **matrix rain** on take_xr (pure C + GL overlay) |
| **P1** | Shared-memory / GL share descriptors for dual OUT; Cube keeps session through play (experimental) |
| **P2** | GMod module = paint + fence only; Cube = WaitFrame/enhance/EndFrame |
| **P3** | Optional enhance chain + mq2 single-pass producer validated |

---

## Matrix rain (launch blend)

**When:** Cube `take_xr` / handoff fade (passthrough + panel → rain → black/GMod).  
**Where:** Cube native (pure C sim + GL overlay), **not** GMod.  
**Why pure C:** offline testable; no Lua; no mat_queue.

See `native_launcher/src/matrix_rain.hpp` + `GlMatrixRainOverlay`.

---

## Creed

- GMod **produces** pixels; compositor **presents** them.  
- Sync at **texture + fence**, not cross-thread GL handles inside GMod.  
- mat_queue 2 only safe when engine and XR **don’t share thrash**.  
- Reality blend effects live on **Cube** while it still owns the session.
