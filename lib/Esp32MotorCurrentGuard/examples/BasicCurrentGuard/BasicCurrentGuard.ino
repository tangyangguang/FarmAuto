#include <Arduino.h>
#include <Esp32MotorCurrentGuard.h>

using namespace Esp32MotorCurrentGuard;

MotorCurrentGuard guard;

void setup() {
  Serial.begin(115200);

  MotorCurrentGuardConfig config;
  config.enabled = true;
  config.warningThresholdMa = 1800;
  config.faultThresholdMa = 2500;
  config.startupGraceMs = 1000;
  config.confirmationSamples = 3;
  config.confirmationMs = 100;

  guard.configure(config);
}

void loop() {
  CurrentSample sample;
  sample.timestampMs = millis();
  sample.currentMa = 500;  // 应用层应替换为 ADC + INA240A2 换算结果。

  guard.update(sample, millis());
  delay(100);
}
