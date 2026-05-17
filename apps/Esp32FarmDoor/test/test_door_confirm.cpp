#include <cassert>
#include <cstring>

#include "DoorConfirm.h"

int main() {
  DoorConfirmGuard guard;
  char token[17] = {};

  assert(guard.issue("set-position", "position:open", 1000, 0x22334455, token, sizeof(token)));
  assert(std::strlen(token) == 8);
  assert(!guard.consume("set-position", "position:closed", token, 2000));

  assert(guard.issue("set-position", "position:open", 1000, 0x22334455, token, sizeof(token)));
  assert(!guard.consume("set-travel", "position:open", token, 2000));

  assert(guard.issue("set-position", "position:open", 1000, 0x22334455, token, sizeof(token)));
  assert(!guard.consume("set-position", "position:open", "bad", 2000));
  assert(!guard.consume("set-position", "position:open", token, 2000));

  assert(guard.issue("set-position", "position:open", 1000, 0x22334455, token, sizeof(token)));
  assert(!guard.consume("set-position", "position:open", token, 61001));

  assert(guard.issue("set-position", "position:open", 1000, 0x22334455, token, sizeof(token)));
  assert(guard.consume("set-position", "position:open", token, 2000));
  assert(!guard.consume("set-position", "position:open", token, 2000));

  return 0;
}
