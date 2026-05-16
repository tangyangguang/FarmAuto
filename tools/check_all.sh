#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

"${ROOT_DIR}/tools/run_host_tests.sh"
pio run -d "${ROOT_DIR}/apps/Esp32FarmDoor"

echo "All FarmAuto checks passed."
