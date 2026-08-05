# gVRMod · State Matrix Exchange (optional / future)

**Status:** off by default. Not part of the core OpenXR / WiVRn / CubeUI ship path.

Optional raw TCP exchange of a packed **player matrix** (poses, sticks, triggers, UI frame hash) for later multiplayer / debug tooling. Framing:

```
TCP stream:  [u32le N][payload N bytes]
```

Payload layout: `SmxPlayerMatrix` in `native_launcher/src/smx_player.hpp`.

## Enable (lab only)

| Variable | Role |
|----------|------|
| `GVRMOD_SMX_BIND` | Serve `host:port` (e.g. `0.0.0.0:17740`) |
| `GVRMOD_SMX_PEER` | Dial `host:port` |
| `GVRMOD_SMX_KEY` | Optional shared secret |
| `GVRMOD_SMX_HZ` | Max pump rate |

Without bind/peer, SMX is idle (no sockets, no cost).

## Related

Longer-term matrix / neural bus ideas: see `docs/concept/CUBALC_FUTURE.md` (deferred).
