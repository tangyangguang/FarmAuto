#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

c++ -std=c++17 \
  -I"${ROOT_DIR}/lib/Esp32At24cRecordStore/include" \
  "${ROOT_DIR}/lib/Esp32At24cRecordStore/test/test_record_store.cpp" \
  "${ROOT_DIR}/lib/Esp32At24cRecordStore/src/Esp32At24cRecordStore.cpp" \
  -o /tmp/farmauto_test_record_store
/tmp/farmauto_test_record_store

c++ -std=c++17 \
  -I"${ROOT_DIR}/lib/Esp32EncodedDcMotor/include" \
  "${ROOT_DIR}/lib/Esp32EncodedDcMotor/test/test_encoded_dc_motor.cpp" \
  "${ROOT_DIR}/lib/Esp32EncodedDcMotor/src/Esp32EncodedDcMotor.cpp" \
  -o /tmp/farmauto_test_encoded_dc_motor
/tmp/farmauto_test_encoded_dc_motor

c++ -std=c++17 \
  -I"${ROOT_DIR}/lib/Esp32MotorCurrentGuard/include" \
  "${ROOT_DIR}/lib/Esp32MotorCurrentGuard/test/test_motor_current_guard.cpp" \
  "${ROOT_DIR}/lib/Esp32MotorCurrentGuard/src/Esp32MotorCurrentGuard.cpp" \
  -o /tmp/farmauto_test_motor_current_guard
/tmp/farmauto_test_motor_current_guard

c++ -std=c++17 \
  -I"${ROOT_DIR}/apps/Esp32FarmDoor/src" \
  "${ROOT_DIR}/apps/Esp32FarmDoor/test/test_door_controller.cpp" \
  "${ROOT_DIR}/apps/Esp32FarmDoor/src/DoorController.cpp" \
  -o /tmp/farmauto_test_door_controller
/tmp/farmauto_test_door_controller

c++ -std=c++17 \
  -I"${ROOT_DIR}/apps/Esp32FarmDoor/src" \
  "${ROOT_DIR}/apps/Esp32FarmDoor/test/test_door_record_log.cpp" \
  "${ROOT_DIR}/apps/Esp32FarmDoor/src/DoorRecordLog.cpp" \
  -o /tmp/farmauto_test_door_record_log
/tmp/farmauto_test_door_record_log

c++ -std=c++17 \
  -I"${ROOT_DIR}/apps/Esp32FarmDoor/src" \
  -I"${ROOT_DIR}/lib/Esp32At24cRecordStore/include" \
  "${ROOT_DIR}/apps/Esp32FarmDoor/test/test_door_record_codec.cpp" \
  "${ROOT_DIR}/apps/Esp32FarmDoor/src/DoorRecordCodec.cpp" \
  "${ROOT_DIR}/lib/Esp32At24cRecordStore/src/Esp32At24cRecordStore.cpp" \
  -o /tmp/farmauto_test_door_record_codec
/tmp/farmauto_test_door_record_codec

c++ -std=c++17 \
  -I"${ROOT_DIR}/apps/Esp32FarmDoor/src" \
  -I"${ROOT_DIR}/lib/Esp32At24cRecordStore/include" \
  "${ROOT_DIR}/apps/Esp32FarmDoor/test/test_door_record_file_store.cpp" \
  "${ROOT_DIR}/apps/Esp32FarmDoor/src/DoorRecordFileStore.cpp" \
  "${ROOT_DIR}/apps/Esp32FarmDoor/src/DoorRecordCodec.cpp" \
  "${ROOT_DIR}/lib/Esp32At24cRecordStore/src/Esp32At24cRecordStore.cpp" \
  -o /tmp/farmauto_test_door_record_file_store
/tmp/farmauto_test_door_record_file_store

c++ -std=c++17 \
  -I"${ROOT_DIR}/apps/Esp32FarmFeeder/src" \
  "${ROOT_DIR}/apps/Esp32FarmFeeder/test/test_feeder_controller.cpp" \
  "${ROOT_DIR}/apps/Esp32FarmFeeder/src/FeederController.cpp" \
  -o /tmp/farmauto_test_feeder_controller
