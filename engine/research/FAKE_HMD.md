# Fake / simulated HMD on this machine (WiVRn 26.9)

Date checked: 2026-10-09. No packages installed. No `sudo`. `wivrn.service` was left disabled and inactive. Nothing started here was left running. `~/.config/openxr/1/` is empty again, and `~/.config/openvr/openvrpaths.vrpath` matches its pre-test hash.

WiVRn is not a fake headset. It is an OpenXR runtime that encodes frames for a paired standalone client. This build has no null compositor and no simulated HMD. LÖVR’s keyboard/mouse simulator is the path that actually draws eye views on the desktop, and it is not installed. It is also not an OpenXR runtime, so it will not feed gVRMod.

## 1. `openxr_runtime_list_json`

Packages: `wivrn-server 26.9-1`, `wivrn-dashboard 26.9-1`, `openxr 1.1.60-2.1` (`libopenxr_loader.so.1.1.60`). `wivrn-server --version` prints `WiVRn version 26.9`.

No active runtime is selected. `XR_RUNTIME_JSON` is unset in the login environment. Both tools fail the same way:

```text
$ openxr_runtime_list_json
Error [GENERAL | xrCreateInstance | OpenXR-Loader] : RuntimeManifestFile::FindManifestFiles - failed to determine active runtime file path for this environment
Error [GENERAL | xrCreateInstance | OpenXR-Loader] : RuntimeInterface::LoadRuntimes - unknown error
Error [GENERAL | xrCreateInstance | OpenXR-Loader] : RuntimeInterface::LoadRuntimes - failed to load a runtime
Error [GENERAL | xrCreateInstance | OpenXR-Loader] : Failed loading runtime information
Error [GENERAL | xrCreateInstance | OpenXR-Loader] : xrCreateInstance failed
Failed to create XR instance.
```

`openxr_runtime_list` (no `_json`) prints the same loader errors.

Pointing the loader at the WiVRn manifest **without** a server also fails, immediately:

```text
$ XR_RUNTIME_JSON=/usr/share/openxr/1/openxr_wivrn.json openxr_runtime_list_json
ERROR [ipc_client_socket_connect] Failed to connect to socket /run/user/1000/wivrn/comp_ipc: No such file or directory!
ERROR [ipc_client_connection_init] Failed to connect to monado service process
###
# Please make sure that the service process is running
# It is called "monado-service"
# In build trees, it is located "build-dir/src/xrt/targets/service/monado-service"
###
XR_ERROR_RUNTIME_UNAVAILABLE in xrCreateInstance: Failed to create instance '-1'
Error [GENERAL | xrCreateInstance | OpenXR-Loader] : LoaderInstance::CreateInstance chained CreateInstance call failed
Error [GENERAL | xrCreateInstance | OpenXR-Loader] : xrCreateInstance failed
Failed to create XR instance.
```

With `wivrn-server --early-active-runtime` already up, the same command produced **no** stdout/stderr for 8 seconds (`stdbuf -o0 -e0`, timeout exit 124). It does not enumerate views. It blocks in `xrCreateInstance`. See section 3.

The only installed runtime manifest is:

`/usr/share/openxr/1/openxr_wivrn.json`

```json
{
    "file_format_version": "1.0.0",
    "runtime": {
        "name": "WiVRn",
        "library_path": "../../../lib/wivrn/libopenxr_wivrn.so",
        "MND_libmonado_path": "../../../lib/wivrn/libmonado_wivrn.so"
    }
}
```

That relative library path resolves to `/usr/lib/wivrn/libopenxr_wivrn.so` and `/usr/lib/wivrn/libmonado_wivrn.so`. Explicit API layers exist under `/usr/share/openxr/1/api_layers/explicit.d/` (api_dump, core_validation, best_practices). There is no `implicit.d`.

## 2. Active runtime file

Loader search (this OpenXR 1.1.60 loader) is `XR_RUNTIME_JSON`, then `active_runtime.x86_64.json` / `active_runtime.json` under the XDG config dirs. On this login:

| Path | State |
| --- | --- |
| `XR_RUNTIME_JSON` | unset |
| `~/.config/openxr/1/` | directory exists, **no files** (no `active_runtime.json`) |
| `~/.config/kdedefaults/openxr/` | absent (`XDG_CONFIG_DIRS` includes `~/.config/kdedefaults`) |
| `/etc/xdg/openxr/1/active_runtime.json` | absent |
| `/etc/openxr/1/active_runtime.json` | absent |
| `~/.local/share/openxr/1/active_runtime.json` | absent |

`XDG_CONFIG_HOME` is unset, so the user file would be `~/.config/openxr/1/active_runtime.json`. `systemctl --user is-enabled wivrn.service` is `disabled`; `is-active` is `inactive`. The unit’s `ExecStart` is plain `/usr/bin/wivrn-server` (no `--early-active-runtime`).

There is a separate OpenVR file, `~/.config/openvr/openvrpaths.vrpath`. It is not the OpenXR active runtime. Idle WiVRn does not read it. `--early-active-runtime` rewrites it for the process lifetime (section 3) and restores it on clean exit.

