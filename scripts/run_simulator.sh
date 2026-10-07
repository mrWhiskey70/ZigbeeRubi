#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
cmake -S host -B build-simulator
cmake --build build-simulator -j2
exec python3 host/server.py "$@"
