CXX ?= c++
CC ?= cc

CXXFLAGS ?= -std=c++11 -Wall -Wextra -Werror
CFLAGS ?= -std=c99 -Wall -Wextra -Werror

INCLUDES = \
	-I. \
	-I../../shared/protocol \
	-I../../firmware/master-esp32/src/rs485 \
	-I../../firmware/master-esp32/src/services \
	-I../../firmware/master-esp32/src/storage

OBJS = \
	test_device_registry.o \
	fa_device_registry.o \
	fa_crc.o \
	fa_payload.o \
	fa_protocol.o

.PHONY: test clean

test: device_registry_smoke
	./device_registry_smoke

device_registry_smoke: $(OBJS)
	$(CXX) $(CXXFLAGS) $(OBJS) -o $@

test_device_registry.o: test_device_registry.cpp
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@

fa_device_registry.o: ../../firmware/master-esp32/src/storage/fa_device_registry.cpp
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@

fa_crc.o: ../../shared/protocol/fa_crc.c
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

fa_payload.o: ../../shared/protocol/fa_payload.c
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

fa_protocol.o: ../../shared/protocol/fa_protocol.c
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

clean:
	rm -f $(OBJS) device_registry_smoke
