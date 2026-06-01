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
