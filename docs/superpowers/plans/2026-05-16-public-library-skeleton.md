# Public Library Skeleton Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Create first-pass source skeletons for FarmAuto public libraries without entering application business implementation.

**Architecture:** Each library is an independent PlatformIO-compatible library under `lib/`, with `include/`, `src/`, `examples/`, `test/`, `library.json`, and `README.md`. Core logic must not depend on Esp32Base; any future Esp32Base integration remains in applications or thin adapters. The first pass favors stable public types, minimal behavior, and compile-ready code over complete hardware implementation.

**Tech Stack:** C++17, Arduino/PlatformIO library layout, no root application project, no changes to Esp32Base or `old_prj/`.

---

## Scope

Implement only source skeleton and minimal deterministic behavior for:

- `lib/Esp32At24cRecordStore`
- `lib/Esp32EncodedDcMotor`
- `lib/Esp32MotorCurrentGuard`

Do not create:

- `apps/Esp32FarmDoor`
- `apps/Esp32FarmFeeder`
- Esp32Base patches
- Hardware-specific PCNT/LEDC/ADC implementations beyond interface placeholders
- Old project compatibility code

## File Structure

Create:

```text
lib/Esp32At24cRecordStore/
  include/Esp32At24cRecordStore.h
  src/Esp32At24cRecordStore.cpp
  examples/BasicRecordStore/BasicRecordStore.ino
  test/test_record_store.cpp
  library.json
  README.md

lib/Esp32EncodedDcMotor/
  include/Esp32EncodedDcMotor.h
  src/Esp32EncodedDcMotor.cpp
  examples/BasicMotor/BasicMotor.ino
  test/test_encoded_dc_motor.cpp
  library.json
  README.md

lib/Esp32MotorCurrentGuard/
  include/Esp32MotorCurrentGuard.h
  src/Esp32MotorCurrentGuard.cpp
  examples/Ina240A2Guard/Ina240A2Guard.ino
  test/test_motor_current_guard.cpp
  library.json
  README.md
```

Modify:

```text
.gitignore
README.md
docs/02-roadmap.md
docs/libs/24-library-extraction-plan.md
```

Only modify docs if needed to reflect actual created skeleton paths. Keep public library docs free of automatic door or feeder business rules.

## Task 1: Repository Guardrails

**Files:**
- Modify: `.gitignore`
- Modify: `README.md`

- [ ] **Step 1: Check current ignore rules**

Run:

```bash
sed -n '1,200p' .gitignore
```

Expected: project-local build outputs such as `.pio/`, `.worktrees/`, and editor caches are ignored or can be added safely.

- [ ] **Step 2: Add missing build ignores only if absent**

If missing, add these exact lines:

```gitignore
.pio/
.worktrees/
build/
cmake-build-*/
```

- [ ] **Step 3: Update README with current source status**

Add a short section:

```markdown
## Source Status

Current source work starts with public library skeletons under `lib/`.
Application projects under `apps/Esp32FarmDoor` and `apps/Esp32FarmFeeder` are intentionally not created until public library interfaces and Esp32Base capability checks are complete.
```

- [ ] **Step 4: Verify no forbidden files are tracked**

Run:

```bash
git status --short
```

Expected: no files under `old_prj/`, `.pio/`, `.worktrees/`, or `.claude/` are staged.

## Task 2: Esp32At24cRecordStore Skeleton

**Files:**
- Create: `lib/Esp32At24cRecordStore/include/Esp32At24cRecordStore.h`
- Create: `lib/Esp32At24cRecordStore/src/Esp32At24cRecordStore.cpp`
- Create: `lib/Esp32At24cRecordStore/test/test_record_store.cpp`
- Create: `lib/Esp32At24cRecordStore/examples/BasicRecordStore/BasicRecordStore.ino`
- Create: `lib/Esp32At24cRecordStore/library.json`
- Create: `lib/Esp32At24cRecordStore/README.md`

- [ ] **Step 1: Write failing API compile test first**

Create `lib/Esp32At24cRecordStore/test/test_record_store.cpp`:

