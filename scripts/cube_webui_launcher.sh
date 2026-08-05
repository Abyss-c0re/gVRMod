#!/usr/bin/env bash
# Compat shim — product entry is scripts/CubeUI.sh
exec "$(cd "$(dirname "$0")" && pwd)/CubeUI.sh" "$@"
