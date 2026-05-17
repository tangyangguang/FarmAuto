#include "FeederDate.h"

#include <time.h>

bool feederLocalDateAndMinute(uint32_t epochSec, uint32_t& date, uint16_t& minute) {
  time_t raw = static_cast<time_t>(epochSec);
  tm value = {};
  if (!localtime_r(&raw, &value)) {
    return false;
  }
  date = static_cast<uint32_t>((value.tm_year + 1900) * 10000 +
                               (value.tm_mon + 1) * 100 +
                               value.tm_mday);
  minute = static_cast<uint16_t>(value.tm_hour * 60 + value.tm_min);
  return true;
}

bool feederNextServiceDate(uint32_t serviceDate, uint32_t& nextServiceDate) {
  const int year = static_cast<int>(serviceDate / 10000);
  const int month = static_cast<int>((serviceDate / 100) % 100);
  const int day = static_cast<int>(serviceDate % 100);
  if (year < 1970 || month < 1 || month > 12 || day < 1 || day > 31) {
    return false;
  }

  tm value = {};
  value.tm_year = year - 1900;
  value.tm_mon = month - 1;
  value.tm_mday = day + 1;
  value.tm_hour = 12;
  value.tm_isdst = -1;
  const time_t normalized = mktime(&value);
  if (normalized == static_cast<time_t>(-1)) {
    return false;
  }

  tm next = {};
  if (!localtime_r(&normalized, &next)) {
    return false;
  }
  tm original = {};
  original.tm_year = year - 1900;
  original.tm_mon = month - 1;
  original.tm_mday = day;
  original.tm_hour = 12;
  original.tm_isdst = -1;
  const time_t originalTime = mktime(&original);
  tm normalizedOriginal = {};
  if (originalTime == static_cast<time_t>(-1) ||
      !localtime_r(&originalTime, &normalizedOriginal) ||
      normalizedOriginal.tm_year + 1900 != year ||
      normalizedOriginal.tm_mon + 1 != month ||
      normalizedOriginal.tm_mday != day) {
    return false;
  }

  nextServiceDate = static_cast<uint32_t>((next.tm_year + 1900) * 10000 +
                                          (next.tm_mon + 1) * 100 +
                                          next.tm_mday);
  return true;
}
