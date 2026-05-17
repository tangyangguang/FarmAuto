#pragma once

#include <cstddef>
#include <cstdint>

class DoorConfirmGuard {
 public:
  static constexpr uint32_t kTtlMs = 60UL * 1000UL;

  bool issue(const char* action,
             const char* resource,
             uint32_t nowMs,
             uint32_t randomValue,
             char* outToken,
             std::size_t outTokenSize);
  bool consume(const char* action, const char* resource, const char* token, uint32_t nowMs);

 private:
  char action_[24] = {};
  char resource_[40] = {};
  char token_[9] = {};
  uint32_t expiresAtMs_ = 0;
  bool active_ = false;
};
