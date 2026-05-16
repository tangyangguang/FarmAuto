# Esp32FarmDoor Read-Only Diagnostics Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add safe read-only hardware diagnostics to `Esp32FarmDoor` so the current PCB can be flashed and inspected before enabling motor movement.

**Architecture:** Diagnostics live in the app shell, not in public libraries. The route returns raw hardware observations only: button GPIO levels, encoder pin levels, GPIO33 ADC raw value, and AT24C I2C presence. It does not drive PWM, move the motor, write AT24C, or change configuration.

**Tech Stack:** Arduino ESP32, PlatformIO, Esp32Base Web API, Wire, FarmAuto public libraries.

---

## Task 1: Add Read-Only Diagnostics Route

Files:

- Modify `apps/Esp32FarmDoor/src/FarmDoorApp.h`
- Modify `apps/Esp32FarmDoor/src/FarmDoorApp.cpp`

Steps:

- Add `configureHardwareInputs()` to set button pins to `INPUT`.
- Start `Wire` on GPIO21/GPIO22.
- Set ADC read resolution to 12 bit and configure GPIO33 attenuation.
- Add `/api/app/diagnostics` route.
- Return JSON fields:
  - `buttons.aux/open/close/stop`
  - `encoder.a/b`
  - `currentSensor.adcPin/rawAdc/compileEnabled/runtimeEnabled`
  - `at24c.address/online`
  - `motorOutput.enabled=false`

Verification:

```bash
tools/check_all.sh
```

Expected: host tests pass and `pio run -d apps/Esp32FarmDoor` succeeds.

## Task 2: Document Firmware Use

Files:

- Modify `apps/Esp32FarmDoor/README.md`

Steps:

- Document `/api/app/status`.
- Document `/api/app/diagnostics`.
- State diagnostics are read-only and safe for first flashing.

Verification:

```bash
tools/check_all.sh
git status --short
```

Expected: no `old_prj/`, `.pio/`, or Esp32Base files staged.
