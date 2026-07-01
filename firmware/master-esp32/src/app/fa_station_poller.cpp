#include "fa_station_poller.h"

#include <Arduino.h>
#include <Esp32Base.h>

namespace {

uint16_t transportErrorCode(FaRs485TransportStatus status) {
    return static_cast<uint16_t>(0x8000u | static_cast<uint8_t>(status));
}

}  // namespace

void FaStationPoller::begin(FaRs485Master* master,
                            FaRs485Transport* transport,
                            FaDeviceRegistry* registry,
                            FaMasterActionRuntime* action_runtime) {
    master_ = master;
    transport_ = transport;
    registry_ = registry;
    action_runtime_ = action_runtime;
    last_poll_ms_ = 0u;
    next_index_ = 0u;
}

void FaStationPoller::handle() {
    if (master_ == nullptr || transport_ == nullptr || registry_ == nullptr ||
        action_runtime_ == nullptr || !registry_->isReady() || !transport_->isReady()) {
        return;
    }
    if (action_runtime_->isBusy()) {
        return;
    }

    const uint32_t now_ms = millis();
    if (last_poll_ms_ != 0u && now_ms - last_poll_ms_ < kDefaultPollIntervalMs) {
        return;
    }
    last_poll_ms_ = now_ms;
    pollOne();
}

void FaStationPoller::pollOne() {
    const uint8_t count = registry_->stationCount();
    if (count == 0u) {
        next_index_ = 0u;
        return;
    }
    if (next_index_ >= count) {
        next_index_ = 0u;
    }

    for (uint8_t attempt = 0u; attempt < count; ++attempt) {
        FaStationRecord station;
        const uint8_t index = next_index_;
        next_index_ = static_cast<uint8_t>((next_index_ + 1u) % count);
        if (!registry_->stationAt(index, station) ||
            station.enabled == 0u ||
            !fa_address_is_normal(station.bus_address)) {
            continue;
        }

        uint8_t request[FA_MAX_FRAME_LEN];
        uint8_t response[FA_MAX_FRAME_LEN];
        size_t request_len = 0u;
        size_t response_len = 0u;
        uint8_t seq = 0u;
        const FaFrameResult frame_result = fa_rs485_master_build_get_status(master_,
                                                                           station.bus_address,
                                                                           request,
                                                                           sizeof(request),
                                                                           &request_len,
                                                                           &seq);
        if (frame_result != FA_FRAME_OK) {
            if (station.online_state != FA_STATION_ONLINE_ERROR ||
                station.last_error != static_cast<uint16_t>(frame_result)) {
                ESP32BASE_LOG_W("farm", "station_poll_error addr=%u stage=build_status error=%u",
                                station.bus_address,
                                static_cast<uint16_t>(frame_result));
            }
            (void)registry_->markStationError(station.bus_address, static_cast<uint16_t>(frame_result));
            return;
        }

        const FaRs485TransportStatus tx_status = transport_->transact(request,
                                                                      request_len,
                                                                      response,
                                                                      sizeof(response),
                                                                      &response_len,
                                                                      0u);
        if (tx_status == FaRs485TransportStatus::TIMEOUT) {
            const uint16_t error_code = transportErrorCode(tx_status);
            if (station.online_state != FA_STATION_ONLINE_OFFLINE || station.last_error != error_code) {
                ESP32BASE_LOG_W("farm", "station_offline addr=%u error=%s",
                                station.bus_address,
                                FaRs485Transport::statusName(tx_status));
            }
            (void)registry_->markStationOffline(station.bus_address, error_code);
            return;
        }
        if (tx_status != FaRs485TransportStatus::OK) {
            const uint16_t error_code = transportErrorCode(tx_status);
            if (station.online_state != FA_STATION_ONLINE_ERROR || station.last_error != error_code) {
                ESP32BASE_LOG_W("farm", "station_poll_error addr=%u stage=transport error=%s",
                                station.bus_address,
                                FaRs485Transport::statusName(tx_status));
            }
            (void)registry_->markStationError(station.bus_address, error_code);
            return;
        }

        FaMasterStatusResponse status;
        const uint8_t parse_status = fa_rs485_master_parse_status(response,
                                                                  response_len,
                                                                  station.bus_address,
                                                                  seq,
                                                                  &status);
        if (parse_status == FA_STATUS_OK) {
            if (station.online_state != FA_STATION_ONLINE_ONLINE || station.last_error != 0u) {
                ESP32BASE_LOG_I("farm", "station_online addr=%u active_action=%lu state=%u fault=%u",
                                station.bus_address,
                                static_cast<unsigned long>(status.active_action_id),
                                status.motor_state,
                                status.common.fault_code);
            }
            (void)registry_->markStationOnline(station.bus_address, FaMasterActionRuntime::nowSeconds());
        } else {
            if (station.online_state != FA_STATION_ONLINE_ERROR || station.last_error != parse_status) {
                ESP32BASE_LOG_W("farm", "station_poll_error addr=%u stage=parse status=%u",
                                station.bus_address,
                                parse_status);
            }
            (void)registry_->markStationError(station.bus_address, parse_status);
        }
        return;
    }

}
