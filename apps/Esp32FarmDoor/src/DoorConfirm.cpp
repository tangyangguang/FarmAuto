#include "DoorConfirm.h"

#include <cstdio>
#include <cstring>

namespace {

bool nonEmpty(const char* value) {
  return value != nullptr && value[0] != '\0';
}

bool copyExact(char* dest, std::size_t destSize, const char* src) {
  if (!nonEmpty(src) || destSize == 0) {
    return false;
  }
  const std::size_t length = std::strlen(src);
  if (length + 1 > destSize) {
    return false;
  }
  std::memcpy(dest, src, length + 1);
  return true;
}

bool expired(uint32_t nowMs, uint32_t expiresAtMs) {
  return static_cast<int32_t>(nowMs - expiresAtMs) > 0;
}

}  // namespace

bool DoorConfirmGuard::issue(const char* action,
                             const char* resource,
                             uint32_t nowMs,
                             uint32_t randomValue,
                             char* outToken,
                             std::size_t outTokenSize) {
  if (outToken == nullptr || outTokenSize < sizeof(token_) ||
      !copyExact(action_, sizeof(action_), action) ||
      !copyExact(resource_, sizeof(resource_), resource)) {
    active_ = false;
    return false;
  }

  uint32_t tokenValue = randomValue ^ nowMs ^ 0xA55A5AA5UL;
  if (tokenValue == 0) {
    tokenValue = 0x13579BDFUL;
  }
  std::snprintf(token_, sizeof(token_), "%08lX", static_cast<unsigned long>(tokenValue));
  std::memcpy(outToken, token_, sizeof(token_));
  expiresAtMs_ = nowMs + kTtlMs;
  active_ = true;
  return true;
}

bool DoorConfirmGuard::consume(const char* action,
                               const char* resource,
                               const char* token,
                               uint32_t nowMs) {
  const bool matched = active_ && !expired(nowMs, expiresAtMs_) &&
                       std::strcmp(action_, action == nullptr ? "" : action) == 0 &&
                       std::strcmp(resource_, resource == nullptr ? "" : resource) == 0 &&
                       std::strcmp(token_, token == nullptr ? "" : token) == 0;
  active_ = false;
  return matched;
}
