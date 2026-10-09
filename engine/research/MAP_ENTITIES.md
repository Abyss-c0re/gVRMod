# Map entities (VBSP 20)

Scan date: 2026-10-09. Read-only parse of on-disk BSPs with Python `struct`. No entity lumps were copied into this note.

## How the numbers were read

Header (`<` = little-endian):

- 4-byte ident, then `int32` version
- 64 lumps × 16 bytes: `int32 fileofs`, `int32 filelen`, `int32 version`, 4-byte fourCC
- `int32 mapRevision` at offset 1032
- Header is 1036 bytes. On every file below, lump 1 (`planes`) starts at offset 1036.

Lump indexes used (Source `bspfile.h`):

| Lump | Index | Count stride |
| --- | ---: | --- |
| entities | 0 | text, not a fixed record |
| planes | 1 | 20 (`dplane_t`) |
| vertexes | 3 | 12 (3 floats; implied only) |
| faces | 7 | 56 (`dface_t`) |
| edges | 12 | 4 (implied only) |
| surfedges | 13 | 4 (implied only) |
| brushes | 18 | 12 (`dbrush_t`) |
| brushsides | 19 | 8 (`dbrushside_t`) |
| dispinfo | 26 | 176 (`ddispinfo_t`), and only if `filelen > 0` |
| disp_verts | 33 | 20 (`dDispVert`) |

Every nonempty lump above divides by its stride with remainder 0. Faces are lump version 1 on every file; the other lumps in the table are version 0. Empty disp lumps stay at count 0 (their `fileofs` is the next lump’s offset, not a missing header).

Displacement check: for each dispinfo record, `power` is the `int32` at offset 20, and the vert count implied by `(2^power + 1)^2` sums to `disp_verts / 20` on every map that has displacements.

Entities: lump bytes are UTF-8 plus one trailing NUL inside `filelen`. Blocks are split on `{` `}` that are outside quotes. Inside a block, the first copy of a key wins. `info_player_start` “first” means first in this lump order, not the spatial minimum.

LZMA: a lump counts as LZMA only when its fourCC is `LZMA`. Result on every file: **none**. All 64 fourCC fields are `00 00 00 00`. No lump `fileofs`/`filelen` runs past EOF.

`prop_static` is not an entity-lump classname. Counts in the static-prop table come from game lump 35, id bytes `prps` (on-disk little-endian form of MSVC `'sprp'`): `int` name count, 128-byte names, `int` leaf count, `uint16` leaves, `int` prop count. Bytes left after that header divide evenly by the prop count (64 bytes at sprp version 6, 72 at version 10).

## Files

`find …/garrysmod -maxdepth 4 -name '*.bsp'` returned only these seven. A second find with no depth limit under `garrysmod/addons` returned the same four addon maps. No other BSP at depth ≤ 4.

| Map | Bytes | mapRevision | SHA-256 | Pak lump (40) bytes |
| --- | ---: | ---: | --- | ---: |
| `maps/gm_construct.bsp` | 36735656 | 1765 | `4d1c027f2b93fa9100bcc4cb08b1dfdda437cadf4cf7d01d4b515dc2caa5a6a2` | 3784681 |
| `maps/gm_flatgrass.bsp` | 47430424 | 146 | `4dfd95ecb8f77a093e3079697b04c5be5675e8595c05639aaf57ad1541024d76` | 41601660 |
| `maps/xr_infmap_passthrough.bsp` | 5787295 | 19 | `81b248bf89111cfbc3f91877dcd90f0caf5a75df17306685c49043ff31a6f998` | 4902236 |
| `addons/cube_ws_2905327911/maps/gm_infmap.bsp` | 5787295 | 19 | `81b248bf89111cfbc3f91877dcd90f0caf5a75df17306685c49043ff31a6f998` | 4902236 |
| `addons/cube_ws_2049617805/maps/gm_novenka.bsp` | 64910184 | 826 | `2a593e76a4af74e9181c9153fb6b4655b355e124cc18abd12e0579e764cb8b6c` | 7213 |
| `addons/cube_ws_3801628931/maps/gm_postal2_suburbs.bsp` | 591274823 | 169 | `352c5fbfc7d72a17f65b10445495827f0ea9114a7708d2683e5e16c8822b2249` | 569260499 |
| `addons/cube_ws_2928366506/maps/gm_infmap_vicecity.bsp` | 1156288 | 7 | `e338de93657b312ecef1b4ef5597423f6b52d9299b6553886843c08f8fb94782` | 181302 |

