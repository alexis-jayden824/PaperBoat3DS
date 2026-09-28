# PaperBoat3DS Refolded M0 Makefile, based on maintained 3ds-examples.
.SUFFIXES:

ifeq ($(strip $(DEVKITARM)),)
$(error "DEVKITARM is not set. Install the 3ds-dev package and export DEVKITARM")
endif

TOPDIR ?= $(CURDIR)
include $(DEVKITARM)/3ds_rules

TARGET          := PaperBoat3DS-Refolded
BUILD           := build
SOURCES         := source
INCLUDES        := include

APP_TITLE       := PaperBoat3DS Refolded
APP_DESCRIPTION := M13 runtime
APP_AUTHOR      := PaperBoat3DS Refolded contributors

PB3DS_BUILD_SHA ?= unknown
PB3DS_BUILD_UTC ?= unknown
HOST_CC         ?= cc
PACKAGING_RSF   := packaging/PaperBoat3DS-Refolded.rsf
MAKEROM         ?= makerom
STRIP           := $(DEVKITARM)/bin/arm-none-eabi-strip
PAPERBOAT_COMMIT := $(shell awk -F= '/^PAPERBOAT_COMMIT=/ { print $$2 }' $(TOPDIR)/upstream/PAPERBOAT.lock)
PAPERBOAT_RELEASE := $(shell awk -F= '/^PAPERBOAT_RELEASE=/ { print $$2 }' $(TOPDIR)/upstream/PAPERBOAT.lock)
PAPERBOAT_ROOT ?= $(TOPDIR)/.cache/upstream/PaperBoat
M13_GAME_BUILD := $(TOPDIR)/build/m13-game
M13_GAME_CFLAGS := $(ARCH) -mword-relocations -ffunction-sections -fdata-sections \
    -O2 -std=gnu11 -Wall -Wextra -Wno-implicit-function-declaration \
    -Wno-int-conversion -Wno-error=incompatible-pointer-types \
    -Wno-initializer-overrides -Wno-return-mismatch -D__3DS__ \
    -D_LANGUAGE_C -DPORT -DMODERN_COMPILER -DVERSION=us -DVERSION_US \
    -DF3DEX_GBI_2 -D__CTX__ -DSPDLOG_ACTIVE_LEVEL=0 \
    -I"$(TOPDIR)/include" -I"$(PAPERBOAT_ROOT)/include" \
    -I"$(PAPERBOAT_ROOT)/src" -I"$(PAPERBOAT_ROOT)/src/port" \
    -I"$(PAPERBOAT_ROOT)/external/libultraship/include"

# Owner-only: compile/link PaperBoat game TUs (not CI default).
# make CFLAGS+=-DPB3DS_GAME_OBJECTS after listing sources with
# tools/list_paperboat_sources.sh
ARCH     := -march=armv6k -mtune=mpcore -mfloat-abi=hard -mtp=soft
CFLAGS   := -g -Wall -Wextra -Werror -O2 -mword-relocations \
            -ffunction-sections $(ARCH) $(INCLUDE) -D__3DS__ \
            -DPB3DS_BUILD_SHA=\"$(PB3DS_BUILD_SHA)\" \
            -DPB3DS_BUILD_UTC=\"$(PB3DS_BUILD_UTC)\" \
            -DPB3DS_PAPERBOAT_COMMIT=\"$(PAPERBOAT_COMMIT)\" \
            -DPB3DS_PAPERBOAT_RELEASE=\"$(PAPERBOAT_RELEASE)\" \
            -I$(TOPDIR)/include/pb3ds/n64shim
CXXFLAGS := $(CFLAGS) -fno-rtti -fno-exceptions -std=gnu++17
ASFLAGS  := -g $(ARCH)
LDFLAGS  := -specs=3dsx.specs -g $(ARCH) -Wl,-Map,$(notdir $*.map)
LIBS     := -lcitro3d -lctru -lm
LIBDIRS  := $(CTRULIB)

ifneq ($(BUILD),$(notdir $(CURDIR)))

export OUTPUT  := $(CURDIR)/$(TARGET)
export TOPDIR  := $(CURDIR)
export VPATH   := $(foreach dir,$(SOURCES),$(CURDIR)/$(dir)) \
                 $(CURDIR)/shaders
export DEPSDIR := $(CURDIR)/$(BUILD)

