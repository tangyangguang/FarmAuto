#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

python3 - "${ROOT_DIR}" <<'PY'
from pathlib import Path
import sys

root = Path(sys.argv[1])

apps = [
    {
        "name": "FarmDoor",
        "source": root / "apps/Esp32FarmDoor/src/FarmDoorApp.cpp",
        "class": "FarmDoorApp",
        "pages": [
            "sendHomePage",
            "sendRecordsPage",
            "sendCalibrationPage",
            "sendDiagnosticsPage",
        ],
        "helpers": [],
    },
    {
        "name": "FarmFeeder",
        "source": root / "apps/Esp32FarmFeeder/src/FarmFeederApp.cpp",
        "class": "FarmFeederApp",
        "pages": [
            "sendHomePage",
            "sendSchedulePage",
            "sendScheduleEditPage",
            "sendRecordsPage",
            "sendBaseInfoPage",
            "sendBaseInfoEditPage",
            "sendDiagnosticsPage",
        ],
        "helpers": [
            "sendOccurrenceTable",
        ],
    },
]


def function_body(source: str, signature: str) -> str | None:
    start = source.find(signature)
    if start < 0:
        return None
    brace = source.find("{", start)
    if brace < 0:
        return None
    depth = 0
    for index in range(brace, len(source)):
        char = source[index]
        if char == "{":
            depth += 1
        elif char == "}":
            depth -= 1
            if depth == 0:
                return source[brace + 1:index]
    return None


failures: list[str] = []
for app in apps:
    source = app["source"].read_text(encoding="utf-8")
    for method in app["pages"]:
        body = function_body(source, f"void {app['class']}::{method}()")
        if body is None:
            failures.append(f"{app['name']} {method}: method not found")
            continue
        for helper in ("sendPageTitle", "beginPanel", "endPanel"):
            if f"Esp32BaseWeb::{helper}" not in body:
                failures.append(f"{app['name']} {method}: missing Esp32BaseWeb::{helper}")
        for raw in ('sendChunk("<h1', 'sendChunk("<section', 'sendChunk("</section'):
            if raw in body:
                failures.append(
                    f"{app['name']} {method}: raw page structure {raw} bypasses Esp32Base UI helpers"
                )
        if "<p><form" in body or "</form></p>" in body:
            failures.append(f"{app['name']} {method}: form nested in paragraph creates invalid HTML layout")
        if "class='tablewrap'><table>" in body or 'class="tablewrap"><table>' in body:
            failures.append(f"{app['name']} {method}: tablewrap table must use an Esp32Base table class")

    if "SYSTEM_NAV_BOTTOM" in source:
        failures.append(f"{app['name']}: must use Esp32Base default footerbar navigation layout")

    for helper in app["helpers"]:
        body = function_body(source, f"void {helper}(")
        if body is None:
            failures.append(f"{app['name']} {helper}: helper not found")
            continue
        for required in ("beginPanel", "endPanel"):
            if f"Esp32BaseWeb::{required}" not in body:
                failures.append(f"{app['name']} {helper}: missing Esp32BaseWeb::{required}")
        for raw in ('sendChunk("<section', 'sendChunk("</section'):
            if raw in body:
                failures.append(
                    f"{app['name']} {helper}: raw page structure {raw} bypasses Esp32Base UI helpers"
                )
        if "class='tablewrap'><table>" in body or 'class="tablewrap"><table>' in body:
            failures.append(f"{app['name']} {helper}: tablewrap table must use an Esp32Base table class")

if failures:
    print("FarmAuto Web UI check failed:")
    for failure in failures:
        print(f"- {failure}")
    sys.exit(1)

print("FarmAuto Web UI check passed.")
PY