Ident on every file is `VBSP`, version 20. `mapRevision` matches worldspawn `mapversion`.

`maps/xr_infmap_passthrough.bsp` and `addons/cube_ws_2905327911/maps/gm_infmap.bsp` are the same bytes (same SHA-256). Sections below name them once.

Flatgrass and postal are large because of the pak lump, not because of brushes. Postal’s pak is a zip (EOCD comment `XZP1`) of 8627 entries: models, materials, sounds, and prop `.vhv` files. No central-directory name contains `entity`, `.ent`, `.lua`, `.vmf`, or `spawn`.

## Static props (game lump, not entity lump)

| Map | sprp version | Model dict | `prop_static` count |
| --- | ---: | ---: | ---: |
| gm_construct | 6 | 17 | 182 |
| gm_flatgrass | 6 | 1 | 1 |
| xr_infmap_passthrough / gm_infmap | 6 | 0 | 0 |
| gm_novenka | 10 | 48 | 433 |
| gm_postal2_suburbs | 10 | 584 | 2915 |
| gm_infmap_vicecity | 10 | 0 | 0 |

## gm_construct

`/home/voldemar/.local/share/Steam/steamapps/common/GarrysMod/garrysmod/maps/gm_construct.bsp`

Worldspawn: mins `-5640 -4576 -960`, maxs `2168 6464 2976`, `skyname` `painted`.

`info_player_start`: **33**. First: origin `704 132 -143`, angles `0 180 0`. All 33 use angles `0 180 0` and z `-143`. Spawn AABB: x 695.387..968.613, y -800.687..721.687, z -143. Some origins are fractional. Keys on these ents: `origin`, `angles`, `classname`, `hammerid` (no `spawnflags`).

| Lump | Idx | Offset | Bytes | Ver | Count |
| --- | ---: | ---: | ---: | ---: | --- |
| entities | 0 | 29483976 | 196915 | 0 | 1227 blocks |
| planes | 1 | 1036 | 86440 | 0 | 4322 |
| brushes | 18 | 4215964 | 26436 | 0 | 2203 |
| brushsides | 19 | 4242400 | 119568 | 0 | 14946 |
| faces | 7 | 2846548 | 529760 | 1 | 9460 |
| dispinfo | 26 | 2028124 | 19360 | 0 | 110 |
| disp_verts | 33 | 2047484 | 252760 | 0 | 12638 |
| vertexes | 3 | 1623912 | 168948 | 0 | 14079 × 12 |
| edges | 12 | 4657540 | 159648 | 0 | 39912 × 4 |
| surfedges | 13 | 4395048 | 262492 | 0 | 65623 × 4 |

Disp powers: 2×4, 3×87, 4×19. Those expand to 12638 verts.

Entity blocks: 1227. Classnames: 35. Top 25 (sum 1217):

| Count | Classname |
| ---: | --- |
| 815 | info_node |
| 108 | light |
| 76 | info_node_air_hint |
| 33 | info_player_start |
| 28 | light_spot |
| 24 | info_node_hint |
| 19 | infodecal |
| 19 | env_soundscape |
| 16 | info_hint |
| 15 | info_node_climb |
| 14 | path_track |
| 8 | path_corner |
| 8 | info_node_air |
| 6 | func_brush |
| 5 | point_spotlight |
| 5 | prop_dynamic |
| 4 | info_ladder_dismount |
| 4 | prop_physics |
| 2 | lua_run |
| 2 | func_vehicleclip |
| 2 | func_illusionary |
| 1 | worldspawn |
| 1 | func_useableladder |
| 1 | func_reflective_glass |
| 1 | func_button |

