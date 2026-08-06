# Credits

## InfMap – Infinite Map Base

gVRMod Home is a **map-local layout** on top of **InfMap**. The infinite-chunk
engine, detours, terrain entities, materials, and empty BSP foundation are
**not** ours.

| | |
|--|--|
| **Author** | **Meetric** (Mee) |
| **GitHub** | https://github.com/meetric1/gmod-infinite-map |
| **Workshop** | https://steamcommunity.com/workshop/filedetails/?id=2905327911 |
| **Docs** | https://github.com/meetric1/gmod-infinite-map/blob/main/docs.md |
| **Wiki** | https://github.com/Mee12345/gmod-infinite-map/wiki/Documentation |
| **Example OBJ map** | https://github.com/meetric1/Infinite-Map-OBJ-Example |
| **License** | [GNU GPL v3](https://github.com/meetric1/gmod-infinite-map/blob/main/license.md) |
| **Discord** | https://discord.gg/cmQvg2AHgP |

InfMap is partly inspired by **gm_infiniteflatgrass** (Gravity Hull). Meetric’s
project recreated that idea with a modern chunk/detour API.

### What gVRMod ships

- `maps/gm_infmap_home.bsp` — empty InfMap-style BSP (same role as base `gm_infmap`)
- `lua/infmap/gm_infmap_home/*` — home hub terrain height + JSON layout (adapted
  from InfMap’s `gm_infmap` map-local pattern; see Meetric’s docs)
- Live improve commands (`cube_home_*`) — gVRMod product layer only

### Runtime dependency

Players still need the **InfMap base** addon (Workshop or extract). We do not
repackage Meetric’s core `lua/infmap` / entities; Cube may extract the Workshop
GMA so `+map` works.

---

**Thank you, Meetric**, for releasing InfMap under GPL and documenting how to
build custom infinite maps.
