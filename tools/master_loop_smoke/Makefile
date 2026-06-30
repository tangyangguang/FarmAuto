CC ?= cc
CFLAGS ?= -std=c99 -Wall -Wextra -Werror \
	-I../../shared/protocol \
	-I../../firmware/station-stc8h/src/action \
	-I../../firmware/master-esp32/src/rs485 \
	-I../test_support

SRCS = \
	test_master_loop.c \
	../test_support/fake_station.c \
	../../shared/protocol/fa_crc.c \
	../../shared/protocol/fa_payload.c \
	../../shared/protocol/fa_protocol.c \
	../../firmware/station-stc8h/src/action/fa_action_controller.c \
	../../firmware/master-esp32/src/rs485/fa_rs485_master.c

.PHONY: test clean

test: master_loop_smoke
	./master_loop_smoke

master_loop_smoke: $(SRCS)
	$(CC) $(CFLAGS) $(SRCS) -o $@

clean:
	rm -f master_loop_smoke