The other 10 classnames are one each (sum 1227): `logic_case`, `water_lod_control`, `shadow_control`, `sky_camera`, `light_environment`, `env_fog_controller`, `env_sun`, `env_skypaint`, `env_tonemap_controller`, `logic_auto`.

One `lua_run` stores a `Code` string that itself contains `{` and `}`. A split that ignores quotes reports 1228 brace pairs, drops that entity’s classname, and counts `lua_run` as 1. The quote-aware walk is the one in the table (`lua_run` = 2).

## gm_flatgrass

`/home/voldemar/.local/share/Steam/steamapps/common/GarrysMod/garrysmod/maps/gm_flatgrass.bsp`

Worldspawn: mins `-15360 -15360 -12800`, maxs `15360 15360 -12288`, `skyname` `painted`. The whole brush AABB sits at large negative z. Spawn points reach z `-12271`, 17 units above `world_maxs` z, which is consistent with displacement verts sticking out of the brush bounds (16 power-4 disps).

`info_player_start`: **56**. First: origin `-512 576 -12287`, angles `0 0 0`. All 56 use angles `0 0 0`. Spawn AABB: x -704..640, y -576..576, z -12287..-12271. Keys: `origin`, `angles`, `classname`, `hammerid`.

| Lump | Idx | Offset | Bytes | Ver | Count |
| --- | ---: | ---: | ---: | ---: | --- |
| entities | 0 | 4784852 | 24727 | 0 | 217 blocks |
| planes | 1 | 1036 | 25720 | 0 | 1286 |
| brushes | 18 | 1749688 | 1344 | 0 | 112 |
| brushsides | 19 | 1751032 | 11352 | 0 | 1419 |
| faces | 7 | 1397932 | 163632 | 1 | 2922 |
| dispinfo | 26 | 1267820 | 2816 | 0 | 16 |
| disp_verts | 33 | 1270636 | 92480 | 0 | 4624 |
| vertexes | 3 | 1104340 | 44856 | 0 | 3738 × 12 |
| edges | 12 | 1829856 | 28972 | 0 | 7243 × 4 |
| surfedges | 13 | 1774140 | 55716 | 0 | 13929 × 4 |

All 16 disps are power 4 (4624 verts). Classnames: 13. Full histogram:

| Count | Classname |
| ---: | --- |
| 150 | info_node |
| 56 | info_player_start |
| 1 | worldspawn |
| 1 | sky_camera |
| 1 | env_tonemap_controller |
| 1 | logic_auto |
| 1 | light_environment |
| 1 | infodecal |
| 1 | env_skypaint |
| 1 | env_fog_controller |
| 1 | shadow_control |
| 1 | env_sun |
| 1 | light |

## xr_infmap_passthrough and gm_infmap

Same file twice:

- `/home/voldemar/.local/share/Steam/steamapps/common/GarrysMod/garrysmod/maps/xr_infmap_passthrough.bsp`
- `/home/voldemar/.local/share/Steam/steamapps/common/GarrysMod/garrysmod/addons/cube_ws_2905327911/maps/gm_infmap.bsp`

Worldspawn mins and maxs are all `-2147483648` (signed 32-bit minimum). That is an unset-bounds sentinel, not an AABB. `skyname` is `painted`.

`info_player_start`: **1**. Origin `0 0 0`, angles `0 0 0`, `spawnflags` `0`.

| Lump | Idx | Offset | Bytes | Ver | Count |
| --- | ---: | ---: | ---: | ---: | --- |
| entities | 0 | 855270 | 2384 | 0 | 9 blocks |
| planes | 1 | 1036 | 3680 | 0 | 184 |
| brushes | 18 | 522840 | 84 | 0 | 7 |
| brushsides | 19 | 522924 | 336 | 0 | 42 |
| faces | 7 | 395612 | 122192 | 1 | 2182 |
| dispinfo | 26 | 391248 | 0 | 0 | 0 |
| disp_verts | 33 | 391248 | 0 | 0 | 0 |
| vertexes | 3 | 258500 | 26532 | 0 | 2211 × 12 |
| edges | 12 | 567082 | 17604 | 0 | 4401 × 4 |
| surfedges | 13 | 531978 | 35104 | 0 | 8776 × 4 |

