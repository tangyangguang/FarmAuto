#include "FeederPersistenceStore.h"

#include "FeederStorageLayout.h"

namespace {

Esp32At24cRecordStore::Result scheduleCodecResultToStoreResult(FeederScheduleCodecResult result) {
  switch (result) {
    case FeederScheduleCodecResult::Ok: return Esp32At24cRecordStore::Result::Ok;
    case FeederScheduleCodecResult::InvalidArgument:
      return Esp32At24cRecordStore::Result::InvalidArgument;
    case FeederScheduleCodecResult::BufferTooSmall:
      return Esp32At24cRecordStore::Result::PayloadTooLarge;
    case FeederScheduleCodecResult::UnsupportedVersion:
      return Esp32At24cRecordStore::Result::LayoutMismatch;
    case FeederScheduleCodecResult::CrcMismatch:
      return Esp32At24cRecordStore::Result::CrcMismatch;
  }
  return Esp32At24cRecordStore::Result::InvalidArgument;
}

Esp32At24cRecordStore::Result targetCodecResultToStoreResult(FeederTargetCodecResult result) {
  switch (result) {
    case FeederTargetCodecResult::Ok: return Esp32At24cRecordStore::Result::Ok;
    case FeederTargetCodecResult::InvalidArgument:
      return Esp32At24cRecordStore::Result::InvalidArgument;
    case FeederTargetCodecResult::BufferTooSmall:
      return Esp32At24cRecordStore::Result::PayloadTooLarge;
    case FeederTargetCodecResult::UnsupportedVersion:
      return Esp32At24cRecordStore::Result::LayoutMismatch;
    case FeederTargetCodecResult::CrcMismatch:
      return Esp32At24cRecordStore::Result::CrcMismatch;
  }
  return Esp32At24cRecordStore::Result::InvalidArgument;
}

Esp32At24cRecordStore::Result bucketCodecResultToStoreResult(FeederBucketCodecResult result) {
  switch (result) {
    case FeederBucketCodecResult::Ok: return Esp32At24cRecordStore::Result::Ok;
    case FeederBucketCodecResult::InvalidArgument:
      return Esp32At24cRecordStore::Result::InvalidArgument;
    case FeederBucketCodecResult::BufferTooSmall:
      return Esp32At24cRecordStore::Result::PayloadTooLarge;
    case FeederBucketCodecResult::UnsupportedVersion:
      return Esp32At24cRecordStore::Result::LayoutMismatch;
    case FeederBucketCodecResult::CrcMismatch:
      return Esp32At24cRecordStore::Result::CrcMismatch;
  }
  return Esp32At24cRecordStore::Result::InvalidArgument;
}

Esp32At24cRecordStore::Result calibrationCodecResultToStoreResult(
    FeederCalibrationCodecResult result) {
  switch (result) {
    case FeederCalibrationCodecResult::Ok: return Esp32At24cRecordStore::Result::Ok;
    case FeederCalibrationCodecResult::InvalidArgument:
      return Esp32At24cRecordStore::Result::InvalidArgument;
    case FeederCalibrationCodecResult::BufferTooSmall:
      return Esp32At24cRecordStore::Result::PayloadTooLarge;
    case FeederCalibrationCodecResult::UnsupportedVersion:
      return Esp32At24cRecordStore::Result::LayoutMismatch;
    case FeederCalibrationCodecResult::CrcMismatch:
      return Esp32At24cRecordStore::Result::CrcMismatch;
  }
  return Esp32At24cRecordStore::Result::InvalidArgument;
}

}  // namespace

Esp32At24cRecordStore::Result saveFeederSchedule(
    Esp32At24cRecordStore::RecordStore& store,
    const FeederScheduleSnapshot& snapshot) {
  uint8_t payload[kFeederScheduleEncodedBytes] = {};
  std::size_t encodedLength = 0;
  const FeederScheduleCodecResult encodeResult =
      encodeFeederScheduleSnapshot(snapshot, payload, sizeof(payload), encodedLength);
  if (encodeResult != FeederScheduleCodecResult::Ok) {
    return scheduleCodecResultToStoreResult(encodeResult);
  }
  return store.write(static_cast<uint16_t>(FeederAt24cRecordType::Schedule),
                     payload,
                     encodedLength);
}

