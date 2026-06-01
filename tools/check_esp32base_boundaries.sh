#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "${ROOT_DIR}"

fail() {
  echo "FAIL: $*" >&2
  exit 1
}

require_rg() {
  local pattern="$1"
  local path="$2"
  local message="$3"
  if ! rg -q "${pattern}" "${path}"; then
    fail "${message}"
  fi
}

append_matches="$(rg -n "Esp32BaseAppEventLog::append" apps lib || true)"
if [[ -z "${append_matches}" ]]; then
  fail "FarmAutoEventLog must be the only place that appends Esp32Base App Events"
fi
while IFS= read -r line; do
  [[ -z "${line}" ]] && continue
  if [[ "${line}" != lib/FarmAutoEventLog/src/FarmAutoEventLog.cpp:* ]]; then
    fail "direct Esp32BaseAppEventLog::append outside FarmAutoEventLog: ${line}"
  fi
done <<< "${append_matches}"

if rg -q "Esp32BaseAppEventLog" apps; then
  rg -n "Esp32BaseAppEventLog" apps >&2
  fail "application code must use FarmAutoEventLog instead of Esp32BaseAppEventLog directly"
fi

if rg -q "ESP32BASE_APP_EVENT_LOG_CAPACITY" apps lib; then
  rg -n "ESP32BASE_APP_EVENT_LOG_CAPACITY" apps lib >&2
  fail "FarmAuto must use Esp32Base default App Events capacity"
fi

require_rg "ESP32BASE_ENABLE_APP_EVENTS=1" apps/Esp32FarmDoor/platformio.ini \
  "Esp32FarmDoor must enable Esp32Base App Events"
require_rg "ESP32BASE_ENABLE_APP_EVENTS=1" apps/Esp32FarmFeeder/platformio.ini \
  "Esp32FarmFeeder must enable Esp32Base App Events"

if rg -q "sendFarmAutoBusinessStyle|setHeadExtraCallback" apps; then
  rg -n "sendFarmAutoBusinessStyle|setHeadExtraCallback" apps >&2
  fail "business CSS must not be copied into app pages; use Esp32Base UI baseline"
fi

require_rg "setHomePath\\(\"/index\"\\)" apps/Esp32FarmDoor/src/FarmDoorApp.cpp \
  "Esp32FarmDoor home path must be /index"
require_rg "setHomePath\\(\"/index\"\\)" apps/Esp32FarmFeeder/src/FarmFeederApp.cpp \
  "Esp32FarmFeeder home path must be /index"

require_rg "checkPostAllowed\\(\"farmdoor\"\\)" apps/Esp32FarmDoor/src/FarmDoorApp.cpp \
  "Esp32FarmDoor POST handlers must pass through Esp32BaseWeb::checkPostAllowed"
require_rg "checkPostAllowed\\(\"farmfeeder\"\\)" apps/Esp32FarmFeeder/src/FarmFeederApp.cpp \
  "Esp32FarmFeeder POST handlers must pass through Esp32BaseWeb::checkPostAllowed"

if rg -q "PAGES=\\(/app" tools; then
  rg -n "PAGES=\\(/app" tools >&2
  fail "smoke tests must use /index as the business home page"
fi

echo "Esp32Base boundary checks passed."
