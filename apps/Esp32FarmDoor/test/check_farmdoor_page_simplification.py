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
require('Esp32BaseWeb::setDeviceName("自动门");', "top-left device name must be 自动门")
require('Esp32BaseAppConfig::addInt({"protection", "door", "maxRunSec"', "max runtime config must use seconds")
require('"最大保护运行时长"', "max runtime label must mention protection")
require('"秒"', "max runtime unit must be seconds")
require("g_doorConfig.maxRunMs = static_cast<uint32_t>(maxRunSec) * 1000UL;",
        "runtime config must convert seconds to milliseconds internally")
require("sendStatusMetric(\"门状态\"", "home status must use split status metrics")
require("sendStatusMetric(\"当前位置\"", "home status must show position as a separate metric")
require("sendStatusMetric(\"AT24C128\"", "diagnostics must use split status metrics")
require("sendCalibrationActionForm(", "calibration forms must use shared action form layout")
require("把当前位置标为关门基准？",
        "closed calibration must use browser confirm")
require("return confirm('清除当前故障？')", "clear fault must use browser confirm")
require("开门会让自动门实际运行，确认执行？",
        "home open action must use browser confirmation")
require("关门会让自动门实际运行，确认执行？",
        "home close action must use browser confirmation")
require("停止会立即中断自动门运行，确认执行？",
        "home stop action must use browser confirmation")
require("redirectAfterAction(",
        "HTML form actions must redirect back to business pages instead of showing JSON")
require("name='returnTo' value='/index'",
        "home action forms must return to the home page after POST")
require("name='returnTo' value='/calibration'",
        "calibration forms must return to the calibration page after POST")

forbid_in(cpp, '"motorOutput"', "motor output enable config must be removed")
forbid_in(cpp, 'addBool({"motor", "door", "motorOutput"', "motor output config field must be removed")
forbid_in(cpp, "g_motorOutputEnabled", "motor output should not be gated by a runtime enable flag")
forbid_in(cpp, "电机输出未启用", "home page must not warn about disabled motor output")
forbid_in(cpp, 'addInt({"protection", "door", "maxRunMs"', "App Config must not expose maxRunMs")
forbid_in(home, "beginPanel(\"快速入口\")", "home page must not show quick links")
forbid_in(home, "beginPanel(\"门操作\")", "door actions must be merged into status panel")
forbid_in(home, "sendMetric(", "home page must not use metric helper")
forbid_in(home, "sendInfoRowCompact", "home page must not use info row helper")
forbid_in(calibration, "confirmToken", "calibration page must not expose confirmToken")
forbid_in(calibration, "只申请 token", "calibration page must not expose token application flow")
forbid_in(calibration, "sendMetric(", "calibration page must not use metric helper")
forbid_in(calibration, "单位为 0.01 圈", "calibration page must not expose internal x100 turn units")
forbid_in(calibration, "openTurnsX100", "calibration page must not use x100 turn input in HTML")
forbid_in(calibration, "deltaTurnsX100", "calibration page must not use signed x100 adjustment input in HTML")
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
