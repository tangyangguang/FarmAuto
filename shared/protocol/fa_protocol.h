#ifndef FA_PROTOCOL_H
#define FA_PROTOCOL_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define FA_PROTOCOL_VERSION 0x01u
#define FA_SOF0 0xA5u
#define FA_SOF1 0x5Au
#define FA_MASTER_ADDRESS 0x00u
#define FA_ADDRESS_MIN 0x01u
#define FA_ADDRESS_MAX 0x7Fu
#define FA_ADDRESS_BROADCAST 0xFFu
#define FA_MAX_PAYLOAD_LEN 64u
#define FA_FRAME_OVERHEAD 11u
#define FA_MAX_FRAME_LEN (FA_FRAME_OVERHEAD + FA_MAX_PAYLOAD_LEN)

typedef enum {
    FA_FRAME_OK = 0,
    FA_FRAME_INCOMPLETE = 1,
    FA_FRAME_ERR_NULL = -1,
    FA_FRAME_ERR_TOO_SHORT = -2,
    FA_FRAME_ERR_BAD_SOF = -3,
    FA_FRAME_ERR_UNSUPPORTED_VERSION = -4,
    FA_FRAME_ERR_PAYLOAD_TOO_LONG = -5,
    FA_FRAME_ERR_LENGTH_MISMATCH = -6,
    FA_FRAME_ERR_CRC = -7,
    FA_FRAME_ERR_OUTPUT_TOO_SMALL = -8
} FaFrameResult;

typedef enum {
    FA_FRAME_FLAG_RESPONSE = 0x01u,
    FA_FRAME_FLAG_ERROR = 0x02u
} FaFrameFlags;

typedef enum {
    FA_CMD_PING = 0x01u,
    FA_CMD_GET_STATUS = 0x02u,
    FA_CMD_SET_MOTOR_CONFIG = 0x10u,
    FA_CMD_START_ACTION = 0x20u,
    FA_CMD_STOP_ACTION = 0x21u,
    FA_CMD_CLEAR_FAULT = 0x22u,
    FA_CMD_IDENTIFY = 0x30u,
    FA_CMD_READ_DIAG = 0x40u
} FaCommand;

typedef enum {
    FA_STATUS_OK = 0x00u,
    FA_STATUS_ERR_UNSUPPORTED_VERSION = 0x01u,
    FA_STATUS_ERR_BAD_LENGTH = 0x02u,
    FA_STATUS_ERR_BAD_PARAM = 0x03u,
    FA_STATUS_ERR_NOT_CONFIGURED = 0x04u,
    FA_STATUS_ERR_BUSY = 0x05u,
    FA_STATUS_ERR_FAULT_ACTIVE = 0x06u,
    FA_STATUS_ERR_ACTION_DUPLICATE = 0x07u,
    FA_STATUS_ERR_SAFETY_BLOCKED = 0x08u,
    FA_STATUS_ERR_ADDRESS_RESERVED = 0x09u,
    FA_STATUS_ERR_INTERNAL = 0x0Au
} FaStatusCode;

typedef enum {
    FA_DEVICE_CLASS_UNKNOWN = 0x00u,
    FA_DEVICE_CLASS_MOTOR_ACTUATOR = 0x01u,
    FA_DEVICE_CLASS_SENSOR_NODE = 0x02u,
    FA_DEVICE_CLASS_SIMPLE_ACTUATOR = 0x03u
} FaDeviceClass;

typedef enum {
    FA_CAP_MOTOR_BIDIRECTIONAL = 1ul << 0,
    FA_CAP_HALL_AB_ENCODER = 1ul << 1,
    FA_CAP_CURRENT_SENSE = 1ul << 2,
    FA_CAP_BRAKE_SUPPORTED = 1ul << 3,
    FA_CAP_IDENTIFY_LED = 1ul << 4,
    FA_CAP_CONFIG_REQUIRED_AFTER_BOOT = 1ul << 5,
    FA_CAP_DIAG_SUPPORTED = 1ul << 6,
    FA_CAP_CLEAR_FAULT_SUPPORTED = 1ul << 7
} FaCapabilityFlags;

