# 公共库接口冻结字段表

## 目标

本文集中记录三个公共库进入源码骨架前必须冻结的字段、结果码和事件结构。具体源码接口可以微调命名，但字段语义、单位和是否必需应以本文为准。

## 通用约定

- 时间字段统一使用 `uint32_t` 毫秒，字段名以 `Ms` 结尾。
- 脉冲、位置和累计计数优先使用 `int64_t` 或 `uint64_t`。
- 电流单位统一使用 mA。
- 电压单位统一使用 mV。
- 百分比字段使用整数 0-100。
- event sink 可为空；回调必须短耗时、不阻塞、不分配大对象。
- snapshot 用于远程状态查询，event 用于记录状态变化和错误。

## Esp32EncodedDcMotor 字段

配置字段：

```text
MotorHardwareConfig
  driverType
  pwmFrequencyHz
  pwmResolutionBits
  pwmPolarity
  ledcChannelA
  ledcChannelB

EncoderBackendConfig
  backendType
  pinA
  pinB
  countMode
  pcntUnit
  pcntChannelA
  pcntChannelB
  glitchFilterNs

MotorKinematics
  motorShaftPulsesPerRev
  gearRatio
  outputPulsesPerRev
  countMode
  motorDirectionInverted
  encoderDirectionInverted

MotorMotionProfile
  speedPercent
  softStartMs
  softStopMs
  minEffectiveSpeedPercent
  controlTickMs

MotorProtection
  startupGraceMs
  stallCheckIntervalMs
  minPulseDelta
  maxRunMs
  maxRunPulses

MotorStopPolicy
  normalStopMode
  faultStopMode
  brakeMs
  emergencyOutputMode
```

结果码：

```text
Ok
Busy
InvalidArgument
InvalidState
NotInitialized
AlreadyAtTarget
FaultActive
ConfigMissing
TargetTooSmall
DriverRejected
```

snapshot 字段：

```text
MotorSnapshot
  state
  activeCommand
  direction
  currentSpeedPercent
  targetSpeedPercent
  driverOutputPercent
  positionPulses
  segmentStartPulses
  targetPulses
  remainingPulses
  pulsesPerSecond
  rpm
  elapsedMs
  lastUpdateMs
  lastPulseMs
  faultReason
  lastCommandResult
  encoderDeltaSinceLastCheck
```

trace 字段：

```text
MotorTracePoint
  timestampMs
  state
  activeCommand
  direction
  positionPulses
  targetPulses
  remainingPulses
  pulsesPerSecond
  rpm
  targetSpeedPercent
  driverOutputPercent
  encoderDelta
  faultReason
```

event 字段：

```text
MotorEvent
  type
  timestampMs
  state
  result
  faultReason
  positionPulses
```

## Esp32MotorCurrentGuard 字段

配置字段：

```text
Ina240A2Config
  adcPin
  adcAttenuation
  adcReferenceMv
  adcResolutionBits
  gain
  rsenseMilliOhm
  zeroOffsetMv
  bidirectional

MotorCurrentGuardConfig
  enabled
  warningThresholdMa
  faultThresholdMa
  startupGraceMs
  confirmationMs
  confirmationSamples
  filterAlpha
  sensorFaultPolicy
```

采样字段：

```text
CurrentSample
  ok
  sequence
  rawAdc
  voltageMv
  currentMa
  timestampMs
  sensorStatus
  sampleLost
```

snapshot 字段：

```text
CurrentGuardSnapshot
  state
  rawCurrentMa
  filteredCurrentMa
  peakCurrentMa
  thresholdMa
  warningThresholdMa
  faultThresholdMa
  sampleRateHz
  lastSampleMs
  overThresholdSinceMs
  consecutiveOverThresholdSamples
  adcSaturationCount
  sensorFaultCount
  warningSinceMs
  faultSinceMs
  sensorStatus
  faultReason
```

trace 字段：

```text
CurrentTracePoint
  timestampMs
  sequence
  rawAdc
  voltageMv
  rawCurrentMa
  filteredCurrentMa
  peakCurrentMa
  warningThresholdMa
  thresholdMa
  state
  sensorStatus
  faultReason
  sampleLost
  adcSaturated
```

event 字段：

```text
CurrentGuardEvent
  type
  timestampMs
  state
  faultReason
  filteredCurrentMa
  thresholdMa
```

## Esp32At24cRecordStore 字段

配置字段：

```text
At24cChipConfig
  i2cAddress
  capacityBytes
  pageSizeBytes
  addressBytes
  writeCycleMs
  maxWriteChunkBytes
  smallDeviceAddressBits

RecordRegion
  recordType
  startAddress
  slotSize
  slotCount
  writeClass

StoreLayout
  magic
  layoutVersion
  regions[]
```

结果码：

```text
Ok
NotInitialized
DeviceOffline
InvalidConfig
LayoutMismatch
OutOfRange
PayloadTooLarge
ReadFailed
WriteFailed
CrcMismatch
HeaderCrcMismatch
NoValidRecord
Unchanged
AckTimeout
CompareFailed
FormatRequired
```

inspect 字段：

```text
RecordInspect
  deviceOnline
  regionFound
  regionUsedBytes
  validSlotCount
  latestSequence
  latestSchemaVersion
  nextSlotIndex
  totalWrites
  unchangedSkips
  crcErrorCount
  verifyFailedCount
  ackTimeoutCount
  estimatedWritesPerSlot
  estimatedLifetimePercent
  lastError
  slots[]

RecordSlotInspect
  slotIndex
  address
  flags
  sequence
  schemaVersion
  payloadLength
  headerCrcOk
  payloadCrcOk
```

event 字段：

```text
RecordStoreEvent
  type
  timestampMs
  result
  recordType
  sequence
  address
```