Nine entities, one each: `worldspawn`, `env_sun`, `light_environment`, `env_skypaint`, `shadow_control`, `env_fog_controller`, `info_player_start`, `logic_auto`, `func_brush`. No displacements and no static props. The pak is two `.vtf` files.

## gm_novenka

`/home/voldemar/.local/share/Steam/steamapps/common/GarrysMod/garrysmod/addons/cube_ws_2049617805/maps/gm_novenka.bsp`

Worldspawn: mins `-15360 -15360 -2320`, maxs `15360 15360 336`, `skyname` `painted`, `targetname` `gm_julvicstruct` (internal name is not the filename).

`info_player_start`: **32**. First: origin `1920 7680 -376`, angles `0 180 0`, `spawnflags` `0`. All 32 use angles `0 180 0` and z `-376`. Spawn AABB: x 1472..1920, y 7296..7680, z -376.

| Lump | Idx | Offset | Bytes | Ver | Count |
| --- | ---: | ---: | ---: | ---: | --- |
| entities | 0 | 61091276 | 706387 | 0 | 4821 blocks |
| planes | 1 | 1036 | 573320 | 0 | 28666 |
| brushes | 18 | 8617496 | 84000 | 0 | 7000 |
| brushsides | 19 | 8701496 | 442528 | 0 | 55316 |
| faces | 7 | 5941432 | 1663424 | 1 | 29704 |
| dispinfo | 26 | 4307272 | 44176 | 0 | 251 |
| disp_verts | 33 | 4351448 | 292380 | 0 | 14619 |
| vertexes | 3 | 2910636 | 555228 | 0 | 46269 × 12 |
| edges | 12 | 10088164 | 518316 | 0 | 129579 × 4 |
| surfedges | 13 | 9248292 | 839872 | 0 | 209968 × 4 |

Disp powers: 2×102, 3×149 (14619 verts). Classnames: 41. Top 25 (sum 4802):

| Count | Classname |
| ---: | --- |
| 3923 | info_node |
| 374 | info_node_air_hint |
| 114 | point_spotlight |
| 95 | light |
| 73 | func_brush |
| 63 | light_spot |
| 32 | info_player_start |
| 17 | env_sprite |
| 13 | env_laser |
| 12 | env_beam |
| 10 | info_target |
| 9 | ambient_generic |
| 7 | infodecal |
| 7 | func_dustmotes |
| 7 | prop_door_rotating |
| 6 | func_illusionary |
| 6 | func_button |
| 6 | prop_dynamic |
| 6 | env_spark |
| 4 | func_door |
| 4 | info_teleport_destination |
| 4 | trigger_teleport |
| 4 | env_citadel_energy_core |
| 4 | logic_relay |
| 2 | trigger_physics_trap |

Remainder (sum 19, total 4821): `env_shake` 2, `env_effectscript` 2, `func_smokevolume` 2, and one each of `worldspawn`, `sky_camera`, `light_environment`, `env_sun`, `shadow_control`, `info_ladder`, `water_lod_control`, `env_skypaint`, `env_lightglow`, `trigger_hurt`, `func_door_rotating`, `trigger_vphysics_motion`, `env_tonemap_controller`.

## gm_postal2_suburbs

`/home/voldemar/.local/share/Steam/steamapps/common/GarrysMod/garrysmod/addons/cube_ws_3801628931/maps/gm_postal2_suburbs.bsp`

The entity lump is 433 bytes and one block. Classname histogram:

| Count | Classname |
| ---: | --- |
| 1 | worldspawn |

`info_player_start`: **0**. There is no first spawn.

That is the whole lump, not a truncated read (one `{` `}` pair, one trailing NUL, no other classnames). Worldspawn keys:

