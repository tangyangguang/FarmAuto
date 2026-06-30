CC ?= cc
CFLAGS ?= -std=c99 -Wall -Wextra -Werror \
	-I../../shared/protocol \
	-I../../firmware/station-stc8h/src/action

SRCS = \
	test_action_controller.c \
	../../firmware/station-stc8h/src/action/fa_action_controller.c

.PHONY: test clean

test: action_smoke
	./action_smoke

action_smoke: $(SRCS)
	$(CC) $(CFLAGS) $(SRCS) -o $@

clean:
	rm -f action_smoke
