#pragma once

#include <cstdint>

bool feederLocalDateAndMinute(uint32_t epochSec, uint32_t& date, uint16_t& minute);
bool feederNextServiceDate(uint32_t serviceDate, uint32_t& nextServiceDate);
