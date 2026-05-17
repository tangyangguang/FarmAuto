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
    PAGES=(/app /records /calibration /diagnostics)
    APIS=(/api/app/status /api/app/diagnostics /api/app/events/recent /api/app/records)
    UNAUTH_API=/api/app/status
    ;;
  feeder)
    PAGES=(/app /schedule /records /base-info /diagnostics)
    APIS=(/api/app/status /api/app/diagnostics /api/app/events/recent /api/app/schedules /api/app/buckets /api/app/base-info /api/app/feeders/targets /api/app/records)
    UNAUTH_API=/api/app/feeders/targets
    ;;
  *)
    echo "Unknown app '${APP}'. Expected door or feeder." >&2
    exit 2
    ;;
esac

tmp="$(mktemp)"
cleanup() {
  rm -f "${tmp}"
}
trap cleanup EXIT

check_page() {
  local path="$1"
  local code
  code="$(curl --http1.0 -sS --max-time 12 -u "${AUTH}" -o "${tmp}" -w '%{http_code}' "${BASE_URL}${path}")"
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
  code="$(curl --http1.0 -sS --max-time 12 -u "${AUTH}" -o "${tmp}" -w '%{http_code}' "${BASE_URL}${path}")"
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

check_unauth() {
  local path="$1"
  local code
  code="$(curl --http1.0 -sS --max-time 12 -o "${tmp}" -w '%{http_code}' "${BASE_URL}${path}")"
  if [[ "${code}" != "401" ]]; then
    echo "FAIL unauth ${path}: expected 401, got ${code}" >&2
    cat "${tmp}" >&2 || true
    return 1
  fi
  echo "OK unauth ${path}"
}

for page in "${PAGES[@]}"; do
  check_page "${page}"
  sleep 0.3
done

for api in "${APIS[@]}"; do
  check_api "${api}"
  sleep 0.3
done

check_unauth "${UNAUTH_API}"

echo "Smoke test passed for ${APP} at ${BASE_URL}"
