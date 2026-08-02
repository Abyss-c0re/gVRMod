#!/bin/bash
# Thin alias → OpenXR native launcher (kept for old shortcuts).
exec "$(cd "$(dirname "$0")" && pwd)/gvrmod_launcher.sh" "$@"