# Esp32FarmDoor Minimal App Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Create the first compile-ready `apps/Esp32FarmDoor` PlatformIO application skeleton without implementing full business behavior.

**Architecture:** The app is a thin Arduino entry point that initializes Esp32Base and references the three FarmAuto public libraries. It keeps hardware constants and compile-time switches local to the app, while business state machines, Web pages, persistence layouts, and hardware adapters remain future work.

**Tech Stack:** PlatformIO, Arduino ESP32, Esp32Base FULL profile, FarmAuto `lib/` public libraries.

---

## Scope

Create only:

- `apps/Esp32FarmDoor/platformio.ini`
- `apps/Esp32FarmDoor/src/main.cpp`
- `apps/Esp32FarmDoor/README.md`

Do not create `apps/Esp32FarmFeeder`.
Do not implement real Web pages, route handlers, AT24C Wire driver, PCNT/LEDC driver, ADC sampling, or door state machine.

## Task 1: Create Minimal Compile Target

- [ ] Create app directory and `platformio.ini`.
- [ ] Configure board as `esp32dev` for ESP32-WROOM-32E compatible target.
- [ ] Reference Esp32Base by `symlink://../../../Esp32Base`.
- [ ] Reference FarmAuto local libraries by `symlink://../../lib/<LibraryName>`.
- [ ] Enable Esp32Base FULL profile and App Config capacity macros.
- [ ] Use Esp32Base 4MB OTA balanced partition CSV.

Verification:

```bash
pio run -d apps/Esp32FarmDoor
```

Expected first result before `src/main.cpp`: fail because source is missing.

## Task 2: Add Minimal Arduino Entry

- [ ] Create `src/main.cpp`.
- [ ] Include `Arduino.h`, `Esp32Base.h`, and the three public library headers.
- [ ] Define hardware constants for current known FarmDoor pins, including INA240A2 GPIO33.
- [ ] Define `FARMAUTO_FARMDOOR_ENABLE_INA240A2` with default `1`.
- [ ] Instantiate public library config objects without doing hardware I/O.
- [ ] Call `Esp32Base::setFirmwareInfo()`, configure basic Web labels, then `Esp32Base::begin()`.
- [ ] `loop()` calls `Esp32Base::handle()`.

Verification:

```bash
pio run -d apps/Esp32FarmDoor
```

Expected: successful compile.

## Task 3: Document App Skeleton Boundary

- [ ] Add app README stating current status and what is intentionally not implemented.
- [ ] Update root README source status from public libraries only to public libraries plus FarmDoor skeleton.

Verification:

```bash
tools/run_host_tests.sh
pio run -d apps/Esp32FarmDoor
git status --short
```

Expected: host tests pass, FarmDoor compiles, no `old_prj/` or Esp32Base changes.
