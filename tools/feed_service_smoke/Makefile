CC ?= cc
CFLAGS ?= -std=c99 -Wall -Wextra -Werror \
	-I../../shared/protocol \
	-I../../firmware/station-stc8h/src/action \
	-I../../firmware/master-esp32/src/rs485 \
	-I../../firmware/master-esp32/src/services \
	-I../test_support

SRCS = \
	test_feed_service.c \
	../test_support/fake_station.c \
	../../shared/protocol/fa_crc.c \
	../../shared/protocol/fa_payload.c \
	../../shared/protocol/fa_protocol.c \
	../../firmware/station-stc8h/src/action/fa_action_controller.c \
	../../firmware/master-esp32/src/rs485/fa_rs485_master.c \
	../../firmware/master-esp32/src/services/fa_feed_service.c

.PHONY: test clean

test: feed_service_smoke
	./feed_service_smoke

feed_service_smoke: $(SRCS)
	$(CC) $(CFLAGS) $(SRCS) -o $@

clean:
	rm -f feed_service_smoke