## 3. `wivrn-server --early-active-runtime` with no headset

Command actually run (Avahi left off so a paired headset would not be invited):

```bash
wivrn-server --early-active-runtime --no-fork --no-manage-active-runtime --no-publish-service
```

A second run dropped `--no-manage-active-runtime`. Same logs. Session around the test: `XDG_SESSION_TYPE=wayland`, `DISPLAY=:0`, `WAYLAND_DISPLAY=wayland-0`. `Xvfb` and `ffmpeg` are installed; they were not used.

The process stayed up (first run ~29 s, still `S<sl`, one pid) until SIGTERM. It did **not** exit for lack of a client. Full log:

```text
Setting the OpenVR compatibility library to /home/voldemar/Dev/VR/xrizer/target/release
WiVRn 26.9 starting
Terminated
```

No compositor line, no null device, no “Waiting for new connection”, no “Server started”, no encoder line. `ss` showed one listening socket:

```text
u_str LISTEN 0 32 /run/user/1000/wivrn/comp_ipc ... users:(("wivrn-server",pid=...,fd=6))
```

That path is Monado’s IPC socket name (`XRT_IPC_MSG_SOCK_FILENAME=wivrn/comp_ipc`). The parent binds and `listen`s on it at startup so the name is reserved. The Monado service (`ipc_server_main_common`) is not started until `headset_connected_success` calls `start_server`. With `--no-fork`, that call would run the service **in this process** and then `exit` it; that did not happen. So the socket accepts connections and never speaks IPC. That is why `openxr_runtime_list_json` hangs instead of returning `XR_ERROR_RUNTIME_UNAVAILABLE`.

While the process was up, `--early-active-runtime` did change the user’s runtime files:

- `~/.config/openxr/1/active_runtime.json` → symlink to `/usr/share/openxr/1/openxr_wivrn.json`
- `~/.config/openvr/openvrpaths.vrpath` replaced by `{"runtime":["/home/voldemar/Dev/VR/xrizer/target/release"],"version":1}`, with the previous file kept as `openvrpaths.vrpath.wivrn-backup`

That OpenVR path comes from `~/.config/wivrn/config.json` key `openvr-compat-path`. The temporary file dropped the Steam `config` and `log` entries that the real file has.

SIGTERM (not SIGKILL) ran the destructor. After exit:

- symlink gone, `~/.config/openxr/1/` empty again
- `openvrpaths.vrpath.wivrn-backup` gone
- `openvrpaths.vrpath` sha256 back to `6782e83bc7ab304fb4757a6ebd831ea26fb890bd2d2f26715da56fbdddd23347`
- `config.json` and `known_keys.json` hashes unchanged
- `/run/user/1000/wivrn/comp_ipc` removed (the empty directory was already there)

Do not SIGKILL this flag. A kill -9 skips the destructor and leaves WiVRn as the active OpenXR runtime and a stripped OpenVR paths file.

Installed 26.9 `main.cpp` (the paru tarball that matches this binary, not the newer checkout at `~/Dev/VR/WiVRn`) does this:

```cpp
if (*early_active_runtime) {
    do_active_runtime = false;
    runtime_setter.emplace();   // set the files immediately
} else {
    do_active_runtime = not *no_active_runtime;
}
```

So `--early-active-runtime` **overrides** `--no-manage-active-runtime` and still installs the active runtime. `--no-manage-active-runtime` alone does not create `runtime_setter` at startup and does not set it when a headset later connects. That second form was not given its own process test; the early-flag run is the one that was observed, and it did create the symlink even with `--no-manage-active-runtime` also passed.

`start_server` (the compositor) runs only after a headset connects. On connect it would also start `application` from the user’s config, which is `["wayvr"]`. That did not happen here.

## 4. Simulated / null / headless flags

`wivrn-server --help` on the installed binary:

```text
wivrn-server [OPTIONS]

OPTIONS:
  -h,     --help              Print this help message and exit
  -f FILE                     configuration file
          --version           print version and exit
          --no-manage-active-runtime
                              don't set the active runtime on connection
          --early-active-runtime
                              forcibly manages the active runtime even if no headset present
          --no-publish-service
                              disable publishing the service through avahi

Debug:
          --no-fork           disable fork to serve connection
          --no-encrypt        disable encryption
```

No `--null`, `--simulate`, `--headless`, `--fake`, or `--debug-gui`. The string `show the debug GUI` is not in the binary. Upstream 26.9 only compiles `--debug-gui` when `WIVRN_FEATURE_DEBUG_GUI` is on. This package was not built that way.

`debug-gui` still exists as a **config** key. User file `~/.config/wivrn/config.json` has `"debug-gui": false`. The 26.9 docs say the key only works when the server was built with `WIVRN_FEATURE_DEBUG_GUI`, and it enables the Monado debug GUI. That GUI lives in the service process, which does not start until a headset connects. Turning it on would not show eyes with no client, and this binary does not expose the CLI switch.

