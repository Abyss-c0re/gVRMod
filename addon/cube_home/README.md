# XR Home Passthrough (`xr_infmap_passthrough`)

VR home hub on **InfMap** by **Meetric** — AR void with **green-key** passthrough (OpenXR).

## Credits

→ **[CREDITS.md](./CREDITS.md)** · https://github.com/meetric1/gmod-infinite-map · Workshop `2905327911`

## Map name

| | |
|--|--|
| BSP | `xr_infmap_passthrough` |
| Why | InfMap requires second token `infmap` (`xr` \| `infmap` \| `passthrough`) |

## Passthrough

- **Only** on this map + **OpenXR** + VR active  
- **Quick menu** toggle appears only then  
- Void key: pure green material `cube_home/pt_void` (not black)

```
cube_home_passthrough 1
cube_home_passthrough_tol 0.22
```

## Install

1. InfMap base (Workshop or Cube extract)  
2. `./install.sh` links `addons/cube_home`  
3. CubeUI defaults to `xr_infmap_passthrough`
