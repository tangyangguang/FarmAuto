CC ?= cc
CFLAGS ?= -std=c99 -Wall -Wextra -Werror

ROOT := ../..
INCLUDES := -I$(ROOT)/shared/protocol \
	-I$(ROOT)/firmware/station-stc8h/src/action \
	-I$(ROOT)/firmware/master-esp32/src/rs485 \
	-I$(ROOT)/firmware/master-esp32/src/services \
	-I$(ROOT)/tools/test_support

SOURCES := test_door_service.c \
	$(ROOT)/tools/test_support/fake_station.c \
	$(ROOT)/shared/protocol/fa_crc.c \
	$(ROOT)/shared/protocol/fa_payload.c \
	$(ROOT)/shared/protocol/fa_protocol.c \
	$(ROOT)/firmware/station-stc8h/src/action/fa_action_controller.c \
	$(ROOT)/firmware/master-esp32/src/rs485/fa_rs485_master.c \
	$(ROOT)/firmware/master-esp32/src/services/fa_door_service.c

.PHONY: test clean

test: door_service_smoke
	./door_service_smoke

door_service_smoke: $(SOURCES)
	$(CC) $(CFLAGS) $(INCLUDES) $(SOURCES) -o $@

clean:
	rm -f door_service_smoke
