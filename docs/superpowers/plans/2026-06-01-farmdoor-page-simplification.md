# FarmDoor Page Simplification Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Simplify FarmDoor home, calibration, and diagnostics pages for direct user operation.

**Architecture:** Keep all changes in `FarmDoorApp.cpp` and use a static structure check to lock user-facing page behavior. Existing API paths remain, but maintenance handlers stop requiring token confirmation for page-submitted calibration and fault-clear operations.

**Tech Stack:** PlatformIO, Arduino ESP32, Esp32Base Web helpers, Python static check.

---

### Task 1: Add Static Page Simplification Check

**Files:**
- Create: `apps/Esp32FarmDoor/test/check_farmdoor_page_simplification.py`
- Inspect: `apps/Esp32FarmDoor/src/FarmDoorApp.cpp`

- [ ] **Step 1: Write the failing check**

The check verifies these exact outcomes:

```python
from pathlib import Path

ROOT = Path(__file__).resolve().parents[3]
CPP = ROOT / "apps" / "Esp32FarmDoor" / "src" / "FarmDoorApp.cpp"
cpp = CPP.read_text(encoding="utf-8")
errors = []

def require(needle, message):
    if needle not in cpp:
        errors.append(message)

def forbid_in(body, needle, message):
    if needle in body:
        errors.append(message)

def body_between(start, end=None):
    s = cpp.find(start)
    if s == -1:
        errors.append(f"missing body: {start}")
        return ""
    if end is None:
        return cpp[s:]
    e = cpp.find(end, s + len(start))
    if e == -1 or e <= s:
        errors.append(f"missing body end: {end}")
        return ""
    return cpp[s:e]

home = body_between("void FarmDoorApp::sendHomePage()", "void FarmDoorApp::sendRecordsPage()")
calibration = body_between("void FarmDoorApp::sendCalibrationPage()", "void FarmDoorApp::sendDiagnosticsPage()")
diagnostics = body_between("void FarmDoorApp::sendDiagnosticsPage()", "void FarmDoorApp::sendStatusJson()")
set_position = body_between("void FarmDoorApp::handleSetPosition()", "void FarmDoorApp::handleSetTravel()")
set_travel = body_between("void FarmDoorApp::handleSetTravel()", "void FarmDoorApp::handleAdjustTravel()")
adjust_travel = body_between("void FarmDoorApp::handleAdjustTravel()", "void FarmDoorApp::handleClearFault()")
clear_fault = body_between("void FarmDoorApp::handleClearFault()")

require("void sendDoorStatusTable(", "home page must use plain status table helper")
require("void sendDiagnosticsStatusTable(", "diagnostics page must use direct diagnostics table helper")
require("return confirm('把当前位置标为关门基准？')", "closed calibration must use browser confirm")
require("return confirm('清除当前故障？')", "clear fault must use browser confirm")

forbid_in(home, "beginPanel(\"快速入口\")", "home page must not show quick links")
forbid_in(home, "beginPanel(\"门操作\")", "door actions must be merged into status panel")
forbid_in(home, "sendMetric(", "home page must not use metric helper")
forbid_in(home, "sendInfoRowCompact", "home page must not use info row helper")
forbid_in(calibration, "confirmToken", "calibration page must not expose confirmToken")
forbid_in(calibration, "只申请 token", "calibration page must not expose token application flow")
forbid_in(calibration, "sendMetric(", "calibration page must not use metric helper")
forbid_in(diagnostics, "故障处理", "diagnostics page must not contain fault handling")
forbid_in(diagnostics, "诊断 JSON", "diagnostics page must not link diagnostic JSON")
forbid_in(diagnostics, "状态 JSON", "diagnostics page must not link status JSON")
forbid_in(diagnostics, "sendMetric(", "diagnostics page must not use metric helper")

forbid_in(set_position, "requireConfirm(", "set-position API must not require token confirmation")
forbid_in(set_travel, "requireConfirm(", "set-travel API must not require token confirmation")
forbid_in(adjust_travel, "requireConfirm(", "adjust-travel API must not require token confirmation")
forbid_in(clear_fault, "requireConfirm(", "clear-fault API must not require token confirmation")

if errors:
    for error in errors:
        print(error)
    raise SystemExit(1)

print("FarmDoor page simplification checks passed")
```

- [ ] **Step 2: Run the check and verify RED**

Run: `python3 apps/Esp32FarmDoor/test/check_farmdoor_page_simplification.py`

Expected: FAIL on current page structure.

### Task 2: Implement Simplified Pages

**Files:**
- Modify: `apps/Esp32FarmDoor/src/FarmDoorApp.cpp`

- [ ] **Step 1: Add plain table helpers**

Add local helpers for status table rows, home status table, calibration summary, and diagnostics table.

- [ ] **Step 2: Simplify home page**

Remove quick links, merge operation buttons into the status panel, show fault clear only when `snapshot.faultReason != DoorFaultReason::None` or state is `Fault`.

- [ ] **Step 3: Simplify calibration page**

Remove token fields and token notice. Add `confirm()` to dangerous forms.

- [ ] **Step 4: Simplify diagnostics page**

Remove JSON links and fault handling. Render hardware state directly.

- [ ] **Step 5: Remove backend token confirmation for these operations**

Remove `requireConfirm()` calls from set-position, set-travel, adjust-travel, and clear-fault handlers.

### Task 3: Verify And Commit

**Files:**
- Run checks and build.

- [ ] **Step 1: Run page checks**

Run:

```bash
python3 apps/Esp32FarmDoor/test/check_farmdoor_record_event_pages.py
python3 apps/Esp32FarmDoor/test/check_farmdoor_page_simplification.py
```

- [ ] **Step 2: Compile**

Run: `pio run -d apps/Esp32FarmDoor`

- [ ] **Step 3: Commit and push**

Run:

```bash
git add apps/Esp32FarmDoor/src/FarmDoorApp.cpp apps/Esp32FarmDoor/test/check_farmdoor_page_simplification.py docs/superpowers/specs/2026-06-01-farmdoor-page-simplification-design.md docs/superpowers/plans/2026-06-01-farmdoor-page-simplification.md
git commit -m "refactor: simplify FarmDoor pages"
git push
```
