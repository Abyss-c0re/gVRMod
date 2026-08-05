# State Matrix Exchange (SMX) — prototype module

**Status:** separate lab binary. **Not** linked into CubeUI or `gmcl_vrmod_*`.

Code: `experimental/smx_proto/`  
How-to: `experimental/smx_proto/README.md`

Wire framing:

```
TCP stream:  [u32le N][payload N bytes]
```

Payload: `SmxPlayerMatrix` in `experimental/smx_proto/smx_player.hpp`.

| Variable | Role |
|----------|------|
| `GVRMOD_SMX_BIND` | Serve `host:port` |
| `GVRMOD_SMX_PEER` | Dial `host:port` |
| `GVRMOD_SMX_KEY` | Optional shared secret |
| `GVRMOD_SMX_HZ` | TX rate (default 20) |

Prove SERVE/DIAL + matrix round-trip here first. Only then consider product integration.

Related deferred ideas: `docs/concept/CUBALC_FUTURE.md`.
