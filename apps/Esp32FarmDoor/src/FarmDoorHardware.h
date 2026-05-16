#pragma once

#include <Arduino.h>
#include <cstdint>

struct FarmDoorHardwarePins {
  uint8_t currentAdc = 33;
  uint8_t buttonAux = 36;
  uint8_t buttonOpen = 39;
  uint8_t buttonClose = 34;
  uint8_t buttonStop = 35;
  uint8_t encoderA = 25;
  uint8_t encoderB = 26;
  uint8_t motorIn1 = 16;
  uint8_t motorIn2 = 17;
  uint8_t i2cSda = 21;
  uint8_t i2cScl = 22;
};

struct FarmDoorReadOnlyDiagnostics {
  uint8_t buttonAux = 0;
  uint8_t buttonOpen = 0;
  uint8_t buttonClose = 0;
  uint8_t buttonStop = 0;
  uint8_t encoderA = 0;
  uint8_t encoderB = 0;
  int currentRawAdc = 0;
  int currentRawMin = 0;
  int currentRawMax = 0;
  int currentRawAvg = 0;
  uint8_t currentSampleCount = 0;
  bool at24cOnline = false;
};

class FarmDoorHardware {
 public:
  static constexpr uint8_t AT24C128_I2C_ADDRESS = 0x50;

  void begin(const FarmDoorHardwarePins& pins);
  const FarmDoorHardwarePins& pins() const;
  FarmDoorReadOnlyDiagnostics readDiagnostics();

 private:
  bool i2cDeviceOnline(uint8_t address);
  uint8_t readDigital(uint8_t pin) const;

  FarmDoorHardwarePins pins_;
};

extern FarmDoorHardware FarmDoorHw;
