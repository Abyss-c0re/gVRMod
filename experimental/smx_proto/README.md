# SMX prototype (lab only)

**State Matrix Exchange** as a **separate module**. Not linked into CubeUI or the GMod OpenXR binary.

## Build

```bash
cmake -S experimental/smx_proto -B experimental/smx_proto/build
cmake --build experimental/smx_proto/build -j
```

## Run (two terminals)

```bash
# A — serve
GVRMOD_SMX_BIND=0.0.0.0:17740 ./experimental/smx_proto/build/smx_proto

# B — dial
GVRMOD_SMX_PEER=127.0.0.1:17740 ./experimental/smx_proto/build/smx_proto
```

Optional: `GVRMOD_SMX_HZ=20`, `GVRMOD_SMX_KEY=<hex>`.

Without bind/peer the tool exits immediately (safe).

## Product

Ship path ignores this directory. Re-integration into the launcher only after the wire protocol is proven here.