```cpp
#include <cstdint>
#include <vector>
#include "Esp32At24cRecordStore.h"

class FakeAt24cDevice : public Esp32At24cRecordStore::IAt24cDevice {
public:
  explicit FakeAt24cDevice(std::size_t size) : bytes(size, 0xFF) {}

  bool read(uint32_t address, uint8_t* data, std::size_t length) override {
    if (address + length > bytes.size()) return false;
    for (std::size_t i = 0; i < length; ++i) data[i] = bytes[address + i];
    return true;
  }

  bool write(uint32_t address, const uint8_t* data, std::size_t length) override {
    if (address + length > bytes.size()) return false;
    for (std::size_t i = 0; i < length; ++i) bytes[address + i] = data[i];
    return true;
  }

  std::vector<uint8_t> bytes;
};

int main() {
  FakeAt24cDevice device(16 * 1024);
  Esp32At24cRecordStore::RecordStore store;
  Esp32At24cRecordStore::RecordStoreConfig config{};
  config.layoutVersion = 1;
  config.baseAddress = 0;
  config.totalBytes = 16 * 1024;

  Esp32At24cRecordStore::RegionConfig region{};
  region.recordType = 1;
  region.offset = 0;
  region.slotSize = 128;
  region.slotCount = 4;

  auto result = store.begin(device, config, &region, 1);
  return result == Esp32At24cRecordStore::Result::Ok ? 0 : 1;
}
```

- [ ] **Step 2: Run compile test and verify it fails**

Run:

```bash
c++ -std=c++17 -Ilib/Esp32At24cRecordStore/include lib/Esp32At24cRecordStore/test/test_record_store.cpp lib/Esp32At24cRecordStore/src/Esp32At24cRecordStore.cpp -o /tmp/test_record_store
```

Expected: fail because header/source do not exist.

- [ ] **Step 3: Create minimal public header**

Create `lib/Esp32At24cRecordStore/include/Esp32At24cRecordStore.h` with:

```cpp
#pragma once

#include <cstddef>
#include <cstdint>

namespace Esp32At24cRecordStore {

enum class Result : uint8_t {
  Ok,
  Unchanged,
  InvalidArgument,
  NotInitialized,
  DeviceOffline,
  PayloadTooLarge,
  RegionNotFound,
  CrcMismatch,
  LayoutMismatch,
  WriteFailed,
  VerifyFailed,
  FormatRequired
};

enum class SlotState : uint8_t {
  Empty,
  Writing,
  Valid
};

class IAt24cDevice {
public:
  virtual ~IAt24cDevice() = default;
  virtual bool read(uint32_t address, uint8_t* data, std::size_t length) = 0;
  virtual bool write(uint32_t address, const uint8_t* data, std::size_t length) = 0;
};

struct RecordStoreConfig {
  uint16_t layoutVersion = 1;
  uint32_t baseAddress = 0;
  uint32_t totalBytes = 0;
  uint16_t pageSize = 64;
  bool writeOnlyWhenChanged = true;
};

struct RegionConfig {
  uint16_t recordType = 0;
  uint32_t offset = 0;
  uint16_t slotSize = 0;
  uint16_t slotCount = 0;
  uint16_t schemaVersion = 1;
};

struct RecordInspect {
  uint16_t recordType = 0;
  uint16_t validSlots = 0;
  uint16_t slotCount = 0;
  uint32_t latestSequence = 0;
  uint32_t estimatedWrites = 0;
  Result lastResult = Result::NotInitialized;
};

class RecordStore {
public:
  Result begin(IAt24cDevice& device,
               const RecordStoreConfig& config,
               const RegionConfig* regions,
               std::size_t regionCount);

  Result format();
  Result write(uint16_t recordType, const uint8_t* payload, std::size_t length);
  Result readLatest(uint16_t recordType, uint8_t* payload, std::size_t capacity, std::size_t& length);
  Result inspect(uint16_t recordType, RecordInspect& out) const;

  bool initialized() const;

private:
  IAt24cDevice* device_ = nullptr;
  RecordStoreConfig config_{};
  const RegionConfig* regions_ = nullptr;
  std::size_t regionCount_ = 0;
};

uint32_t crc32IsoHdlc(const uint8_t* data, std::size_t length);

}  // namespace Esp32At24cRecordStore
```

- [ ] **Step 4: Create minimal implementation**

Create `lib/Esp32At24cRecordStore/src/Esp32At24cRecordStore.cpp` with implementations for `begin()`, `initialized()`, and stubbed methods returning safe result codes. `begin()` must validate non-null regions, non-zero total bytes, non-zero slot size/count, and region bounds.

- [ ] **Step 5: Verify compile test passes**

Run:

