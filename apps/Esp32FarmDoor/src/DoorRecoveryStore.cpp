#include "DoorRecoveryStore.h"

#include "DoorStorageLayout.h"

namespace {

Esp32At24cRecordStore::Result codecResultToStoreResult(DoorRecoveryCodecResult result) {
  switch (result) {
    case DoorRecoveryCodecResult::Ok: return Esp32At24cRecordStore::Result::Ok;
    case DoorRecoveryCodecResult::InvalidArgument:
      return Esp32At24cRecordStore::Result::InvalidArgument;
    case DoorRecoveryCodecResult::BufferTooSmall:
      return Esp32At24cRecordStore::Result::PayloadTooLarge;
    case DoorRecoveryCodecResult::UnsupportedVersion:
      return Esp32At24cRecordStore::Result::LayoutMismatch;
    case DoorRecoveryCodecResult::CrcMismatch:
      return Esp32At24cRecordStore::Result::CrcMismatch;
  }
  return Esp32At24cRecordStore::Result::InvalidArgument;
}

}  // namespace

Esp32At24cRecordStore::Result saveDoorRecoveryState(
    Esp32At24cRecordStore::RecordStore& store,
    const DoorRecoveryState& state) {
  uint8_t payload[kDoorRecoveryEncodedBytes] = {};
  std::size_t encodedLength = 0;
  const DoorRecoveryCodecResult encodeResult =
      encodeDoorRecoveryState(state, payload, sizeof(payload), encodedLength);
  if (encodeResult != DoorRecoveryCodecResult::Ok) {
    return codecResultToStoreResult(encodeResult);
  }
  return store.write(static_cast<uint16_t>(DoorAt24cRecordType::State), payload, encodedLength);
}

Esp32At24cRecordStore::Result loadDoorRecoveryState(Esp32At24cRecordStore::RecordStore& store,
                                                    DoorRecoveryState& out) {
  uint8_t payload[kDoorRecoveryEncodedBytes] = {};
  std::size_t length = 0;
  const Esp32At24cRecordStore::Result readResult =
      store.readLatest(static_cast<uint16_t>(DoorAt24cRecordType::State),
                       payload,
                       sizeof(payload),
                       length);
  if (readResult != Esp32At24cRecordStore::Result::Ok) {
    return readResult;
  }
  return codecResultToStoreResult(decodeDoorRecoveryState(payload, length, out));
}
