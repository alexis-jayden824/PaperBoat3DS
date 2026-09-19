# PaperBoat3DS bootstrap Makefile, based on devkitPro's maintained 3DS examples.
.SUFFIXES:

ifeq ($(strip $(DEVKITARM)),)
$(error "DEVKITARM is not set. Install devkitPro's 3ds-dev toolchain and export DEVKITARM")
endif

TOPDIR ?= $(CURDIR)
include $(DEVKITARM)/3ds_rules

TARGET          := PaperBoat3DS
BUILD           := build
SOURCES         := source
DATA            := data
INCLUDES        := include
ROMFS           := romfs

APP_TITLE       := PaperBoat3DS
APP_DESCRIPTION := PaperBoat native-port bootstrap
APP_AUTHOR      := PaperBoat3DS contributors

PB3DS_BUILD_SHA ?= unknown
PB3DS_BUILD_UTC ?= unknown

PACKAGING_RSF   := packaging/PaperBoat3DS.rsf
MAKEROM         ?= makerom
STRIP           := $(DEVKITARM)/bin/arm-none-eabi-strip
UPSTREAM_ROOT   ?= $(CURDIR)/.cache/upstream
PAPERBOAT_ROOT  := $(UPSTREAM_ROOT)/PaperBoat
M5_BUILD        := $(CURDIR)/build/m5-core

ARCH     := -march=armv6k -mtune=mpcore -mfloat-abi=hard -mtp=soft
CFLAGS   := -g -Wall -Wextra -Werror -O2 -mword-relocations \
            -ffunction-sections $(ARCH) $(INCLUDE) -D__3DS__ \
            -DPB3DS_BUILD_SHA=\"$(PB3DS_BUILD_SHA)\" \
            -DPB3DS_BUILD_UTC=\"$(PB3DS_BUILD_UTC)\"
CXXFLAGS := $(CFLAGS) -fno-rtti -fno-exceptions -std=gnu++17
ASFLAGS  := -g $(ARCH)
LDFLAGS  := -specs=3dsx.specs -g $(ARCH) -Wl,-Map,$(notdir $*.map)
LIBS     := -lcitro2d -lcitro3d -lctru -lm
LIBDIRS  := $(CTRULIB)

ifneq ($(BUILD),$(notdir $(CURDIR)))

export OUTPUT  := $(CURDIR)/$(TARGET)
export TOPDIR  := $(CURDIR)
export VPATH   := $(foreach dir,$(SOURCES),$(CURDIR)/$(dir)) \
                  $(foreach dir,$(DATA),$(CURDIR)/$(dir))
export DEPSDIR := $(CURDIR)/$(BUILD)

