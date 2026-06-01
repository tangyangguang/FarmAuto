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
    start_index = cpp.find(start)
    if start_index == -1:
        errors.append(f"missing body: {start}")
        return ""
    if end is None:
        return cpp[start_index:]
    end_index = cpp.find(end, start_index + len(start))
    if end_index == -1 or end_index <= start_index:
        errors.append(f"missing body end: {end}")
        return ""
    return cpp[start_index:end_index]


home = body_between("void FarmDoorApp::sendHomePage()", "void FarmDoorApp::sendRecordsPage()")
calibration = body_between("void FarmDoorApp::sendCalibrationPage()",
                           "void FarmDoorApp::sendDiagnosticsPage()")
diagnostics = body_between("void FarmDoorApp::sendDiagnosticsPage()",
                           "void FarmDoorApp::sendStatusJson()")
set_position = body_between("void FarmDoorApp::handleSetPosition()",
                            "void FarmDoorApp::handleSetTravel()")
set_travel = body_between("void FarmDoorApp::handleSetTravel()",
                          "void FarmDoorApp::handleAdjustTravel()")
adjust_travel = body_between("void FarmDoorApp::handleAdjustTravel()",
                             "void FarmDoorApp::handleClearFault()")
clear_fault = body_between("void FarmDoorApp::handleClearFault()")

require("void sendDoorStatusTable(", "home page must use plain status table helper")
require("void sendDiagnosticsStatusTable(", "diagnostics page must use direct diagnostics table helper")
require("return confirm('把当前位置标为关门基准？')",
        "closed calibration must use browser confirm")
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
