#include <cassert>
#include <cstdint>

#include "FeederDate.h"

int main() {
  uint32_t next = 0;
  assert(feederNextServiceDate(20260517, next));
  assert(next == 20260518);
  assert(feederNextServiceDate(20260531, next));
  assert(next == 20260601);
  assert(feederNextServiceDate(20260228, next));
  assert(next == 20260301);
  assert(feederNextServiceDate(20240228, next));
  assert(next == 20240229);
  assert(!feederNextServiceDate(0, next));
  assert(!feederNextServiceDate(20261301, next));
  assert(!feederNextServiceDate(20260431, next));

  uint32_t date = 0;
  uint16_t minute = 0;
  assert(feederLocalDateAndMinute(0, date, minute));
  assert(date >= 19700101);
  assert(minute < 24 * 60);

  return 0;
}