/tmp/farmauto_test_feeder_controller

c++ -std=c++17 \
  -I"${ROOT_DIR}/apps/Esp32FarmFeeder/src" \
  "${ROOT_DIR}/apps/Esp32FarmFeeder/test/test_feeder_schedule.cpp" \
  "${ROOT_DIR}/apps/Esp32FarmFeeder/src/FeederSchedule.cpp" \
  -o /tmp/farmauto_test_feeder_schedule
/tmp/farmauto_test_feeder_schedule

c++ -std=c++17 \
  -I"${ROOT_DIR}/apps/Esp32FarmFeeder/src" \
  "${ROOT_DIR}/apps/Esp32FarmFeeder/test/test_feeder_bucket.cpp" \
  "${ROOT_DIR}/apps/Esp32FarmFeeder/src/FeederBucket.cpp" \
  -o /tmp/farmauto_test_feeder_bucket
/tmp/farmauto_test_feeder_bucket

c++ -std=c++17 \
  -I"${ROOT_DIR}/apps/Esp32FarmFeeder/src" \
  "${ROOT_DIR}/apps/Esp32FarmFeeder/test/test_feeder_target.cpp" \
  "${ROOT_DIR}/apps/Esp32FarmFeeder/src/FeederTarget.cpp" \
  "${ROOT_DIR}/apps/Esp32FarmFeeder/src/FeederBucket.cpp" \
  -o /tmp/farmauto_test_feeder_target
/tmp/farmauto_test_feeder_target

c++ -std=c++17 \
  -I"${ROOT_DIR}/apps/Esp32FarmFeeder/src" \
  "${ROOT_DIR}/apps/Esp32FarmFeeder/test/test_feeder_run_tracker.cpp" \
  "${ROOT_DIR}/apps/Esp32FarmFeeder/src/FeederRunTracker.cpp" \
  -o /tmp/farmauto_test_feeder_run_tracker
/tmp/farmauto_test_feeder_run_tracker

c++ -std=c++17 \
  -I"${ROOT_DIR}/apps/Esp32FarmFeeder/src" \
  "${ROOT_DIR}/apps/Esp32FarmFeeder/test/test_feeder_record_log.cpp" \
  "${ROOT_DIR}/apps/Esp32FarmFeeder/src/FeederRecordLog.cpp" \
  -o /tmp/farmauto_test_feeder_record_log
/tmp/farmauto_test_feeder_record_log

c++ -std=c++17 \
  -I"${ROOT_DIR}/apps/Esp32FarmFeeder/src" \
  -I"${ROOT_DIR}/lib/Esp32At24cRecordStore/include" \
  "${ROOT_DIR}/apps/Esp32FarmFeeder/test/test_feeder_record_codec.cpp" \
  "${ROOT_DIR}/apps/Esp32FarmFeeder/src/FeederRecordCodec.cpp" \
  "${ROOT_DIR}/lib/Esp32At24cRecordStore/src/Esp32At24cRecordStore.cpp" \
  -o /tmp/farmauto_test_feeder_record_codec
/tmp/farmauto_test_feeder_record_codec

c++ -std=c++17 \
  -I"${ROOT_DIR}/apps/Esp32FarmFeeder/src" \
  -I"${ROOT_DIR}/lib/Esp32At24cRecordStore/include" \
  "${ROOT_DIR}/apps/Esp32FarmFeeder/test/test_feeder_record_file_store.cpp" \
  "${ROOT_DIR}/apps/Esp32FarmFeeder/src/FeederRecordFileStore.cpp" \
  "${ROOT_DIR}/apps/Esp32FarmFeeder/src/FeederRecordCodec.cpp" \
  "${ROOT_DIR}/lib/Esp32At24cRecordStore/src/Esp32At24cRecordStore.cpp" \
  -o /tmp/farmauto_test_feeder_record_file_store
/tmp/farmauto_test_feeder_record_file_store

echo "Host tests passed."
