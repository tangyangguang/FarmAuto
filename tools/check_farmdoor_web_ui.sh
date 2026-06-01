#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
APP_SOURCE="${ROOT_DIR}/apps/Esp32FarmDoor/src/FarmDoorApp.cpp"

python3 - "$APP_SOURCE" <<'PY'
import re
import sys

source_path = sys.argv[1]
source = open(source_path, encoding="utf-8").read()

page_methods = [
    "sendHomePage",
    "sendRecordsPage",
    "sendCalibrationPage",
    "sendDiagnosticsPage",
]

failures = []
for method in page_methods:
    match = re.search(
        r"void FarmDoorApp::" + re.escape(method) + r"\(\) \{\n(?P<body>.*?)\n\}",
        source,
        re.S,
    )
    if not match:
        failures.append(f"{method}: method not found")
        continue
    body = match.group("body")
    for helper in ("sendPageTitle", "beginPanel", "endPanel"):
        if f"Esp32BaseWeb::{helper}" not in body:
            failures.append(f"{method}: missing Esp32BaseWeb::{helper}")
    for raw in ('sendChunk("<h1', 'sendChunk("<section', 'sendChunk("</section'):
        if raw in body:
            failures.append(f"{method}: raw page structure {raw} bypasses Esp32Base UI helpers")
    if "<p><form" in body or "</form></p>" in body:
        failures.append(f"{method}: form nested in paragraph creates invalid HTML layout")

if failures:
    print("FarmDoor Web UI check failed:")
    for failure in failures:
        print(f"- {failure}")
    sys.exit(1)

print("FarmDoor Web UI check passed.")
PY
