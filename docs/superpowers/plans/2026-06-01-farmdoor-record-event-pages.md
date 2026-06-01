# FarmDoor Record Event Pages Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Split FarmDoor records and business events into two human-readable paginated navigation pages.

**Architecture:** Keep existing persistence and JSON APIs unchanged. Add HTML list rendering in `FarmDoorApp.cpp`, using `DoorRecordFileStore` for door records and `FarmAutoEventLog::readLatest()` for business events, with `Esp32BaseWeb::sendPagination()` for both pages.

**Tech Stack:** PlatformIO, Arduino ESP32, Esp32Base Web helpers, C++17-style embedded C++.

---

### Task 1: Add A Web Page Structure Check

**Files:**
- Create: `apps/Esp32FarmDoor/test/check_farmdoor_record_event_pages.py`
- Inspect: `apps/Esp32FarmDoor/src/FarmDoorApp.cpp`
- Inspect: `apps/Esp32FarmDoor/src/FarmDoorApp.h`

- [ ] **Step 1: Write the failing check**

Create `apps/Esp32FarmDoor/test/check_farmdoor_record_event_pages.py` with checks for:

```python
from pathlib import Path

ROOT = Path(__file__).resolve().parents[3]
CPP = ROOT / "apps" / "Esp32FarmDoor" / "src" / "FarmDoorApp.cpp"
HEADER = ROOT / "apps" / "Esp32FarmDoor" / "src" / "FarmDoorApp.h"

cpp = CPP.read_text(encoding="utf-8")
header = HEADER.read_text(encoding="utf-8")
errors = []

def require(text, needle, message):
    if needle not in text:
        errors.append(message)

require(header, "static void sendEventsPage();", "FarmDoorApp.h must declare sendEventsPage()")
require(cpp, 'Esp32BaseWeb::addNavItem("/records", "开关门记录");', "records nav label must be 开关门记录")
require(cpp, 'Esp32BaseWeb::addNavItem("/events", "业务事件");', "events nav item must be registered")
require(cpp, 'Esp32BaseWeb::addPage("/events", "业务事件", FarmDoorApp::sendEventsPage);', "events page must be registered")
require(cpp, "void FarmDoorApp::sendEventsPage()", "sendEventsPage() must be implemented")
require(cpp, 'Esp32BaseWeb::sendPageTitle("开关门记录"', "records page title must be 开关门记录")
require(cpp, 'Esp32BaseWeb::sendPageTitle("业务事件"', "events page title must be 业务事件")
require(cpp, 'Esp32BaseWeb::sendPagination(recordPagination);', "records page must use Esp32Base pagination")
require(cpp, 'Esp32BaseWeb::sendPagination(eventPagination);', "events page must use Esp32Base pagination")

records_start = cpp.find("void FarmDoorApp::sendRecordsPage()")
events_start = cpp.find("void FarmDoorApp::sendEventsPage()")
calibration_start = cpp.find("void FarmDoorApp::sendCalibrationPage()")
if records_start == -1 or events_start == -1 or calibration_start == -1:
    errors.append("records/events/calibration page functions must be present")
else:
    records_body = cpp[records_start:events_start]
    events_body = cpp[events_start:calibration_start]
    if "/api/app/events/recent" in records_body:
        errors.append("records page must not link users to business event JSON")
    if "查询 JSON" in records_body or "查询 JSON" in events_body:
        errors.append("HTML list pages must not present JSON query buttons")
    if "<table" not in records_body or "<table" not in events_body:
        errors.append("both pages must render HTML tables")

if errors:
    for error in errors:
        print(error)
    raise SystemExit(1)

print("FarmDoor record/event page checks passed")
```

- [ ] **Step 2: Run the check and verify RED**

Run: `python3 apps/Esp32FarmDoor/test/check_farmdoor_record_event_pages.py`

Expected: FAIL because `/events` and HTML pagination are not implemented yet.

### Task 2: Implement Paginated HTML Pages

**Files:**
- Modify: `apps/Esp32FarmDoor/src/FarmDoorApp.h`
- Modify: `apps/Esp32FarmDoor/src/FarmDoorApp.cpp`

- [ ] **Step 1: Add page declaration and route registration**

Update `FarmDoorApp.h` with `static void sendEventsPage();`. Update `configureBusinessShell()` to add the `/events` nav item and page route, and update the static route count from 15 to 16.

- [ ] **Step 2: Add HTML formatting helpers**

Add small helpers in `FarmDoorApp.cpp` for page/per parsing, timestamp display, record position/travel delta display, and business event table rows. Keep helpers local to the file.

- [ ] **Step 3: Replace records page JSON form with table rendering**

Change `sendRecordsPage()` so it parses HTML page parameters, reads the selected `DoorRecordPage`, renders a table, and calls:

```cpp
Esp32BaseWeb::Pagination recordPagination = {"/records", queryString, page, per, totalRecords};
Esp32BaseWeb::sendPagination(recordPagination);
```

- [ ] **Step 4: Add events page table rendering**

Implement `sendEventsPage()` so it parses `page` and `per`, reads `FarmAutoEventLog::readLatest(offset, per, ...)`, renders a table, and calls:

```cpp
Esp32BaseWeb::Pagination eventPagination = {"/events", nullptr, page, per, FarmAutoEventLog::count()};
Esp32BaseWeb::sendPagination(eventPagination);
```

- [ ] **Step 5: Update home quick links**

Update the home page quick links so users can enter “开关门记录” and “业务事件” separately.

### Task 3: Verify And Deliver

**Files:**
- Run checks against `apps/Esp32FarmDoor`

- [ ] **Step 1: Run the page structure check**

Run: `python3 apps/Esp32FarmDoor/test/check_farmdoor_record_event_pages.py`

Expected: `FarmDoor record/event page checks passed`

- [ ] **Step 2: Run FarmDoor tests**

Run: `pio test -d apps/Esp32FarmDoor -e native`

Expected: all FarmDoor host tests pass if the native environment is available.

- [ ] **Step 3: Compile FarmDoor firmware**

Run: `pio run -d apps/Esp32FarmDoor`

Expected: firmware compiles successfully for `esp32e_full`.

- [ ] **Step 4: Commit and push**

Run:

```bash
git status --short
git add docs/superpowers/specs/2026-06-01-farmdoor-record-event-pages-design.md docs/superpowers/plans/2026-06-01-farmdoor-record-event-pages.md apps/Esp32FarmDoor/src/FarmDoorApp.h apps/Esp32FarmDoor/src/FarmDoorApp.cpp apps/Esp32FarmDoor/test/check_farmdoor_record_event_pages.py
git commit -m "feat: split FarmDoor record and event pages"
git push
```
