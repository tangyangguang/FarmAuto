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

echo "Host tests passed."
