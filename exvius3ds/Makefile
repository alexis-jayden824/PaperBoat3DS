# Exvius3DS — host tests always; 3DS packages via Makefile.3ds when DEVKITARM is set.

ROOT := $(abspath .)
INC  := $(ROOT)/include
SRC  := $(ROOT)/source
TST  := $(ROOT)/tests
TOOLS := $(ROOT)/tools
HOST_OUT := $(ROOT)/build/host
FIXTURES := $(ROOT)/romfs/fixtures

HOST_CC ?= gcc
HOST_CFLAGS ?= -std=c11 -Wall -Wextra -Werror -O2
EX3_BUILD_SHA ?= unknown
EX3_BUILD_UTC ?= unknown
HOST_CPPFLAGS := -I$(INC) -DEX3_HOST=1 \
	-DEX3_BUILD_SHA=\"$(EX3_BUILD_SHA)\" \
	-DEX3_BUILD_UTC=\"$(EX3_BUILD_UTC)\"

.PHONY: all test host-test fixtures clean 3ds packages

all: test

$(HOST_OUT):
	mkdir -p $(HOST_OUT)

$(FIXTURES):
	mkdir -p $(FIXTURES)

$(HOST_OUT)/%.o: $(SRC)/%.c | $(HOST_OUT)
	$(HOST_CC) $(HOST_CPPFLAGS) $(HOST_CFLAGS) -c $< -o $@

$(HOST_OUT)/bake_s3_fixtures: $(TOOLS)/bake_s3_fixtures.c \
		$(HOST_OUT)/aes128.o $(HOST_OUT)/crypto_envelope.o | $(HOST_OUT)
	$(HOST_CC) $(HOST_CPPFLAGS) $(HOST_CFLAGS) $^ -o $@

fixtures: $(HOST_OUT)/bake_s3_fixtures | $(FIXTURES)
	$(HOST_OUT)/bake_s3_fixtures $(FIXTURES)

HOST_OBJS := \
	$(HOST_OUT)/crypto_envelope.o \
	$(HOST_OUT)/aes128.o \
	$(HOST_OUT)/base64.o \
	$(HOST_OUT)/plat_common.o \
	$(HOST_OUT)/plat_host.o \
	$(HOST_OUT)/log.o \
	$(HOST_OUT)/texture.o \
	$(HOST_OUT)/cocos.o \
	$(HOST_OUT)/fs.o \
	$(HOST_OUT)/mst.o \
	$(HOST_OUT)/texpack.o \
	$(HOST_OUT)/content.o \
	$(HOST_OUT)/bootstrap.o \
	$(HOST_OUT)/jni.o \
	$(HOST_OUT)/prefs.o \
	$(HOST_OUT)/http.o \
	$(HOST_OUT)/login.o \
	$(HOST_OUT)/recovered_client_refs.o

S4_OBJS := \
	$(HOST_OUT)/crypto_envelope.o \
	$(HOST_OUT)/aes128.o \
	$(HOST_OUT)/base64.o \
	$(HOST_OUT)/jni.o \
	$(HOST_OUT)/prefs.o \
	$(HOST_OUT)/http.o \
	$(HOST_OUT)/login.o \
	$(HOST_OUT)/recovered_client_refs.o

$(HOST_OUT)/test_crypto_envelope: $(TST)/test_crypto_envelope.c $(HOST_OUT)/crypto_envelope.o
	$(HOST_CC) $(HOST_CPPFLAGS) $(HOST_CFLAGS) $^ -o $@

$(HOST_OUT)/test_aes128: $(TST)/test_aes128.c $(HOST_OUT)/aes128.o
	$(HOST_CC) $(HOST_CPPFLAGS) $(HOST_CFLAGS) $^ -o $@

$(HOST_OUT)/test_plat_host: $(TST)/test_plat_host.c $(HOST_OBJS)
	$(HOST_CC) $(HOST_CPPFLAGS) $(HOST_CFLAGS) $(TST)/test_plat_host.c $(HOST_OBJS) -o $@

$(HOST_OUT)/test_bootstrap: $(TST)/test_bootstrap.c $(HOST_OBJS)
	$(HOST_CC) $(HOST_CPPFLAGS) $(HOST_CFLAGS) $(TST)/test_bootstrap.c $(HOST_OBJS) -o $@

$(HOST_OUT)/test_cocos: $(TST)/test_cocos.c $(HOST_OBJS)
	$(HOST_CC) $(HOST_CPPFLAGS) $(HOST_CFLAGS) $(TST)/test_cocos.c $(HOST_OBJS) -o $@

$(HOST_OUT)/test_texture: $(TST)/test_texture.c $(HOST_OUT)/texture.o
	$(HOST_CC) $(HOST_CPPFLAGS) $(HOST_CFLAGS) $^ -o $@

$(HOST_OUT)/test_mst: $(TST)/test_mst.c $(HOST_OBJS)
	$(HOST_CC) $(HOST_CPPFLAGS) $(HOST_CFLAGS) $(TST)/test_mst.c $(HOST_OBJS) -o $@

$(HOST_OUT)/test_jni_link: $(TST)/test_jni_link.c $(S4_OBJS) | $(HOST_OUT)
	$(HOST_CC) $(HOST_CPPFLAGS) $(HOST_CFLAGS) $(TST)/test_jni_link.c $(S4_OBJS) -o $@

test host-test: fixtures \
		$(HOST_OUT)/test_crypto_envelope $(HOST_OUT)/test_aes128 \
		$(HOST_OUT)/test_plat_host $(HOST_OUT)/test_bootstrap \
		$(HOST_OUT)/test_cocos $(HOST_OUT)/test_texture $(HOST_OUT)/test_mst \
		$(HOST_OUT)/test_jni_link
	$(HOST_OUT)/test_crypto_envelope
	$(HOST_OUT)/test_aes128
	$(HOST_OUT)/test_plat_host
	EX3_HOST_ROOT=$(ROOT) $(HOST_OUT)/test_bootstrap
	$(HOST_OUT)/test_cocos
	$(HOST_OUT)/test_texture
	EX3_HOST_ROOT=$(ROOT) $(HOST_OUT)/test_mst
	$(HOST_OUT)/test_jni_link

clean:
	rm -rf $(HOST_OUT) $(ROOT)/build3ds $(ROOT)/build/3ds
	rm -f $(ROOT)/Exvius3DS.elf $(ROOT)/Exvius3DS.3dsx $(ROOT)/Exvius3DS.3ds \
		$(ROOT)/Exvius3DS.cia $(ROOT)/Exvius3DS.smdh $(ROOT)/Exvius3DS.map \
		$(ROOT)/Exvius3DS-stripped.elf $(ROOT)/exvius3ds-host.log

3ds packages: fixtures
ifeq ($(strip $(DEVKITARM)),)
	@echo "DEVKITARM is not set. Use CI image devkitpro/devkitarm or export DEVKITARM."
	@echo "Host proof still available: make test"
	@exit 1
else
	$(MAKE) -f Makefile.3ds $@ EX3_BUILD_SHA=$(EX3_BUILD_SHA) EX3_BUILD_UTC=$(EX3_BUILD_UTC)
endif
