CC ?= cc
CFLAGS ?= -std=c99 -Wall -Wextra -Werror \
	-I../../shared/protocol \
	-I../../firmware/station-stc8h/src/action

SRCS = \
	test_fake_station.c \
	../../shared/protocol/fa_crc.c \
	../../shared/protocol/fa_payload.c \
	../../shared/protocol/fa_protocol.c \
	../../firmware/station-stc8h/src/action/fa_action_controller.c

.PHONY: test clean

test: fake_station_smoke
	./fake_station_smoke

fake_station_smoke: $(SRCS)
	$(CC) $(CFLAGS) $(SRCS) -o $@

clean:
	rm -f fake_station_smoke
