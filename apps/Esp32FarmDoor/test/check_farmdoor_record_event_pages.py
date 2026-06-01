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
require(cpp,
        'Esp32BaseWeb::addNavItem("/records", "开关门记录");',
        "records nav label must be 开关门记录")
require(cpp,
        'Esp32BaseWeb::addNavItem("/events", "业务事件");',
        "events nav item must be registered")
require(cpp,
        'Esp32BaseWeb::addPage("/events", "业务事件", FarmDoorApp::sendEventsPage);',
        "events page must be registered")
require(cpp, "void FarmDoorApp::sendEventsPage()", "sendEventsPage() must be implemented")
require(cpp,
        'Esp32BaseWeb::sendPageTitle("开关门记录"',
        "records page title must be 开关门记录")
require(cpp, 'Esp32BaseWeb::sendPageTitle("业务事件"', "events page title must be 业务事件")
require(cpp,
        "Esp32BaseWeb::sendPagination(recordPagination);",
        "records page must use Esp32Base pagination")
require(cpp,
        "Esp32BaseWeb::sendPagination(eventPagination);",
        "events page must use Esp32Base pagination")
require(cpp,
        'strftime(out, outSize, "%m-%d %H:%M:%S"',
        "record and event time must display as MM-DD HH:MM:SS when real time is available")
require(cpp, "recordTypeDisplayName(", "records page must translate record types to business language")
require(cpp, "commandDisplayName(", "records page must translate commands to business language")
require(cpp, "recordResultDisplayName(", "records page must translate results to business language")
require(cpp, "eventLevelDisplayName(", "events page must translate event levels to business language")

records_start = cpp.find("void FarmDoorApp::sendRecordsPage()")
events_start = cpp.find("void FarmDoorApp::sendEventsPage()")
calibration_start = cpp.find("void FarmDoorApp::sendCalibrationPage()")
if records_start == -1 or events_start == -1 or calibration_start == -1:
    errors.append("records/events/calibration page functions must be present")
else:
    records_body = cpp[records_start:events_start]
    events_body = cpp[events_start:calibration_start]
    if "beginPanel(\"筛选\")" in records_body or "eventType" in records_body:
        errors.append("records page must not expose filters")
    if "startUnixTime" in records_body or "endUnixTime" in records_body:
        errors.append("records page must not expose time filters")
    if "archive" in records_body:
        errors.append("records page must not expose archive selector")
    if "/api/app/events/recent" in records_body:
        errors.append("records page must not link users to business event JSON")
    if "查询 JSON" in records_body or "查询 JSON" in events_body:
        errors.append("HTML list pages must not present JSON query buttons")
    if "<table" not in records_body or "<table" not in events_body:
        errors.append("both pages must render HTML tables")
    for technical_text in ("Unix ", "DoorCommandRequested", "DoorTravelSet", "DoorTravelAdjusted"):
        if technical_text in records_body:
            errors.append(f"records page must not display technical text: {technical_text}")
    for technical_text in ("appEventLevelName(event.level)",
                           "writeHtmlEscaped(event.domain)",
                           "writeHtmlEscaped(event.action)",
                           "writeHtmlEscaped(event.target)"):
        if technical_text in events_body:
            errors.append(f"events page must not display raw event field directly: {technical_text}")

if errors:
    for error in errors:
        print(error)
    raise SystemExit(1)

print("FarmDoor record/event page checks passed")