typedef enum {
    FA_STATION_BOOTING = 0u,
    FA_STATION_READY = 1u,
    FA_STATION_CONFIG_REQUIRED = 2u,
    FA_STATION_RUNNING = 3u,
    FA_STATION_FAULT = 4u,
    FA_STATION_ADDRESS_RESERVED = 5u
} FaStationState;

typedef enum {
    FA_MOTOR_UNCONFIGURED = 0u,
    FA_MOTOR_IDLE = 1u,
    FA_MOTOR_RUNNING = 2u,
    FA_MOTOR_STOPPING = 3u,
    FA_MOTOR_COMPLETED = 4u,
    FA_MOTOR_STOPPED = 5u,
    FA_MOTOR_FAULT = 6u
} FaMotorState;

typedef enum {
    FA_FAULT_NONE = 0x0000u,
    FA_FAULT_OVER_CURRENT = 0x0001u,
    FA_FAULT_STALL = 0x0002u,
    FA_FAULT_ENCODER_LOST = 0x0003u,
    FA_FAULT_RUN_TIMEOUT = 0x0004u,
    FA_FAULT_TARGET_OVERRUN = 0x0005u,
    FA_FAULT_CONFIG_INVALID = 0x0006u,
    FA_FAULT_DRIVER_ABNORMAL = 0x0007u,
    FA_FAULT_CURRENT_SENSOR = 0x0008u,
    FA_FAULT_WATCHDOG_RESET = 0x0009u,
    FA_FAULT_RESERVED_ADDRESS = 0x000Au,
    FA_FAULT_COMMAND_REJECTED = 0x000Bu,
    FA_FAULT_COMMUNICATION = 0x000Cu
} FaFaultCode;

typedef enum {
    FA_STOP_NONE = 0u,
    FA_STOP_TARGET_REACHED = 1u,
    FA_STOP_MASTER_COMMAND = 2u,
    FA_STOP_OVER_CURRENT = 3u,
    FA_STOP_STALL = 4u,
    FA_STOP_TIMEOUT = 5u,
    FA_STOP_TARGET_OVERRUN = 6u,
    FA_STOP_WATCHDOG = 7u,
    FA_STOP_LOCAL_FAULT = 8u
} FaStopReason;

typedef enum {
    FA_DEVICE_TYPE_FEEDER = 1u,
    FA_DEVICE_TYPE_DOOR = 2u
} FaDeviceType;

typedef enum {
    FA_ACTION_TYPE_FEED = 1u,
    FA_ACTION_TYPE_DOOR_OPEN = 2u,
    FA_ACTION_TYPE_DOOR_CLOSE = 3u,
    FA_ACTION_TYPE_CALIBRATION_MOVE = 4u
} FaActionType;

typedef enum {
    FA_TARGET_MODE_RELATIVE_PULSES = 1u,
    FA_TARGET_MODE_ABSOLUTE_POSITION = 2u
} FaTargetMode;

typedef struct {
    uint8_t version;
    uint8_t flags;
    uint8_t dst;
    uint8_t src;
    uint8_t seq;
    uint8_t cmd;
    uint8_t len;
    uint8_t payload[FA_MAX_PAYLOAD_LEN];
} FaFrame;

typedef struct {
    uint8_t buffer[FA_MAX_FRAME_LEN];
    size_t len;
    size_t expected_len;
} FaFrameParser;

int fa_address_is_normal(uint8_t address);
size_t fa_frame_encoded_len(uint8_t payload_len);
FaFrameResult fa_frame_encode(const FaFrame *frame, uint8_t *out, size_t out_cap, size_t *out_len);
FaFrameResult fa_frame_decode(const uint8_t *data, size_t data_len, FaFrame *frame);
void fa_frame_parser_init(FaFrameParser *parser);
FaFrameResult fa_frame_parser_push(FaFrameParser *parser, uint8_t byte, FaFrame *frame);

#ifdef __cplusplus
}
#endif

#endif
