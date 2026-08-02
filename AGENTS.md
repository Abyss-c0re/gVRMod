# Agent notes — gVRMod

## Version control (mandatory)

Do **not** wait for the user to ask. After any meaningful code/docs/config change:

1. `git status` / `git diff` on **gVRMod** and **addon/vrmod-x64**
2. Commit with a clear message (what + why)
3. `git push` to `origin` on the current branch
4. Or run `./scripts/sync_repos.sh` (fetch/ff-pull when clean + push if ahead)

Never leave product work only in the working tree. Screenshots/temp go in `.scratch/` (gitignored).

Repos: `Abyss-c0re/gVRMod`, `Abyss-c0re/vrmod-x64`. No force-push / hard-reset of shared history unless the user explicitly asks.

## Cube WebUI native launcher

- Product entry: `scripts/cube_webui_launcher.sh` → `install/native/cube_webui_launcher`
- Source: `native_launcher/`
- Defaults: `native_launcher/cube_webui.conf` (shipped; user overrides in `~/.config/gvrmod/`)