| Key | Value |
| --- | --- |
| bsp_compiler | VBSP++ Sep 11 2026 |
| vis_compiler | VVIS++ Sep 11 2026 |
| rad_compiler | VRAD++ Sep 11 2026 |
| world_mins | -6645 -6552 -2322 |
| world_maxs | 7783 5760 429 |
| skyname | postal2_suburbs_dusk |
| message | Postal 2 Suburbs Created By MasterKills  STEAM_0:0:426408912 |
| mapversion | 169 |

Also the usual `detailvbsp`, `detailmaterial`, `maxpropscreenwidth`, `hammerid`, `classname`.

Placed models are the 2915 static props above, not entity-lump `prop_static` / `prop_physics`. Geometry is real: 3275 brushes, 9032 faces, 959 power-3 displacements (77679 verts).

| Lump | Idx | Offset | Bytes | Ver | Count |
| --- | ---: | ---: | ---: | ---: | --- |
| entities | 0 | 21316548 | 433 | 0 | 1 block |
| planes | 1 | 1036 | 406160 | 0 | 20308 |
| brushes | 18 | 21767560 | 39300 | 0 | 3275 |
| brushsides | 19 | 21806860 | 207464 | 0 | 25933 |
| faces | 7 | 9980456 | 505792 | 1 | 9032 |
| dispinfo | 26 | 2474312 | 168784 | 0 | 959 |
| disp_verts | 33 | 2643096 | 1553580 | 0 | 77679 |
| vertexes | 3 | 1820520 | 164664 | 0 | 13722 × 12 |
| edges | 12 | 10952776 | 95572 | 0 | 23893 × 4 |
| surfedges | 13 | 10782216 | 170560 | 0 | 42640 × 4 |

## gm_infmap_vicecity

`/home/voldemar/.local/share/Steam/steamapps/common/GarrysMod/garrysmod/addons/cube_ws_2928366506/maps/gm_infmap_vicecity.bsp`

Worldspawn mins and maxs are again all `-2147483648`. `skyname` is `sky190`. `comment` is `Decompiled by BSPSource v1.4.1 from gm_infmap_vvicecity_current`. `mapversion` 7.

`info_player_start`: **1**. Origin `0 0 0`, angles `0 0 0`, `spawnflags` `0`.

| Lump | Idx | Offset | Bytes | Ver | Count |
| --- | ---: | ---: | ---: | ---: | --- |
| entities | 0 | 855300 | 92271 | 0 | 294 blocks |
| planes | 1 | 1036 | 3680 | 0 | 184 |
| brushes | 18 | 522868 | 84 | 0 | 7 |
| brushsides | 19 | 522952 | 336 | 0 | 42 |
| faces | 7 | 395640 | 122192 | 1 | 2182 |
| dispinfo | 26 | 391276 | 0 | 0 | 0 |
| disp_verts | 33 | 391276 | 0 | 0 | 0 |
| vertexes | 3 | 258528 | 26532 | 0 | 2211 × 12 |
| edges | 12 | 567112 | 17604 | 0 | 4401 × 4 |
| surfedges | 13 | 532008 | 35104 | 0 | 8776 × 4 |

Same record counts as `xr_infmap_passthrough`. SHA-256 of the lump bytes: edges, surfedges, brushes, and the empty disp lumps match that map. Planes, vertexes, faces, and brushsides are the same length but not the same bytes.

Classnames: 10. Full histogram:

| Count | Classname |
| ---: | --- |
| 285 | prop_scalable |
| 1 | worldspawn |
| 1 | shadow_control |
| 1 | info_player_start |
| 1 | logic_auto |
| 1 | func_brush |
| 1 | env_fog_controller |
| 1 | env_sun |
| 1 | env_skypaint |
| 1 | light_environment |

All 285 `prop_scalable` share origin `0 0 0`, angles `0 -90 0`, `modelscale` `39.3701` (the four-decimal rounding of `100/2.54`, meters to Source units), `skin` `0`. Each `model` is unique, named like `models/vicecitychunks/-1_-1_0.mdl` (grid indices in the model name, not in `origin`). The pak has four `.vtf` files and does not contain those models. There are no static props and no displacements. The city is those scaled chunk models stacked on the infmap shell, not 285 separately placed origins.
