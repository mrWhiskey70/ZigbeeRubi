#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
if ! command -v idf.py >/dev/null; then
  echo "Activate ESP-IDF v5.5.2 with its export.sh first." >&2
  exit 1
fi
idf.py -B build-c6 -DIDF_TARGET=esp32c6 build
idf.py -B build-c6 size
bash scripts/check_ota_slot_size.sh build-c6 partitions.csv
python3 tools/write_build_manifest.py build-c6
