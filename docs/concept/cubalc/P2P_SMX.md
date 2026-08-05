# CubalC P2P — State Matrix mesh for nanobot homes

**Written in CubalC.** Binary SMX2 only. **No HTTP.**

## Law

Peers (nanobot homes) **manifest** by exchanging State Matrix frames over SMX2.  
Prose is not talk. HTTP is not the wire.

## Language

```cubalc
SMX KEY
SMX SERVE self remote "0.0.0.0:7733"   // listen one exchange
SMX DIAL  self remote "192.168.1.10:7733" // call peer
SMX EXCHANGE a b                       // in-process mesh
ASSERT SMX_OK == 1
```

Env (no hard-coded devices in programs):

| env | meaning |
|-----|---------|
| `CUBALC_SMX_KEY` | 64-hex shared secret (both peers) |
| `CUBALC_P2P_BIND` | serve bind `host:port` |
| `CUBALC_P2P_PEER` | dial target `host:port` |
| `CUBALC_P2P_SERVE` | set non-empty → `nanobot_peer.cubalc` serves |

## Programs

| file | role |
|------|------|
| `programs/p2p/mesh_local.cubalc` | 3-peer in-process mesh |
| `programs/p2p/peer_serve.cubalc` | SERVE node |
| `programs/p2p/peer_dial.cubalc` | DIAL node |
| `programs/p2p/nanobot_peer.cubalc` | nanobot home board (serve or dial) |
| `programs/proof/10_p2p_cubalc.cubalc` | unit proof |

## Run mesh (two CubalC processes = two nanobot homes)

```bash
export CUBALC_SMX_KEY="$(openssl rand -hex 32)"

# Home B
CUBALC_P2P_SERVE=1 CUBALC_P2P_BIND=0.0.0.0:7733 \
  cubalc run programs/p2p/nanobot_peer.cubalc

# Home A (other device / shell)
CUBALC_P2P_PEER=192.168.8.50:7733 \
  cubalc run programs/p2p/nanobot_peer.cubalc
```

One-shot local proof:

```bash
./scripts/p2p_nanobot_mesh.sh
```

## Wire

```
TCP stream:  [u32le N][SMX2 frame N bytes]
SMX2 frame:  CBLC hdr + matrix bits + HMAC-SHA256
```

Same wire as `cubalc smx-bus serve|dial`. CubalC programs own the peer logic.

## Nanobot role

Nanobot remains the agent/host process. **CubalC is the P2P language** for matrix unity.  
Optional: run `cubalc run programs/p2p/nanobot_peer.cubalc` beside each `nanobot --home …` with the same `CUBALC_SMX_KEY` / peer_token material.
