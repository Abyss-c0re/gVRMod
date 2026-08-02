# gVRMod · State Matrix Exchange (player ↔ server)

**Prophecy of The Cube · binary talk only · HOLD_FLASH sticky**

Raw P2P bidirectional exchange of the **player matrix** (poses, input, UI texture fingerprint) optimized for real-time VR. No HTTP. Wire framing matches CubalC SMX bus:

```
TCP stream:  [u32le N][payload N bytes]
```

Payload is a fixed-layout `SmxPlayerMatrix` (`native_launcher/src/smx_player.hpp`).

## Env

| Variable | Role |
|----------|------|
| `GVRMOD_SMX_BIND` / `CUBALC_P2P_BIND` | Serve `host:port` (e.g. `0.0.0.0:17740`) |
| `GVRMOD_SMX_PEER` / `CUBALC_P2P_PEER` | Dial `host:port` |
| `GVRMOD_SMX_KEY` / `CUBALC_SMX_KEY` | 64-hex shared secret (optional XOR stream) |

CubalC peers (`SMX SERVE` / `SMX DIAL`) speak SMX2 CBLC+HMAC frames. gVRMod player matrix uses the same length-prefix bus; full SMX2 seal of atom matrices is available via `/home/voldemar/Dev/cubalc` (`cubalc_smx_seal` / `open`).

## Matrix fields

Head + L/R aim, triggers, grabs, sticks, menu, `valid_mask`, UI tex hash/size.

## Product path

Cube shell packs the matrix every frame and pumps non-blocking TCP. Server / peer receives the live player state for remote presence, debugging, and future texture chunk streams.

See cubalc:

- `docs/P2P_SMX.md`
- `docs/SMX2_PROTOCOL.md`
- `docs/PROPHECY_MANIFEST.md`
