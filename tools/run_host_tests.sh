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
  -I"${ROOT_DIR}/lib/Esp32At24cRecordStore/include" \
  "${ROOT_DIR}/lib/Esp32At24cRecordStore/test/test_at24c_i2c_device.cpp" \
  "${ROOT_DIR}/lib/Esp32At24cRecordStore/src/Esp32At24cRecordStore.cpp" \
  -o /tmp/farmauto_test_at24c_i2c_device
/tmp/farmauto_test_at24c_i2c_device

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
  "${ROOT_DIR}/apps/Esp32FarmDoor/test/test_door_recovery_codec.cpp" \
  "${ROOT_DIR}/apps/Esp32FarmDoor/src/DoorRecoveryCodec.cpp" \
  "${ROOT_DIR}/lib/Esp32At24cRecordStore/src/Esp32At24cRecordStore.cpp" \
  -o /tmp/farmauto_test_door_recovery_codec
/tmp/farmauto_test_door_recovery_codec

c++ -std=c++17 \
  -I"${ROOT_DIR}/apps/Esp32FarmDoor/src" \
  "${ROOT_DIR}/apps/Esp32FarmDoor/test/test_door_recovery_apply.cpp" \
  "${ROOT_DIR}/apps/Esp32FarmDoor/src/DoorRecoveryApply.cpp" \
  "${ROOT_DIR}/apps/Esp32FarmDoor/src/DoorController.cpp" \
  -o /tmp/farmauto_test_door_recovery_apply
/tmp/farmauto_test_door_recovery_apply

c++ -std=c++17 \
  -I"${ROOT_DIR}/apps/Esp32FarmDoor/src" \
  -I"${ROOT_DIR}/lib/Esp32At24cRecordStore/include" \
  "${ROOT_DIR}/apps/Esp32FarmDoor/test/test_door_storage_layout.cpp" \
  "${ROOT_DIR}/apps/Esp32FarmDoor/src/DoorStorageLayout.cpp" \
  -o /tmp/farmauto_test_door_storage_layout
/tmp/farmauto_test_door_storage_layout

c++ -std=c++17 \
  -I"${ROOT_DIR}/apps/Esp32FarmDoor/src" \
  -I"${ROOT_DIR}/lib/Esp32At24cRecordStore/include" \
  "${ROOT_DIR}/apps/Esp32FarmDoor/test/test_door_recovery_store.cpp" \
  "${ROOT_DIR}/apps/Esp32FarmDoor/src/DoorRecoveryStore.cpp" \
  "${ROOT_DIR}/apps/Esp32FarmDoor/src/DoorRecoveryCodec.cpp" \
  "${ROOT_DIR}/apps/Esp32FarmDoor/src/DoorStorageLayout.cpp" \
  "${ROOT_DIR}/lib/Esp32At24cRecordStore/src/Esp32At24cRecordStore.cpp" \
  -o /tmp/farmauto_test_door_recovery_store
/tmp/farmauto_test_door_recovery_store

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
  -I"${ROOT_DIR}/lib/Esp32At24cRecordStore/include" \
  "${ROOT_DIR}/apps/Esp32FarmFeeder/test/test_feeder_schedule_codec.cpp" \
  "${ROOT_DIR}/apps/Esp32FarmFeeder/src/FeederScheduleCodec.cpp" \
  "${ROOT_DIR}/lib/Esp32At24cRecordStore/src/Esp32At24cRecordStore.cpp" \
  -o /tmp/farmauto_test_feeder_schedule_codec
/tmp/farmauto_test_feeder_schedule_codec

c++ -std=c++17 \
  -I"${ROOT_DIR}/apps/Esp32FarmFeeder/src" \
  "${ROOT_DIR}/apps/Esp32FarmFeeder/test/test_feeder_today.cpp" \
  "${ROOT_DIR}/apps/Esp32FarmFeeder/src/FeederToday.cpp" \
  -o /tmp/farmauto_test_feeder_today
/tmp/farmauto_test_feeder_today

c++ -std=c++17 \
  -I"${ROOT_DIR}/apps/Esp32FarmFeeder/src" \
  -I"${ROOT_DIR}/lib/Esp32At24cRecordStore/include" \
  "${ROOT_DIR}/apps/Esp32FarmFeeder/test/test_feeder_today_codec.cpp" \
  "${ROOT_DIR}/apps/Esp32FarmFeeder/src/FeederTodayCodec.cpp" \
  "${ROOT_DIR}/lib/Esp32At24cRecordStore/src/Esp32At24cRecordStore.cpp" \
  -o /tmp/farmauto_test_feeder_today_codec
/tmp/farmauto_test_feeder_today_codec

c++ -std=c++17 \
  -I"${ROOT_DIR}/apps/Esp32FarmFeeder/src" \
  "${ROOT_DIR}/apps/Esp32FarmFeeder/test/test_feeder_bucket.cpp" \
  "${ROOT_DIR}/apps/Esp32FarmFeeder/src/FeederBucket.cpp" \
  -o /tmp/farmauto_test_feeder_bucket
/tmp/farmauto_test_feeder_bucket

c++ -std=c++17 \
  -I"${ROOT_DIR}/apps/Esp32FarmFeeder/src" \
  -I"${ROOT_DIR}/lib/Esp32At24cRecordStore/include" \
  "${ROOT_DIR}/apps/Esp32FarmFeeder/test/test_feeder_bucket_codec.cpp" \
  "${ROOT_DIR}/apps/Esp32FarmFeeder/src/FeederBucketCodec.cpp" \
  "${ROOT_DIR}/lib/Esp32At24cRecordStore/src/Esp32At24cRecordStore.cpp" \
  -o /tmp/farmauto_test_feeder_bucket_codec