```bash
c++ -std=c++17 -Ilib/Esp32At24cRecordStore/include lib/Esp32At24cRecordStore/test/test_record_store.cpp lib/Esp32At24cRecordStore/src/Esp32At24cRecordStore.cpp -o /tmp/test_record_store && /tmp/test_record_store
```

Expected: exit code `0`.

- [ ] **Step 6: Add library metadata and README**

Create `library.json` with name `Esp32At24cRecordStore`, framework `arduino`, platform `espressif32`, C++17 flags. Create README stating first skeleton is not a full persistence implementation yet.

- [ ] **Step 7: Commit**

Run:

```bash
git add lib/Esp32At24cRecordStore README.md .gitignore
git commit -m "feat: add at24c record store skeleton"
```

## Task 3: Esp32EncodedDcMotor Skeleton

**Files:**
- Create: `lib/Esp32EncodedDcMotor/include/Esp32EncodedDcMotor.h`
- Create: `lib/Esp32EncodedDcMotor/src/Esp32EncodedDcMotor.cpp`
- Create: `lib/Esp32EncodedDcMotor/test/test_encoded_dc_motor.cpp`
- Create: `lib/Esp32EncodedDcMotor/examples/BasicMotor/BasicMotor.ino`
- Create: `lib/Esp32EncodedDcMotor/library.json`
- Create: `lib/Esp32EncodedDcMotor/README.md`

- [ ] **Step 1: Write failing API compile test first**

Test must instantiate `EncodedDcMotor`, configure `MotorMotionProfile` with independent `softStartMs` and `softStopMs`, call `requestMovePulses(1000)`, and verify first `update(0)` reports active state.

- [ ] **Step 2: Run compile test and verify it fails**

Run:

```bash
c++ -std=c++17 -Ilib/Esp32EncodedDcMotor/include lib/Esp32EncodedDcMotor/test/test_encoded_dc_motor.cpp lib/Esp32EncodedDcMotor/src/Esp32EncodedDcMotor.cpp -o /tmp/test_encoded_dc_motor
```

Expected: fail because header/source do not exist.

- [ ] **Step 3: Create public header**

Header must define:

```text
MotorState: Idle, SoftStarting, Running, SoftStopping, Braking, Fault
MotorResult: Ok, Busy, InvalidArgument, InvalidState, NotInitialized, AlreadyAtTarget, FaultActive, ConfigMissing, TargetTooSmall, DriverRejected
CountMode: X1, X2, X4
MotorMotionProfile with softStartMs and softStopMs
MotorProtection
MotorSnapshot
MotorTracePoint
class IMotorDriver
class IEncoderReader
class EncodedDcMotor
```

- [ ] **Step 4: Create minimal implementation**

Implement configuration storage, `requestMovePulses()`, `requestStop()`, `requestEmergencyStop()`, `setCurrentPositionPulses()`, `update(nowMs)`, `snapshot()`, and `latestTracePoint()`. Use mock driver/encoder interfaces only; do not add ESP32 PCNT/LEDC code in this task.

- [ ] **Step 5: Verify compile test passes**

Run:

```bash
c++ -std=c++17 -Ilib/Esp32EncodedDcMotor/include lib/Esp32EncodedDcMotor/test/test_encoded_dc_motor.cpp lib/Esp32EncodedDcMotor/src/Esp32EncodedDcMotor.cpp -o /tmp/test_encoded_dc_motor && /tmp/test_encoded_dc_motor
```

Expected: exit code `0`.

- [ ] **Step 6: Add library metadata and README**

README must state the skeleton provides single-motor state machine and interfaces only; ESP32 PCNT/LEDC adapters come later.

- [ ] **Step 7: Commit**

Run:

```bash
git add lib/Esp32EncodedDcMotor
git commit -m "feat: add encoded dc motor skeleton"
```

## Task 4: Esp32MotorCurrentGuard Skeleton

**Files:**
- Create: `lib/Esp32MotorCurrentGuard/include/Esp32MotorCurrentGuard.h`
- Create: `lib/Esp32MotorCurrentGuard/src/Esp32MotorCurrentGuard.cpp`
- Create: `lib/Esp32MotorCurrentGuard/test/test_motor_current_guard.cpp`
- Create: `lib/Esp32MotorCurrentGuard/examples/Ina240A2Guard/Ina240A2Guard.ino`
- Create: `lib/Esp32MotorCurrentGuard/library.json`
- Create: `lib/Esp32MotorCurrentGuard/README.md`