`wivrnctl` subcommands: `pair`, `unpair`, `rename`, `list-paired`, `stop-server`, `disconnect`, `tab`. No simulate/null/headless subcommand. With the server stopped, `wivrnctl list-paired` prints `read property KnownKeys failed: The name is not activatable` (D-Bus name `io.github.wivrn.Server` is not up).

`/usr/bin` has no `monado-service`, `monado-cli`, or `monado-gui`. Strings in `wivrn-server` include Monado’s “App asked for headless session, creating native compositor anyways” and “Native compositor failed to begin session on warm start; will retry once a client actually connects.” They did not appear in the idle log. There is no `comp_null`, “null compositor”, or “simulated” HMD string in the binary. `XR_MND_headless` is not a way to get a null device out of this runtime: the server still wants its native (encode) compositor, and it only builds that compositor after a client connects.

Nearby binaries that are not a fake HMD:

- `hello_xr` — OpenXR **client** sample (`--graphics`, `--formfactor`, `--viewconfig`). It needs a runtime that can create an instance. It will fail or hang the same way as `openxr_runtime_list_json`.
- `wxrc` — wxWidgets XRC compiler, not a compositor.
- `xr_driver_cli` — XRLinuxDriver. Its user service is not active. It is not Monado and not a simulator.

## 5. What a later harness should run

### WiVRn: OpenXR runtime, no eye images without a client

Safe listen (does not rewrite the active runtime; client must opt in with `XR_RUNTIME_JSON`):

```bash
wivrn-server --no-manage-active-runtime --no-fork --no-publish-service
```

If the test must make WiVRn the active runtime **before** a headset exists, this is the command that did that. It rewrites OpenXR and OpenVR files until a clean SIGTERM:

```bash
wivrn-server --early-active-runtime --no-fork --no-publish-service
```

Do not combine that with a hope that `--no-manage-active-runtime` cancels it. It does not. Stop it with SIGTERM. Do not `systemctl --user enable --now wivrn`.

Client probe, environment only on that process:

```bash
XR_RUNTIME_JSON=/usr/share/openxr/1/openxr_wivrn.json openxr_runtime_list_json
```

| Setup | What happens | Eye images |
| --- | --- | --- |
| No server, no `XR_RUNTIME_JSON` | Loader: no active runtime file | none |
| No server, `XR_RUNTIME_JSON` set | `XR_ERROR_RUNTIME_UNAVAILABLE`, socket missing | none |
| Server up, no headset, with or without `XR_RUNTIME_JSON` | `xrCreateInstance` blocks on `/run/user/1000/wivrn/comp_ipc` | none |
| Headset WiVRn app connects | service starts, VAAPI encode (`encoder: vaapi`, 8-bit in the user config), bitstream to the headset. User config also launches `wayvr` | on the **headset**, not on the desktop |
| `Xvfb` + `ffmpeg` | nothing of WiVRn is presented to X11 or Wayland while idle | none |
| `debug-gui: true` | not available in this build; would still require the service process | not a local fake HMD |

WiVRn cannot present frames without a phone or headset client. The compositor, encoders, and “two views” path are created from the client’s `headset_info` after connect. There is no desktop window of the eyes.

A harness that must not touch OpenVR even temporarily should pass `-f` a private config with `"openvr-compat-path": null` and no `application` (so a surprise client does not launch `wayvr`). `-f` was not executed in this check; it is the documented switch. `--early-active-runtime` would still symlink `active_runtime.json` for the process lifetime.

### LÖVR simulator: two rendered views, not OpenXR

LÖVR is not installed (`lovr` not on `PATH`, no matching Flatpak). Do not treat this as a tested run.

Current docs (0.18, and 0.19 which moves the simulator into `lovr.simulate`) call the driver `simulator`. The 0.15 docs called the same idea `desktop`. Default `conf.lua` tries OpenXR first:

```lua
t.headset.drivers = { 'openxr', 'simulator' }
```

For a fake HMD that must not hang on the WiVRn socket, force the simulator and do not leave `wivrn-server` running:

```lua
function lovr.conf(t)
  t.headset.drivers = { 'simulator' } -- 0.15 name was 'desktop'
  t.headset.start = true
end
```

```bash
lovr /path/to/project
```

`lovr.draw` runs as a stereo headset pass (two views; single-pass stereo when the GPU path is used). Keyboard/mouse pose comes from `lovr.simulate` (WASD, mouse-look while mouse button 1 is held, right button as trigger). The default `lovr.mirror` is `pass:fill(lovr.headset.getTexture())`. `Pass:fill` of a 2-layer texture onto the mono window canvas draws the two layers side by side, which is the usual LÖVR stereo texture. That was not executed here, so treat “both eyes visible in the window” as the documented fill rule, not a local screenshot.

Those pixels exist only inside LÖVR. The simulator does not create `comp_ipc`, does not set `active_runtime.json`, and does not give gVRMod an OpenXR session. `ffmpeg` can grab that window later. It cannot grab WiVRn eyes, because WiVRn never puts them on screen.

### Not available here

`monado-service` is not installed. Stock Monado’s simulated HMD / null compositor is the usual OpenXR fake device, and it is not on this system. WiVRn’s embedded Monado does not include that device. Installing Monado would be a package change and was not done.
