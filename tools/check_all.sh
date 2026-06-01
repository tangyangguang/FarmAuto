#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

"${ROOT_DIR}/tools/check_esp32base_boundaries.sh"
"${ROOT_DIR}/tools/check_farmdoor_web_ui.sh"
"${ROOT_DIR}/tools/run_host_tests.sh"
pio run -d "${ROOT_DIR}/apps/Esp32FarmDoor"
pio run -d "${ROOT_DIR}/apps/Esp32FarmFeeder"

echo "All FarmAuto checks passed."
