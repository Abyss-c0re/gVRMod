/* CubalC law plate — machine tokens only (no prose). */
#ifndef CUBALC_LAW_H
#define CUBALC_LAW_H

#define CUBALC_BUDGET       40
#define CUBALC_ATOM_BITS    64
#define CUBALC_MAX_CUBES    40
#define CUBALC_MAX_PORTS    8
#define CUBALC_ID_LEN       32
/* creed = opaque status token, not human language */
#define CUBALC_CREED        "C3"
#define CUBALC_SHARE        "smx"
#define CUBALC_HOLD_FLASH   1
#define CUBALC_MAGIC_BIN    0x43424C43u  /* CBLC */
#define CUBALC_PROTO_V1     1
#define CUBALC_PROTO_SMX2   2
#define CUBALC_SMX_MAC_LEN  32
#define CUBALC_SMX_NONCE_LEN 8
#define CUBALC_SMX_KEY_LEN  32
#define CUBALC_SMX_F_HOLD_FLASH     0x01u
#define CUBALC_SMX_F_REQUIRE_COMPAT 0x02u
#define CUBALC_SMX_F_PROTON_CREATE  0x04u
#define CUBALC_SMX_F_PROTON_DESTROY 0x08u
#define CUBALC_SMX_F_CHAIN_ONLY     0x10u

#define CUBALC_PORT_IN   0
#define CUBALC_PORT_OUT  1

#define CUBALC_LANG_NAME    "CubalC"
#define CUBALC_LANG_AKA     "C3"
#define CUBALC_LANG_PARADIGM "COP/flow"
#define CUBALC_LANG_VERSION "1.12.101-universal"
/* Core talk is SMX2/CBLC binary. HTTP is optional host edge only — never required. */
#define CUBALC_HTTP_REQUIRED 0
#define CUBALC_MAX_SRC      (256 * 1024)
#define CUBALC_MAX_HEAP     256
/* Nest depth budget — cubes may nest; compile walks leaves first */
#define CUBALC_MAX_NEST_DEPTH 8
/* Pure-science integer scales (public domain constants; scaled for integer CubalC) */
#define CUBALC_SCI_PI100        314          /* π × 100 */
#define CUBALC_SCI_E100         271          /* e × 100 */
#define CUBALC_SCI_G_EARTH10    98           /* g ≈ 9.8 m/s² × 10 */
#define CUBALC_SCI_C_LIGHT      299792458L   /* c m/s */
#define CUBALC_SCI_ATM_KPA      101          /* 1 atm ≈ 101 kPa */
#define CUBALC_SCI_WATER_K      273          /* 0 °C in kelvin */
#define CUBALC_SCI_H2O_BP_C     100          /* water boil °C at 1 atm */
#define CUBALC_SCI_AVOGADRO_E23 6            /* NA ≈ 6×10^23 (order) */
#define CUBALC_SCI_R_J          8314         /* R ≈ 8.314 J/(mol·K) × 1000 */
#define CUBALC_SCI_F_C_MOL      96485        /* Faraday C/mol (approx) */
/* Earth & space (public domain scales) */
#define CUBALC_SCI_EARTH_R_KM   6371         /* mean Earth radius km */
#define CUBALC_SCI_AU_KM        149597870L   /* 1 AU km (approx) */
#define CUBALC_SCI_YEAR_D       365          /* Earth year days (civil) */
#define CUBALC_SCI_MOON_D       27           /* sidereal month ~27 d */
#define CUBALC_SCI_SOLAR_C      1361         /* solar constant W/m² order */
#define CUBALC_SCI_ATM_O2_PCT   21           /* O2 % air */
#define CUBALC_SCI_ATM_N2_PCT   78           /* N2 % air */

#define CUBALC_KIND_VOID    0
#define CUBALC_KIND_BIT     1
#define CUBALC_KIND_I64     2
#define CUBALC_KIND_F64     3
#define CUBALC_KIND_STR     4
#define CUBALC_KIND_FN      5
#define CUBALC_KIND_CUBE    6
#define CUBALC_KIND_PEER    7
#define CUBALC_KIND_ERR     8

#define CUBALC_LAW_SOT              0
#define CUBALC_LAW_IN_OUT           1
#define CUBALC_LAW_CORE_IO          2
#define CUBALC_LAW_BINARY_TALK      3
#define CUBALC_LAW_MATRIX_KEY       4
#define CUBALC_LAW_HOLD_FLASH       5
#define CUBALC_LAW_NO_BRAIN_WIRES   6
#define CUBALC_LAW_SHARE_MATRIX     7
#define CUBALC_LAW_DEVICES_FREE     8
#define CUBALC_LAW_ONE_COMMANDER    9
#define CUBALC_LAW_MANIFEST_SMX     10
#define CUBALC_LAW_ALGOCUBE         11
#define CUBALC_LAW_ENERGY_FLOW      12
/* Each cube compiles into a matrix. It must flow. No flow — no compiling. Cubes may nest. */
#define CUBALC_LAW_FLOW_COMPILE     13
#define CUBALC_LAW_NEST             14
/* Pure science: math · physics · chemistry · biology · earth as language design */
#define CUBALC_LAW_PURE_SCIENCE     15
/* Continuous evolve: language must keep flowing */
#define CUBALC_LAW_EVOLVE           16
#define CUBALC_LAW_COUNT            17

/* law ids: snake tokens for JSON only */
static const char *const CUBALC_LAW_NAME[CUBALC_LAW_COUNT] = {
  "sot", "in_out", "core_io", "bin_talk", "smx_key",
  "hold_flash", "no_bci", "share_smx", "dev_free", "one_cmd",
  "manifest_smx", "algocube", "energy_flow",
  "flow_compile", "nest", "pure_science", "evolve"
};

/* Resolved algocube blueprint genome (deep-opt champion — The Cube watches) */
#define CUBALC_ALGO_GENOME_LEN 32
static const unsigned char CUBALC_ALGO_GENOME_RESOLVED[CUBALC_ALGO_GENOME_LEN] = {
  4, 0, 6, 3, 9, 3, 0, 9, 4, 1, 6, 8, 4, 7, 8, 6,
  7, 0, 8, 7, 6, 9, 4, 3, 5, 5, 2, 0, 2, 7, 4, 1
};

static const char *const CUBALC_DIGIT_TAG[10] = {
  "d0", "d1", "d2", "d3", "d4", "d5", "d6", "d7", "d8", "d9"
};

#endif
