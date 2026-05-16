#pragma once

#include <cstddef>
#include <cstdint>

#include "FeederRecordCodec.h"

enum class FeederRecordWriteResult : uint8_t {
  Ok,
  InvalidArgument,
  EncodeFailed,
  WriteFailed
};

using FeederRecordAppendBytes =
    bool (*)(const char* path, const uint8_t* data, std::size_t length, void* user);

FeederRecordWriteResult appendFeederRecordToPath(const FeederRecord& record,
                                                 const char* path,
                                                 FeederRecordAppendBytes appendBytes,
                                                 void* user);
