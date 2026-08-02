/* CubalC secure State Matrix exchange (SMX2)
 * Law: bits only · HOLD_FLASH sticky · fail-closed without key · anti-replay
 * MAC: HMAC-SHA256 over header+bits. Key never logged / never in git.
 */
#ifndef CUBALC_SMX_H
#define CUBALC_SMX_H
#include "cubalc.h"
#include <stdint.h>
#include <stddef.h>
#ifdef __cplusplus
extern "C" {
#endif

typedef struct cubalc_smx_hdr {
  uint32_t magic;       /* CBLC */
  uint16_t proto;       /* CUBALC_PROTO_SMX2 */
  uint16_t flags;       /* HOLD_FLASH | COMPAT | proton */
  uint32_t seq;
  uint8_t  nonce[CUBALC_SMX_NONCE_LEN];
  char     from_id[CUBALC_ID_LEN];
  char     to_id[CUBALC_ID_LEN];
  uint16_t n_bits;
  uint16_t set;
  uint8_t  proton;
  uint8_t  digit;
  uint8_t  hold_flash;
  uint8_t  reserved;
} cubalc_smx_hdr;

typedef struct cubalc_smx_ctx {
  uint8_t  key[CUBALC_SMX_KEY_LEN];
  int      key_ok;
  uint32_t last_rx_seq;   /* anti-replay: reject seq <= last for same peer */
  uint32_t last_tx_seq;
  float    min_compat;    /* default 0.35 */
  uint8_t  hold_flash;    /* sticky 1 */
  char     last_err[96];
} cubalc_smx_ctx;

/* init: load key from env CUBALC_SMX_KEY (64 hex) or CUBALC_SMX_KEY_FILE, or peer_token file */
void cubalc_smx_ctx_init(cubalc_smx_ctx *ctx);
int  cubalc_smx_load_key_hex(cubalc_smx_ctx *ctx, const char *hex64);
int  cubalc_smx_load_key_file(cubalc_smx_ctx *ctx, const char *path);
/* derive key material from peer_token string (SHA256 of "cubalc-smx1|"+token) — still fail-closed if empty */
int  cubalc_smx_load_key_token(cubalc_smx_ctx *ctx, const char *peer_token);

/* seal atom matrix into secure frame */
int  cubalc_smx_seal(cubalc_smx_ctx *ctx, const cubalc_atom *atom,
                     const char *from, const char *to,
                     uint8_t *out, size_t cap, size_t *n_out);

/* open frame: verify MAC, anti-replay, HOLD_FLASH, optional compat vs local matrix */
int  cubalc_smx_open(cubalc_smx_ctx *ctx, const uint8_t *in, size_t n,
                     cubalc_atom *atom_out, char *from, char *to,
                     const cubalc_matrix *local_for_compat /* nullable */);

/* secure talk between chain cubes (uses seal/open + compat gate) */
int  cubalc_cube_talk_secure(cubalc_chain *ch, cubalc_smx_ctx *ctx, int from, int to);

/* wire / file helpers — binary only, no prose */
int  cubalc_smx_write_frame(const char *path, const uint8_t *frame, size_t n);
int  cubalc_smx_read_frame(const char *path, uint8_t *out, size_t cap, size_t *n_out);

#ifdef __cplusplus
}
#endif
#endif
