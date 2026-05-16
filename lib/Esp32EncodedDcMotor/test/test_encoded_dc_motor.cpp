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
    return Esp32EncodedDcMotor::MotorResult::Ok;
  }

  int8_t lastDirection = 0;
  uint8_t lastPercent = 0;
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

  if (motor.begin(driver, encoder, hardware, encoderConfig) != Esp32EncodedDcMotor::MotorResult::Ok) {
    return 1;
  }
  if (motor.configure(kinematics, profile, protection, stopPolicy) != Esp32EncodedDcMotor::MotorResult::Ok) {
    return 2;
  }
  if (motor.requestMovePulses(1000) != Esp32EncodedDcMotor::MotorResult::Ok) {
    return 3;
  }
  motor.update(0);
  auto snapshot = motor.snapshot();
  if (snapshot.state != Esp32EncodedDcMotor::MotorState::SoftStarting) {
    return 4;
  }
  if (snapshot.targetPulses != 1000) {
    return 5;
  }
  return 0;
}
