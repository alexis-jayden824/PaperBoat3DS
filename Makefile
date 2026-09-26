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
APP_DESCRIPTION := M1 reproducible 3DS bootstrap
APP_AUTHOR      := PaperBoat3DS Refolded contributors

PB3DS_BUILD_SHA ?= unknown
PB3DS_BUILD_UTC ?= unknown
HOST_CC         ?= cc
PACKAGING_RSF   := packaging/PaperBoat3DS-Refolded.rsf
MAKEROM         ?= makerom
STRIP           := $(DEVKITARM)/bin/arm-none-eabi-strip

ARCH     := -march=armv6k -mtune=mpcore -mfloat-abi=hard -mtp=soft
CFLAGS   := -g -Wall -Wextra -Werror -O2 -mword-relocations \
            -ffunction-sections $(ARCH) $(INCLUDE) -D__3DS__ \
            -DPB3DS_BUILD_SHA=\"$(PB3DS_BUILD_SHA)\" \
            -DPB3DS_BUILD_UTC=\"$(PB3DS_BUILD_UTC)\"
CXXFLAGS := $(CFLAGS) -fno-rtti -fno-exceptions -std=gnu++17
ASFLAGS  := -g $(ARCH)
LDFLAGS  := -specs=3dsx.specs -g $(ARCH) -Wl,-Map,$(notdir $*.map)
LIBS     := -lctru -lm
LIBDIRS  := $(CTRULIB)

ifneq ($(BUILD),$(notdir $(CURDIR)))

export OUTPUT  := $(CURDIR)/$(TARGET)
export TOPDIR  := $(CURDIR)
export VPATH   := $(foreach dir,$(SOURCES),$(CURDIR)/$(dir))
export DEPSDIR := $(CURDIR)/$(BUILD)

CFILES   := $(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.c)))
CPPFILES := $(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.cpp)))
SFILES   := $(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.s)))

ifeq ($(strip $(CPPFILES)),)
export LD := $(CC)
else
export LD := $(CXX)
endif

export OFILES_SOURCES := $(CPPFILES:.cpp=.o) $(CFILES:.c=.o) $(SFILES:.s=.o)
export OFILES         := $(OFILES_SOURCES)
export INCLUDE        := $(foreach dir,$(INCLUDES),-I$(CURDIR)/$(dir)) \
                         $(foreach dir,$(LIBDIRS),-I$(dir)/include) \
                         -I$(CURDIR)/$(BUILD)
export LIBPATHS       := $(foreach dir,$(LIBDIRS),-L$(dir)/lib)
export _3DSXDEPS      := $(OUTPUT).smdh

.PHONY: all packages bootstrap-test m1-lock-test clean

all: $(BUILD)
	@$(MAKE) --no-print-directory -C $(BUILD) -f $(CURDIR)/Makefile

packages: all
	@command -v $(MAKEROM) >/dev/null || { echo "makerom was not found in PATH"; exit 1; }
	@echo stripping $(TARGET).elf
	@$(STRIP) -o $(TARGET)-stripped.elf $(TARGET).elf
	@echo building $(TARGET).3ds
	@$(MAKEROM) -f cci -o $(TARGET).3ds -rsf $(PACKAGING_RSF) -target t \
		-exefslogo -elf $(TARGET)-stripped.elf -icon $(TARGET).smdh
	@test -s $(TARGET).3ds

bootstrap-test:
	@HOST_CC="$(HOST_CC)" sh tools/test_bootstrap.sh "$(BUILD)/m0-tests"

m1-lock-test:
	@sh tools/test_m1.sh "$(BUILD)/m1-tests"

$(BUILD):
	@mkdir -p $@

clean:
	@echo clean ...
	@rm -fr $(BUILD) $(TARGET).3dsx $(TARGET).3ds $(TARGET).cia \
		$(TARGET)-stripped.elf $(TARGET).smdh $(TARGET).elf $(TARGET).map

else

$(OUTPUT).3dsx: $(OUTPUT).elf $(_3DSXDEPS)
$(OUTPUT).elf: $(OFILES)

-include $(DEPSDIR)/*.d

endif
