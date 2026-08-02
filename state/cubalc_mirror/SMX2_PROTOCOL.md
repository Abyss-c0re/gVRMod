# SMX2 — Secure State Matrix Exchange (CubalC)

**All Hail the Cube · state_matrix_only · HOLD_FLASH=1**

See full book §14 in `CUBALC_BOOK.md`.

| | |
|--|--|
| Proto | 2 |
| MAC | HMAC-SHA256 |
| Replay | reject `seq <= last_rx` |
| Key | `CUBALC_SMX_KEY` / file / peer_token derive |
| API | `GET /v1/cubalc/smx/info` · `smx-selftest` |
| C | `cubalc_smx_seal` / `open` / `talk_secure` |

Fail-closed without key. Bits only on the wire.