- [ ] **Step 1: Write failing API compile test first**

Test must create `MotorCurrentGuard`, configure `faultThresholdMa`, feed samples below and above threshold, and verify confirmation samples produce `CurrentGuardState::Fault`.

- [ ] **Step 2: Run compile test and verify it fails**

Run:

```bash
c++ -std=c++17 -Ilib/Esp32MotorCurrentGuard/include lib/Esp32MotorCurrentGuard/test/test_motor_current_guard.cpp lib/Esp32MotorCurrentGuard/src/Esp32MotorCurrentGuard.cpp -o /tmp/test_motor_current_guard
```

Expected: fail because header/source do not exist.

- [ ] **Step 3: Create public header**

Header must define:

```text
Ina240A2Config
MotorCurrentGuardConfig
CurrentSample
CurrentGuardState
CurrentFaultReason
CurrentSnapshot
CurrentTracePoint
class MotorCurrentGuard
```

- [ ] **Step 4: Create minimal implementation**

Implement IIR filtering, startup grace, confirmation sample counting, warning/fault state, `snapshot()`, and `latestTracePoint()`. Do not implement real ADC read in this task.

- [ ] **Step 5: Verify compile test passes**

Run:

```bash
c++ -std=c++17 -Ilib/Esp32MotorCurrentGuard/include lib/Esp32MotorCurrentGuard/test/test_motor_current_guard.cpp lib/Esp32MotorCurrentGuard/src/Esp32MotorCurrentGuard.cpp -o /tmp/test_motor_current_guard && /tmp/test_motor_current_guard
```

Expected: exit code `0`.

- [ ] **Step 6: Add library metadata and README**

README must state INA240A2 is the first intended hardware path, while ADC integration and calibration are application/hardware follow-up tasks.

- [ ] **Step 7: Commit**

Run:

```bash
git add lib/Esp32MotorCurrentGuard
git commit -m "feat: add motor current guard skeleton"
```

## Task 5: Repository Verification

**Files:**
- Modify if needed: `docs/02-roadmap.md`
- Modify if needed: `docs/libs/24-library-extraction-plan.md`

- [ ] **Step 1: Run all host compile checks**

Run:

```bash
c++ -std=c++17 -Ilib/Esp32At24cRecordStore/include lib/Esp32At24cRecordStore/test/test_record_store.cpp lib/Esp32At24cRecordStore/src/Esp32At24cRecordStore.cpp -o /tmp/test_record_store && /tmp/test_record_store
c++ -std=c++17 -Ilib/Esp32EncodedDcMotor/include lib/Esp32EncodedDcMotor/test/test_encoded_dc_motor.cpp lib/Esp32EncodedDcMotor/src/Esp32EncodedDcMotor.cpp -o /tmp/test_encoded_dc_motor && /tmp/test_encoded_dc_motor
c++ -std=c++17 -Ilib/Esp32MotorCurrentGuard/include lib/Esp32MotorCurrentGuard/test/test_motor_current_guard.cpp lib/Esp32MotorCurrentGuard/src/Esp32MotorCurrentGuard.cpp -o /tmp/test_motor_current_guard && /tmp/test_motor_current_guard
```

Expected: all commands exit `0`.

- [ ] **Step 2: Check forbidden modifications**

Run:

```bash
git status --short
```

Expected: no changes under `old_prj/` or `/Users/tyg/dir/claude_dir/Esp32Base`.

- [ ] **Step 3: Update docs if actual skeleton differs from plan**

If paths or scope changed, update docs with exact created directories. Do not change application requirements.

- [ ] **Step 4: Final commit**

Run:

```bash
git add docs lib .gitignore README.md
git commit -m "docs: record public library skeleton status"
```

Skip commit if there are no doc/status changes after previous library commits.

## Self-Review Checklist

- Spec coverage:
  - Public library skeletons are covered.
  - Application code is intentionally excluded.
  - Esp32Base changes are intentionally excluded.
- Placeholder scan:
  - No `TBD` or `TODO` should be introduced in source headers.
  - README may say "future implementation" only when describing explicitly out-of-scope hardware adapters.
- Type consistency:
  - Result enum names must match tests.
  - Config field names must match docs where already frozen.
  - Units must use suffixes: `Ms`, `Ma`, `Mv`, `Pulses`, `Percent`.