Esp32At24cRecordStore::Result loadFeederSchedule(Esp32At24cRecordStore::RecordStore& store,
                                                 FeederScheduleSnapshot& out) {
  uint8_t payload[kFeederScheduleEncodedBytes] = {};
  std::size_t length = 0;
  const Esp32At24cRecordStore::Result readResult =
      store.readLatest(static_cast<uint16_t>(FeederAt24cRecordType::Schedule),
                       payload,
                       sizeof(payload),
                       length);
  if (readResult != Esp32At24cRecordStore::Result::Ok) {
    return readResult;
  }
  return scheduleCodecResultToStoreResult(decodeFeederScheduleSnapshot(payload, length, out));
}

Esp32At24cRecordStore::Result saveFeederTargets(Esp32At24cRecordStore::RecordStore& store,
                                                const FeederTargetSnapshot& snapshot) {
  uint8_t payload[kFeederTargetEncodedBytes] = {};
  std::size_t encodedLength = 0;
  const FeederTargetCodecResult encodeResult =
      encodeFeederTargetSnapshot(snapshot, payload, sizeof(payload), encodedLength);
  if (encodeResult != FeederTargetCodecResult::Ok) {
    return targetCodecResultToStoreResult(encodeResult);
  }
  return store.write(static_cast<uint16_t>(FeederAt24cRecordType::ChannelTarget),
                     payload,
                     encodedLength);
}

Esp32At24cRecordStore::Result loadFeederTargets(Esp32At24cRecordStore::RecordStore& store,
                                                FeederTargetSnapshot& out) {
  uint8_t payload[kFeederTargetEncodedBytes] = {};
  std::size_t length = 0;
  const Esp32At24cRecordStore::Result readResult =
      store.readLatest(static_cast<uint16_t>(FeederAt24cRecordType::ChannelTarget),
                       payload,
                       sizeof(payload),
                       length);
  if (readResult != Esp32At24cRecordStore::Result::Ok) {
    return readResult;
  }
  return targetCodecResultToStoreResult(decodeFeederTargetSnapshot(payload, length, out));
}

Esp32At24cRecordStore::Result saveFeederBuckets(Esp32At24cRecordStore::RecordStore& store,
                                                const FeederBucketSnapshot& snapshot) {
  uint8_t payload[kFeederBucketEncodedBytes] = {};
  std::size_t encodedLength = 0;
  const FeederBucketCodecResult encodeResult =
      encodeFeederBucketSnapshot(snapshot, payload, sizeof(payload), encodedLength);
  if (encodeResult != FeederBucketCodecResult::Ok) {
    return bucketCodecResultToStoreResult(encodeResult);
  }
  return store.write(static_cast<uint16_t>(FeederAt24cRecordType::BucketState),
                     payload,
                     encodedLength);
}

Esp32At24cRecordStore::Result loadFeederBuckets(Esp32At24cRecordStore::RecordStore& store,
                                                FeederBucketSnapshot& out) {
  uint8_t payload[kFeederBucketEncodedBytes] = {};
  std::size_t length = 0;
  const Esp32At24cRecordStore::Result readResult =
      store.readLatest(static_cast<uint16_t>(FeederAt24cRecordType::BucketState),
                       payload,
                       sizeof(payload),
                       length);
  if (readResult != Esp32At24cRecordStore::Result::Ok) {
    return readResult;
  }
  return bucketCodecResultToStoreResult(decodeFeederBucketSnapshot(payload, length, out));
}

Esp32At24cRecordStore::Result saveFeederCalibration(
    Esp32At24cRecordStore::RecordStore& store,
    const FeederBucketSnapshot& snapshot) {
  uint8_t payload[kFeederCalibrationEncodedBytes] = {};
  std::size_t encodedLength = 0;
  const FeederCalibrationCodecResult encodeResult =
      encodeFeederCalibrationSnapshot(snapshot, payload, sizeof(payload), encodedLength);
  if (encodeResult != FeederCalibrationCodecResult::Ok) {
    return calibrationCodecResultToStoreResult(encodeResult);
  }
  return store.write(static_cast<uint16_t>(FeederAt24cRecordType::Calibration),
                     payload,
                     encodedLength);
}

Esp32At24cRecordStore::Result loadFeederCalibration(Esp32At24cRecordStore::RecordStore& store,
                                                    FeederBucketSnapshot& out) {
  uint8_t payload[kFeederCalibrationEncodedBytes] = {};
  std::size_t length = 0;
  const Esp32At24cRecordStore::Result readResult =
      store.readLatest(static_cast<uint16_t>(FeederAt24cRecordType::Calibration),
                       payload,
                       sizeof(payload),
                       length);
  if (readResult != Esp32At24cRecordStore::Result::Ok) {
    return readResult;
  }
  return calibrationCodecResultToStoreResult(
      decodeFeederCalibrationSnapshot(payload, length, out));
}
