CC ?= cc
CFLAGS ?= -std=c99 -Wall -Wextra -Werror -I../../shared/protocol

SRCS = \
	test_fa_protocol.c \
	../../shared/protocol/fa_crc.c \
	../../shared/protocol/fa_payload.c \
	../../shared/protocol/fa_protocol.c

.PHONY: test clean

test: protocol_smoke
	./protocol_smoke

protocol_smoke: $(SRCS)
	$(CC) $(CFLAGS) $(SRCS) -o $@

clean:
	rm -f protocol_smoke