/tmp/farmauto_test_feeder_bucket_codec

c++ -std=c++17 \
  -I"${ROOT_DIR}/apps/Esp32FarmFeeder/src" \
  -I"${ROOT_DIR}/lib/Esp32At24cRecordStore/include" \
  "${ROOT_DIR}/apps/Esp32FarmFeeder/test/test_feeder_calibration_codec.cpp" \
  "${ROOT_DIR}/apps/Esp32FarmFeeder/src/FeederCalibrationCodec.cpp" \
  "${ROOT_DIR}/lib/Esp32At24cRecordStore/src/Esp32At24cRecordStore.cpp" \
  -o /tmp/farmauto_test_feeder_calibration_codec
/tmp/farmauto_test_feeder_calibration_codec

c++ -std=c++17 \
  -I"${ROOT_DIR}/apps/Esp32FarmFeeder/src" \
  -I"${ROOT_DIR}/lib/Esp32At24cRecordStore/include" \
  "${ROOT_DIR}/apps/Esp32FarmFeeder/test/test_feeder_storage_layout.cpp" \
  "${ROOT_DIR}/apps/Esp32FarmFeeder/src/FeederStorageLayout.cpp" \
  "${ROOT_DIR}/apps/Esp32FarmFeeder/src/FeederBucketCodec.cpp" \
  "${ROOT_DIR}/apps/Esp32FarmFeeder/src/FeederTodayCodec.cpp" \
  "${ROOT_DIR}/lib/Esp32At24cRecordStore/src/Esp32At24cRecordStore.cpp" \
  -o /tmp/farmauto_test_feeder_storage_layout
/tmp/farmauto_test_feeder_storage_layout

c++ -std=c++17 \
  -I"${ROOT_DIR}/apps/Esp32FarmFeeder/src" \
  -I"${ROOT_DIR}/lib/Esp32At24cRecordStore/include" \
  "${ROOT_DIR}/apps/Esp32FarmFeeder/test/test_feeder_persistence_store.cpp" \
  "${ROOT_DIR}/apps/Esp32FarmFeeder/src/FeederPersistenceStore.cpp" \
  "${ROOT_DIR}/apps/Esp32FarmFeeder/src/FeederBucketCodec.cpp" \
  "${ROOT_DIR}/apps/Esp32FarmFeeder/src/FeederCalibrationCodec.cpp" \
  "${ROOT_DIR}/apps/Esp32FarmFeeder/src/FeederScheduleCodec.cpp" \
  "${ROOT_DIR}/apps/Esp32FarmFeeder/src/FeederTargetCodec.cpp" \
  "${ROOT_DIR}/apps/Esp32FarmFeeder/src/FeederTodayCodec.cpp" \
  "${ROOT_DIR}/apps/Esp32FarmFeeder/src/FeederStorageLayout.cpp" \
  "${ROOT_DIR}/lib/Esp32At24cRecordStore/src/Esp32At24cRecordStore.cpp" \
  -o /tmp/farmauto_test_feeder_persistence_store
/tmp/farmauto_test_feeder_persistence_store

c++ -std=c++17 \
  -I"${ROOT_DIR}/apps/Esp32FarmFeeder/src" \
  "${ROOT_DIR}/apps/Esp32FarmFeeder/test/test_feeder_target.cpp" \
  "${ROOT_DIR}/apps/Esp32FarmFeeder/src/FeederTarget.cpp" \
  "${ROOT_DIR}/apps/Esp32FarmFeeder/src/FeederBucket.cpp" \
  -o /tmp/farmauto_test_feeder_target
/tmp/farmauto_test_feeder_target

c++ -std=c++17 \
  -I"${ROOT_DIR}/apps/Esp32FarmFeeder/src" \
  -I"${ROOT_DIR}/lib/Esp32At24cRecordStore/include" \
  "${ROOT_DIR}/apps/Esp32FarmFeeder/test/test_feeder_target_codec.cpp" \
  "${ROOT_DIR}/apps/Esp32FarmFeeder/src/FeederTargetCodec.cpp" \
  "${ROOT_DIR}/lib/Esp32At24cRecordStore/src/Esp32At24cRecordStore.cpp" \
  -o /tmp/farmauto_test_feeder_target_codec
/tmp/farmauto_test_feeder_target_codec

c++ -std=c++17 \
  -I"${ROOT_DIR}/apps/Esp32FarmFeeder/src" \
  "${ROOT_DIR}/apps/Esp32FarmFeeder/test/test_feeder_run_tracker.cpp" \
  "${ROOT_DIR}/apps/Esp32FarmFeeder/src/FeederRunTracker.cpp" \
  -o /tmp/farmauto_test_feeder_run_tracker
/tmp/farmauto_test_feeder_run_tracker

c++ -std=c++17 \
  -I"${ROOT_DIR}/apps/Esp32FarmFeeder/src" \
  "${ROOT_DIR}/apps/Esp32FarmFeeder/test/test_feeder_feed_settlement.cpp" \
  "${ROOT_DIR}/apps/Esp32FarmFeeder/src/FeederFeedSettlement.cpp" \
  "${ROOT_DIR}/apps/Esp32FarmFeeder/src/FeederBucket.cpp" \
  "${ROOT_DIR}/apps/Esp32FarmFeeder/src/FeederToday.cpp" \
  -o /tmp/farmauto_test_feeder_feed_settlement
/tmp/farmauto_test_feeder_feed_settlement

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
