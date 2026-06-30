CC ?= cc
CFLAGS ?= -std=c99 -Wall -Wextra -Werror

ROOT := ../..
INCLUDES := \
	-I$(ROOT)/shared/protocol \
	-I$(ROOT)/firmware/station-stc8h/src/action \
	-I$(ROOT)/firmware/station-stc8h/src/protocol \
	-I$(ROOT)/firmware/master-esp32/src/rs485

SOURCES := \
	test_station_node.c \
	$(ROOT)/shared/protocol/fa_crc.c \
	$(ROOT)/shared/protocol/fa_payload.c \
	$(ROOT)/shared/protocol/fa_protocol.c \
	$(ROOT)/firmware/station-stc8h/src/action/fa_action_controller.c \
	$(ROOT)/firmware/station-stc8h/src/protocol/fa_station_node.c \
	$(ROOT)/firmware/master-esp32/src/rs485/fa_rs485_master.c

TARGET := station_node_smoke

.PHONY: test clean

test: $(TARGET)
	./$(TARGET)

$(TARGET): $(SOURCES)
	$(CC) $(CFLAGS) $(INCLUDES) $(SOURCES) -o $(TARGET)

clean:
	rm -f $(TARGET)
