#include "fa_master_action_runtime.h"

#include <Arduino.h>
#include <Esp32Base.h>
#include <string.h>

#include "fa_action_record_store.h"

namespace {

const char* recordStateName(uint8_t state) {
    switch (state) {
    case FA_ACTION_RECORD_RUNNING:
        return "running";
    case FA_ACTION_RECORD_COMPLETED:
        return "completed";
    case FA_ACTION_RECORD_STOPPED:
        return "stopped";
    case FA_ACTION_RECORD_FAILED:
        return "failed";
    default:
        return "unknown";
    }
}

}  // namespace

void FaMasterActionRuntime::begin(FaRs485Master* master, FaRs485Transport* transport) {
    master_ = master;
    transport_ = transport;
    busy_ = false;
    poll_failures_ = 0u;
    last_poll_ms_ = 0u;
    last_error_ = "none";
    memset(&active_, 0, sizeof(active_));
}

void FaMasterActionRuntime::handle() {
    if (!busy_) {
        return;
    }
    const uint32_t now_ms = millis();
    if (last_poll_ms_ != 0u && static_cast<uint32_t>(now_ms - last_poll_ms_) < kDefaultPollIntervalMs) {
        return;
    }
    pollStatus(now_ms);
}

bool FaMasterActionRuntime::trackStartedAction(const FaActionRecordStart& start) {
    if (busy_) {
        last_error_ = "busy";
        return false;
    }
    if (fa_action_record_begin(&active_, &start) != FA_STATUS_OK) {
        last_error_ = "bad_start";
        return false;
    }

    busy_ = true;
    poll_failures_ = 0u;
    last_poll_ms_ = 0u;
    last_error_ = "none";
    ESP32BASE_LOG_I("farm", "action_tracking_started action_id=%lu device_id=%u addr=%u target=%lu",
                    static_cast<unsigned long>(active_.action_id),
                    active_.device_id,
                    active_.bus_address,
                    static_cast<unsigned long>(active_.target_pulses));
    return true;
}

bool FaMasterActionRuntime::isBusy() const {
    return busy_;
}

const FaActionRecord* FaMasterActionRuntime::activeRecord() const {
    return busy_ ? &active_ : nullptr;
}

const char* FaMasterActionRuntime::lastError() const {
    return last_error_;
}

uint32_t FaMasterActionRuntime::nowSeconds() {
    const Esp32BaseTime::Snapshot now = Esp32BaseTime::snapshot();
    return now.synced ? now.epochSec : now.uptimeSec;
}

void FaMasterActionRuntime::pollStatus(uint32_t now_ms) {
    last_poll_ms_ = now_ms;
    if (master_ == nullptr || transport_ == nullptr || !transport_->isReady()) {
        ESP32BASE_LOG_W("farm", "action_poll_failed action_id=%lu reason=transport_unavailable",
                        static_cast<unsigned long>(active_.action_id));
        failActiveRecord(FA_FAULT_COMMUNICATION, FA_STOP_LOCAL_FAULT);
        last_error_ = "transport_unavailable";
        return;
    }

    uint8_t request[FA_MAX_FRAME_LEN];
    uint8_t response[FA_MAX_FRAME_LEN];
    size_t request_len = 0u;
    size_t response_len = 0u;
    uint8_t seq = 0u;
    if (fa_rs485_master_build_get_status(master_, active_.bus_address, request, sizeof(request), &request_len, &seq) != FA_FRAME_OK) {
        ESP32BASE_LOG_W("farm", "action_poll_failed action_id=%lu reason=build_status",
                        static_cast<unsigned long>(active_.action_id));
        failActiveRecord(FA_FAULT_COMMUNICATION, FA_STOP_LOCAL_FAULT);
        last_error_ = "build_status";
        return;
    }

    const FaRs485TransportStatus tx_status = transport_->transact(request, request_len, response, sizeof(response), &response_len, 0u);
    if (tx_status != FaRs485TransportStatus::OK) {
        ++poll_failures_;
        last_error_ = FaRs485Transport::statusName(tx_status);
        ESP32BASE_LOG_W("farm", "action_poll_retry action_id=%lu addr=%u error=%s failures=%u/%u",
                        static_cast<unsigned long>(active_.action_id),
                        active_.bus_address,
                        last_error_,
                        poll_failures_,
                        kMaxPollFailures);
        if (poll_failures_ >= kMaxPollFailures) {
            failActiveRecord(FA_FAULT_COMMUNICATION, FA_STOP_LOCAL_FAULT);
        }
        return;
    }

    FaMasterStatusResponse status;
    const uint8_t parse_status = fa_rs485_master_parse_status(response, response_len, active_.bus_address, seq, &status);
    if (parse_status != FA_STATUS_OK) {
        ++poll_failures_;
        last_error_ = "bad_status";
        ESP32BASE_LOG_W("farm", "action_poll_retry action_id=%lu addr=%u error=bad_status status=%u failures=%u/%u",
                        static_cast<unsigned long>(active_.action_id),
                        active_.bus_address,
                        parse_status,
                        poll_failures_,
                        kMaxPollFailures);
        if (poll_failures_ >= kMaxPollFailures) {
            failActiveRecord(FA_FAULT_COMMUNICATION, FA_STOP_LOCAL_FAULT);
        }
        return;
    }

    poll_failures_ = 0u;
    last_error_ = "none";
    if (fa_action_record_apply_status(&active_, &status, nowSeconds()) != FA_STATUS_OK) {
        ESP32BASE_LOG_W("farm", "action_poll_failed action_id=%lu reason=record_status status_action_id=%lu",
                        static_cast<unsigned long>(active_.action_id),
                        static_cast<unsigned long>(status.active_action_id));
        failActiveRecord(FA_FAULT_COMMUNICATION, FA_STOP_LOCAL_FAULT);
        last_error_ = "record_status";
        return;
    }
    if (fa_action_record_is_terminal(&active_) != 0u) {
        finishActiveRecord();
    }
}

void FaMasterActionRuntime::finishActiveRecord() {
    ESP32BASE_LOG_I("farm", "action_finished action_id=%lu state=%s pulses=%lu/%lu run_ms=%lu stop=%u fault=%u peak_ma=%u",
                    static_cast<unsigned long>(active_.action_id),
                    recordStateName(active_.state),
                    static_cast<unsigned long>(active_.completed_pulses),
                    static_cast<unsigned long>(active_.target_pulses),
                    static_cast<unsigned long>(active_.run_ms),
                    active_.stop_reason,
                    active_.fault_code,
                    active_.peak_current_ma);
    if (!FaActionRecordStore::append(active_)) {
        ESP32BASE_LOG_W("farm", "action_record_append_failed action_id=%lu",
                        static_cast<unsigned long>(active_.action_id));
        last_error_ = "record_append";
    }
    busy_ = false;
}

void FaMasterActionRuntime::failActiveRecord(uint16_t fault_code, uint8_t stop_reason) {
    ESP32BASE_LOG_W("farm", "action_failed action_id=%lu fault=%u stop=%u completed=%lu target=%lu",
                    static_cast<unsigned long>(active_.action_id),
                    fault_code,
                    stop_reason,
                    static_cast<unsigned long>(active_.completed_pulses),
                    static_cast<unsigned long>(active_.target_pulses));
    active_.fault_code = fault_code;
    active_.stop_reason = stop_reason;
    active_.state = FA_ACTION_RECORD_FAILED;
    active_.ended_at_s = nowSeconds();
    finishActiveRecord();
}
