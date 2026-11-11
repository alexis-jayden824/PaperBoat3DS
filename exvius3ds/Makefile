# Exvius3DS host-side build (S0).
# New 3DS cross builds land in later sprints; this Makefile runs recovery unit tests on the PC.

ROOT := $(abspath .)
INC  := $(ROOT)/include
SRC  := $(ROOT)/source
TST  := $(ROOT)/tests
OUT  := $(ROOT)/build/host

CC      ?= gcc
CFLAGS  ?= -std=c11 -Wall -Wextra -Werror -O2
CPPFLAGS += -I$(INC)

.PHONY: all test clean

all: test

$(OUT):
	mkdir -p $(OUT)

$(OUT)/crypto_envelope.o: $(SRC)/crypto_envelope.c $(INC)/ex3/crypto_envelope.h | $(OUT)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

$(OUT)/test_crypto_envelope: $(TST)/test_crypto_envelope.c $(OUT)/crypto_envelope.o
	$(CC) $(CPPFLAGS) $(CFLAGS) $^ -o $@

test: $(OUT)/test_crypto_envelope
	$(OUT)/test_crypto_envelope

clean:
	rm -rf $(OUT)
