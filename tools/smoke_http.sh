#!/usr/bin/env bash
set -euo pipefail

if [[ $# -lt 2 || $# -gt 4 ]]; then
  echo "Usage: $0 <door|feeder> <base-url> [user] [password]" >&2
  echo "Example: $0 door http://192.168.2.156 admin admin" >&2
  exit 2
fi

APP="$1"
BASE_URL="${2%/}"
USER="${3:-admin}"
PASSWORD="${4:-admin}"
AUTH="${USER}:${PASSWORD}"

if ! command -v curl >/dev/null 2>&1; then
  echo "curl is required" >&2
  exit 2
fi

if ! command -v jq >/dev/null 2>&1; then
  echo "jq is required" >&2
  exit 2
fi

case "${APP}" in
  door)
    PAGES=(/index /records /calibration /diagnostics)
    APIS=(/api/app/status /api/app/diagnostics /api/app/events/recent /api/app/records)
    UNAUTH_API=/api/app/status
    ;;
  feeder)
    PAGES=(/index /schedule /schedule/edit /records /base-info /diagnostics)
    APIS=(/api/app/status /api/app/diagnostics /api/app/events/recent /api/app/schedules /api/app/buckets /api/app/base-info /api/app/feeders/targets /api/app/records)
    UNAUTH_API=/api/app/feeders/targets
    ;;
  *)
    echo "Unknown app '${APP}'. Expected door or feeder." >&2
    exit 2
    ;;
esac

SYSTEM_PAGES=(/esp32base /esp32base/logs /esp32base/app-config /esp32base/tools /esp32base/app-events)

tmp="$(mktemp)"
cleanup() {
  rm -f "${tmp}"
}
trap cleanup EXIT

request() {
  local path="$1"
  local auth_mode="$2"
  local code
  for attempt in 1 2 3; do
    if [[ "${auth_mode}" == "auth" ]]; then
      code="$(curl -sS --connect-timeout 5 --max-time 25 -u "${AUTH}" -o "${tmp}" -w '%{http_code}' "${BASE_URL}${path}")" && {
        printf '%s' "${code}"
        return 0
      }
    else
      code="$(curl -sS --connect-timeout 5 --max-time 25 -o "${tmp}" -w '%{http_code}' "${BASE_URL}${path}")" && {
        printf '%s' "${code}"
        return 0
      }
    fi
    if [[ "${attempt}" != "3" ]]; then
      sleep 2
    fi
  done
  printf '%s' "${code:-000}"
  return 1
}

check_page() {
  local path="$1"
  local code
  code="$(request "${path}" auth || true)"
  if [[ "${code}" != "200" ]]; then
    echo "FAIL page ${path}: HTTP ${code}" >&2
    return 1
  fi
  if ! head -c 16 "${tmp}" | grep -q '<!doctype html>'; then
    echo "FAIL page ${path}: not HTML" >&2
    return 1
  fi
  echo "OK page ${path}"
}

check_api() {
  local path="$1"
  local code
  code="$(request "${path}" auth || true)"
  if [[ "${code}" != "200" ]]; then
    echo "FAIL api ${path}: HTTP ${code}" >&2
    cat "${tmp}" >&2 || true
    return 1
  fi
  if ! jq -e . "${tmp}" >/dev/null; then
    echo "FAIL api ${path}: invalid JSON" >&2
    cat "${tmp}" >&2 || true
    return 1
  fi
  echo "OK api ${path}"
}

check_json_expr() {
  local path="$1"
  local expr="$2"
  local label="$3"
  local code
  code="$(request "${path}" auth || true)"
  if [[ "${code}" != "200" ]]; then
    echo "FAIL ${label} ${path}: HTTP ${code}" >&2
    cat "${tmp}" >&2 || true
    return 1
  fi
  if ! jq -e "${expr}" "${tmp}" >/dev/null; then
    echo "FAIL ${label} ${path}: jq expression failed: ${expr}" >&2
    cat "${tmp}" >&2 || true
    return 1
  fi
  echo "OK ${label} ${path}"
}

check_unauth() {
  local path="$1"
  local code
  code="$(request "${path}" unauth || true)"
  if [[ "${code}" != "401" ]]; then
    echo "FAIL unauth ${path}: expected 401, got ${code}" >&2
    cat "${tmp}" >&2 || true
    return 1
  fi
  echo "OK unauth ${path}"
}

for page in "${SYSTEM_PAGES[@]}"; do
  check_page "${page}"
  sleep 0.3
done

for page in "${PAGES[@]}"; do
  check_page "${page}"
  sleep 0.3
done

for api in "${APIS[@]}"; do
  check_api "${api}"
  sleep 0.3
done

check_json_expr /api/app/status \
  '(.motorOutput.enabled | type) == "boolean" and (.motorOutput.ready | type) == "boolean"' \
  "motorOutput-status"
check_json_expr /api/app/diagnostics \
  '(.motorOutput.enabled | type) == "boolean" and (.motorOutput.ready | type) == "boolean"' \
  "motorOutput-diagnostics"
check_json_expr /api/app/events/recent \
  '.store == "app_events" and .capacity == 1024 and (.events | type) == "array"' \
  "app-events"

check_unauth "${UNAUTH_API}"

echo "Smoke test passed for ${APP} at ${BASE_URL}"
