#include <cassert>
#include <cstring>

#include "FeederConfirm.h"

int main() {
  FeederConfirmGuard guard;
  char token[17] = {};

  assert(guard.issue("skip", "plan:1:20260517", 1000, 0x12345678, token, sizeof(token)));
  assert(std::strlen(token) == 8);
  assert(!guard.consume("skip", "plan:2:20260517", token, 2000));

  assert(guard.issue("skip", "plan:1:20260517", 1000, 0x12345678, token, sizeof(token)));
  assert(!guard.consume("cancel-skip", "plan:1:20260517", token, 2000));

  assert(guard.issue("skip", "plan:1:20260517", 1000, 0x12345678, token, sizeof(token)));
  assert(!guard.consume("skip", "plan:1:20260517", "bad", 2000));
  assert(!guard.consume("skip", "plan:1:20260517", token, 2000));

  assert(guard.issue("skip", "plan:1:20260517", 1000, 0x12345678, token, sizeof(token)));
  assert(!guard.consume("skip", "plan:1:20260517", token, 61001));

  assert(guard.issue("skip", "plan:1:20260517", 1000, 0x12345678, token, sizeof(token)));
  assert(guard.consume("skip", "plan:1:20260517", token, 2000));
  assert(!guard.consume("skip", "plan:1:20260517", token, 2000));

  assert(!guard.issue("", "resource", 1000, 1, token, sizeof(token)));
  assert(!guard.issue("action", "", 1000, 1, token, sizeof(token)));
  assert(!guard.issue("action", "resource", 1000, 1, token, 4));

  return 0;
}