CFILES   := $(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.c)))
CPPFILES := $(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.cpp)))
SFILES   := $(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.s)))
PICAFILES := $(notdir $(wildcard shaders/*.v.pica))

ifeq ($(strip $(CPPFILES)),)
export LD := $(CC)
else
export LD := $(CXX)
endif

export OFILES_SOURCES := $(CPPFILES:.cpp=.o) $(CFILES:.c=.o) $(SFILES:.s=.o)
export OFILES_BIN     := $(PICAFILES:.v.pica=.shbin.o)
export OFILES         := $(OFILES_BIN) $(OFILES_SOURCES)
export HFILES         := $(PICAFILES:.v.pica=_shbin.h)
export INCLUDE        := $(foreach dir,$(INCLUDES),-I$(CURDIR)/$(dir)) \
                         $(foreach dir,$(LIBDIRS),-I$(dir)/include) \
                         -I$(CURDIR)/$(BUILD)
export LIBPATHS       := $(foreach dir,$(LIBDIRS),-L$(dir)/lib)
export _3DSXDEPS      := $(OUTPUT).smdh

.PHONY: all packages fetch fetch-torch fetch-game m13-game-core bootstrap-test m1-lock-test m2-audit-test \
	m3-platform-test m4-diag-test m5-slice-test m6-compat-test m7-assets-test \
	m8-input-test m9-fs-test m10-loop-test m11-gfx-test m12-title-test \
	m13-runtime-test clean

all: $(BUILD)/paperboat_config.h $(BUILD)
	@$(MAKE) --no-print-directory -C $(BUILD) -f $(CURDIR)/Makefile

packages: all
	@command -v $(MAKEROM) >/dev/null || { echo "makerom was not found in PATH"; exit 1; }
	@echo stripping $(TARGET).elf
	@$(STRIP) -o $(TARGET)-stripped.elf $(TARGET).elf
	@echo building $(TARGET).3ds
	@$(MAKEROM) -f cci -o $(TARGET).3ds -rsf $(PACKAGING_RSF) -target t \
		-exefslogo -elf $(TARGET)-stripped.elf -icon $(TARGET).smdh
	@test -s $(TARGET).3ds
	@echo building $(TARGET).cia
	@$(MAKEROM) -f cia -o $(TARGET).cia -rsf $(PACKAGING_RSF) -target t \
		-exefslogo -elf $(TARGET)-stripped.elf -icon $(TARGET).smdh
	@test -s $(TARGET).cia

bootstrap-test:
	@HOST_CC="$(HOST_CC)" sh tools/test_bootstrap.sh "$(BUILD)/m0-tests"

m1-lock-test:
	@sh tools/test_m1.sh "$(BUILD)/m1-tests"

m2-audit-test:
	@sh tools/test_m2.sh "$(BUILD)/m2-tests"

m3-platform-test:
	@HOST_CC="$(HOST_CC)" sh tools/test_platform.sh "$(BUILD)/m3-tests"

m4-diag-test:
	@HOST_CC="$(HOST_CC)" sh tools/test_m4.sh "$(BUILD)/m4-tests"

m5-slice-test:
	@HOST_CC="$(HOST_CC)" sh tools/test_m5.sh "$(BUILD)/m5-tests"

m6-compat-test:
	@HOST_CC="$(HOST_CC)" sh tools/test_m6.sh "$(BUILD)/m6-tests"

m7-assets-test:
	@HOST_CC="$(HOST_CC)" sh tools/test_m7.sh "$(BUILD)/m7-tests"

m8-input-test:
	@HOST_CC="$(HOST_CC)" sh tools/test_m8.sh "$(BUILD)/m8-tests"

m9-fs-test:
	@HOST_CC="$(HOST_CC)" sh tools/test_m9.sh "$(BUILD)/m9-tests"

m10-loop-test:
	@HOST_CC="$(HOST_CC)" sh tools/test_m10.sh "$(BUILD)/m10-tests"

m11-gfx-test:
	@HOST_CC="$(HOST_CC)" sh tools/test_m11.sh "$(BUILD)/m11-tests"

m12-title-test:
	@HOST_CC="$(HOST_CC)" sh tools/test_m12.sh "$(BUILD)/m12-tests"

m13-runtime-test:
	@HOST_CC="$(HOST_CC)" sh tools/test_m13.sh "$(BUILD)/m13-tests"

fetch-torch:
	@sh tools/fetch_torch.sh

fetch:
	@sh tools/fetch_paperboat.sh
	@mkdir -p $(BUILD)
	@sh tools/write_paperboat_config.sh "$(BUILD)/paperboat_config.h"

fetch-game:
	@PB3DS_FETCH_FULL_GAME=1 sh tools/fetch_paperboat.sh
	@sh tools/fetch_libultraship.sh
	@sh tools/write_paperboat_config.sh "$(BUILD)/paperboat_config.h"

# Staged compile gate: building the game closure does not yet link it into the
# executable. The platform ABI and startup path must pass the link gate first.
m13-game-core: fetch-game
	@sh tools/build_m13_runtime.sh "$(PAPERBOAT_ROOT)" \
		"$(M13_GAME_BUILD)" "$(DEVKITARM)/bin/arm-none-eabi-gcc" \
		"$(DEVKITARM)/bin/arm-none-eabi-ar" $(M13_GAME_CFLAGS)

$(BUILD)/paperboat_config.h:
	@mkdir -p $(BUILD)
	@sh tools/write_paperboat_config.sh "$@"

$(BUILD):
	@mkdir -p $@

clean:
	@echo clean ...
	@rm -fr $(BUILD) $(TARGET).3dsx $(TARGET).3ds $(TARGET).cia \
		$(TARGET)-stripped.elf $(TARGET).smdh $(TARGET).elf $(TARGET).map \
		$(TARGET).bnr

else

$(OUTPUT).3dsx: $(OUTPUT).elf $(_3DSXDEPS)

$(OFILES_SOURCES): $(HFILES)

$(OUTPUT).elf: $(OFILES)

%.shbin: %.v.pica
	@echo $(notdir $<)
	@picasso -o $@ $<

%.shbin.o %_shbin.h: %.shbin
	@echo $(notdir $<)
	@$(bin2o)

-include $(DEPSDIR)/*.d

endif