CFILES   := $(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.c)))
CPPFILES := $(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.cpp)))
SFILES   := $(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.s)))
BINFILES := $(foreach dir,$(DATA),$(notdir $(wildcard $(dir)/*.*)))

ifeq ($(strip $(CPPFILES)),)
export LD := $(CC)
else
export LD := $(CXX)
endif

export OFILES_SOURCES := $(CPPFILES:.cpp=.o) $(CFILES:.c=.o) $(SFILES:.s=.o)
export OFILES_BIN     := $(addsuffix .o,$(BINFILES))
export OFILES         := $(OFILES_BIN) $(OFILES_SOURCES)
export HFILES         := $(addsuffix .h,$(subst .,_,$(BINFILES)))
export INCLUDE        := $(foreach dir,$(INCLUDES),-I$(CURDIR)/$(dir)) \
                         $(foreach dir,$(LIBDIRS),-I$(dir)/include) \
                         -I$(CURDIR)/$(BUILD)
export LIBPATHS       := $(foreach dir,$(LIBDIRS),-L$(dir)/lib)
export _3DSXDEPS      := $(OUTPUT).smdh
export _3DSXFLAGS     += --smdh=$(OUTPUT).smdh --romfs=$(CURDIR)/$(ROMFS)

.PHONY: all packages fetch-upstream m5-core-check clean

all: $(BUILD)
	@$(MAKE) --no-print-directory -C $(BUILD) -f $(CURDIR)/Makefile

packages: all
	@command -v $(MAKEROM) >/dev/null || { echo "makerom was not found in PATH"; exit 1; }
	@echo stripping $(TARGET).elf
	@$(STRIP) -o $(TARGET)-stripped.elf $(TARGET).elf
	@echo building $(TARGET).3ds
	@$(MAKEROM) -f cci -o $(TARGET).3ds -rsf $(PACKAGING_RSF) -target t \
		-exefslogo -elf $(TARGET)-stripped.elf -icon $(TARGET).smdh
	@echo building $(TARGET).cia
	@$(MAKEROM) -f cia -o $(TARGET).cia -rsf $(PACKAGING_RSF) -target t \
		-exefslogo -elf $(TARGET)-stripped.elf -icon $(TARGET).smdh

fetch-upstream:
	@sh tools/fetch_upstream.sh "$(UPSTREAM_ROOT)"

m5-core-check: fetch-upstream
	@mkdir -p "$(M5_BUILD)"
	@echo cross-compiling pinned PaperBoat M5 core slice
	@$(CC) -c "$(PAPERBOAT_ROOT)/src/port/decode_yay0.c" \
		-o "$(M5_BUILD)/decode_yay0.o" \
		$(ARCH) -mword-relocations -ffunction-sections -fdata-sections \
		-O2 -std=gnu11 -Wall -Wextra -Werror -D__3DS__ \
		-D_LANGUAGE_C -DPORT -DMODERN_COMPILER \
		-I"$(CURDIR)/include" \
		-I"$(PAPERBOAT_ROOT)/include" \
		-I"$(PAPERBOAT_ROOT)/external/libultraship/include"
	@$(CC) -c "$(PAPERBOAT_ROOT)/src/port/libc_compat.c" \
		-o "$(M5_BUILD)/libc_compat.o" \
		$(ARCH) -mword-relocations -ffunction-sections -fdata-sections \
		-O2 -std=gnu11 -Wall -Wextra -Werror -D__3DS__
	@$(CC) -c "$(PAPERBOAT_ROOT)/src/main_pre.c" \
		-o "$(M5_BUILD)/main_pre.o" \
		$(ARCH) -mword-relocations -ffunction-sections -fdata-sections \
		-O2 -std=gnu11 -Wall -Wextra -Werror -Wno-error -D__3DS__ \
		-D_LANGUAGE_C -DPORT -DMODERN_COMPILER -DVERSION=us -DVERSION_US \
		-DF3DEX_GBI_2 -D__CTX__ -DSPDLOG_ACTIVE_LEVEL=0 \
		-I"$(CURDIR)/include" \
		-I"$(PAPERBOAT_ROOT)/include" \
		-I"$(PAPERBOAT_ROOT)/src" \
		-I"$(PAPERBOAT_ROOT)/external/libultraship/include"
	@test -s "$(M5_BUILD)/decode_yay0.o"
	@test -s "$(M5_BUILD)/libc_compat.o"
	@test -s "$(M5_BUILD)/main_pre.o"

$(BUILD):
	@mkdir -p $@

clean:
	@echo clean ...
	@rm -fr $(BUILD) $(TARGET).3dsx $(TARGET).3ds $(TARGET).cia \
		$(TARGET)-stripped.elf $(TARGET).smdh $(TARGET).elf $(TARGET).map

else

$(OUTPUT).3dsx: $(OUTPUT).elf $(_3DSXDEPS)
$(OFILES_SOURCES): $(HFILES)
$(OUTPUT).elf: $(OFILES)

%.bin.o %_bin.h: %.bin
	@echo $(notdir $<)
	@$(bin2o)

-include $(DEPSDIR)/*.d

endif
