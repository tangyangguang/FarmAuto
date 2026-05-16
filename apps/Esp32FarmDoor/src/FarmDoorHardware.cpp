#include "FarmDoorHardware.h"

#include <Wire.h>

FarmDoorHardware FarmDoorHw;

void FarmDoorHardware::begin(const FarmDoorHardwarePins& pins) {
  pins_ = pins;

  pinMode(pins_.buttonAux, INPUT);
  pinMode(pins_.buttonOpen, INPUT);
  pinMode(pins_.buttonClose, INPUT);
  pinMode(pins_.buttonStop, INPUT);
  pinMode(pins_.encoderA, INPUT);
  pinMode(pins_.encoderB, INPUT);

  analogReadResolution(12);
  analogSetPinAttenuation(pins_.currentAdc, ADC_11db);
  Wire.begin(pins_.i2cSda, pins_.i2cScl);
}

const FarmDoorHardwarePins& FarmDoorHardware::pins() const {
  return pins_;
}

FarmDoorReadOnlyDiagnostics FarmDoorHardware::readDiagnostics() {
  FarmDoorReadOnlyDiagnostics diagnostics;
  diagnostics.buttonAux = readDigital(pins_.buttonAux);
  diagnostics.buttonOpen = readDigital(pins_.buttonOpen);
  diagnostics.buttonClose = readDigital(pins_.buttonClose);
  diagnostics.buttonStop = readDigital(pins_.buttonStop);
  diagnostics.encoderA = readDigital(pins_.encoderA);
  diagnostics.encoderB = readDigital(pins_.encoderB);
  diagnostics.currentRawAdc = analogRead(pins_.currentAdc);
  diagnostics.at24cOnline = i2cDeviceOnline(AT24C128_I2C_ADDRESS);
  return diagnostics;
}

bool FarmDoorHardware::i2cDeviceOnline(uint8_t address) {
  Wire.beginTransmission(address);
  return Wire.endTransmission() == 0;
}

uint8_t FarmDoorHardware::readDigital(uint8_t pin) const {
  return digitalRead(pin) == HIGH ? 1 : 0;
}
