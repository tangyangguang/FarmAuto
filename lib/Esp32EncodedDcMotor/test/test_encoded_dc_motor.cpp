#include <cassert>

#include "Esp32EncodedDcMotor.h"

class FakeDriver : public Esp32EncodedDcMotor::IMotorDriver {
public:
  Esp32EncodedDcMotor::MotorResult setOutput(int8_t direction, uint8_t percent) override {
    lastDirection = direction;
    lastPercent = percent;
    return Esp32EncodedDcMotor::MotorResult::Ok;
  }

  Esp32EncodedDcMotor::MotorResult stop(Esp32EncodedDcMotor::EmergencyOutputMode) override {
    lastDirection = 0;
    lastPercent = 0;
    stopCount++;
    return Esp32EncodedDcMotor::MotorResult::Ok;
  }

  int8_t lastDirection = 0;
  uint8_t lastPercent = 0;
  int stopCount = 0;
};

class FakeEncoder : public Esp32EncodedDcMotor::IEncoderReader {
public:
  int64_t positionPulses() const override {
    return position;
  }

  int64_t position = 0;
};

int main() {
  FakeDriver driver;
  FakeEncoder encoder;
  Esp32EncodedDcMotor::EncodedDcMotor motor;

  Esp32EncodedDcMotor::MotorHardwareConfig hardware{};
  Esp32EncodedDcMotor::EncoderBackendConfig encoderConfig{};
  Esp32EncodedDcMotor::MotorKinematics kinematics{};
  kinematics.outputPulsesPerRev = 2096;
  Esp32EncodedDcMotor::MotorMotionProfile profile{};
  profile.speedPercent = 60;
  profile.softStartMs = 1000;
  profile.softStopMs = 500;
  Esp32EncodedDcMotor::MotorProtection protection{};
  protection.maxRunPulses = 5000;
  Esp32EncodedDcMotor::MotorStopPolicy stopPolicy{};

  assert(motor.begin(driver, encoder, hardware, encoderConfig) == Esp32EncodedDcMotor::MotorResult::Ok);
  assert(motor.configure(kinematics, profile, protection, stopPolicy) ==
         Esp32EncodedDcMotor::MotorResult::Ok);
  assert(motor.requestMovePulses(1000) == Esp32EncodedDcMotor::MotorResult::Ok);

  motor.update(0);
  auto snapshot = motor.snapshot();
  assert(snapshot.state == Esp32EncodedDcMotor::MotorState::SoftStarting);
  assert(snapshot.targetPulses == 1000);

  encoder.position = 1000;
  motor.update(1200);
  snapshot = motor.snapshot();
  assert(snapshot.state == Esp32EncodedDcMotor::MotorState::Idle);
  assert(snapshot.remainingPulses == 0);
  assert(driver.stopCount == 1);

  protection.maxRunPulses = 500;
  assert(motor.configure(kinematics, profile, protection, stopPolicy) ==
         Esp32EncodedDcMotor::MotorResult::Ok);
  assert(motor.requestMovePulses(1000) == Esp32EncodedDcMotor::MotorResult::Ok);
  encoder.position = 1601;
  motor.update(1300);
  snapshot = motor.snapshot();
  assert(snapshot.state == Esp32EncodedDcMotor::MotorState::Fault);
  assert(snapshot.faultReason == Esp32EncodedDcMotor::FaultReason::MaxRunPulses);
  assert(driver.stopCount == 2);

  FakeDriver stopDriver;
  FakeEncoder stopEncoder;
  Esp32EncodedDcMotor::EncodedDcMotor stopMotor;
  protection.maxRunPulses = 5000;
  assert(stopMotor.begin(stopDriver, stopEncoder, hardware, encoderConfig) ==
         Esp32EncodedDcMotor::MotorResult::Ok);
  assert(stopMotor.configure(kinematics, profile, protection, stopPolicy) ==
         Esp32EncodedDcMotor::MotorResult::Ok);
  assert(stopMotor.requestMovePulses(1000) == Esp32EncodedDcMotor::MotorResult::Ok);
  stopMotor.update(1000);
  assert(stopMotor.snapshot().state == Esp32EncodedDcMotor::MotorState::Running);
  assert(stopMotor.requestStop() == Esp32EncodedDcMotor::MotorResult::Ok);
  assert(stopMotor.snapshot().state == Esp32EncodedDcMotor::MotorState::SoftStopping);
  assert(stopDriver.stopCount == 0);
  stopMotor.update(1600);
  assert(stopMotor.snapshot().state == Esp32EncodedDcMotor::MotorState::Idle);
  assert(stopDriver.stopCount == 1);

  return 0;
}
