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
HOST_CC         ?= cc
HOST_CXX        ?= c++
STRIP           := $(DEVKITARM)/bin/arm-none-eabi-strip
UPSTREAM_ROOT   ?= $(TOPDIR)/.cache/upstream
PAPERBOAT_ROOT  := $(UPSTREAM_ROOT)/PaperBoat
TORCH_ZLIB_ROOT := $(PAPERBOAT_ROOT)/external/torch/lib/StormLib/src/zlib
ZLIB_CFILES     := adler32.c inffast.c inflate.c inftrees.c zutil.c
ZLIB_OFILES     := $(ZLIB_CFILES:.c=.o)
M5_BUILD        := $(CURDIR)/build/m5-core
M13_RUNTIME_BUILD := $(TOPDIR)/build/m13-runtime
M13_RUNTIME_LIB := $(M13_RUNTIME_BUILD)/libpaperboat-m13.a
M13_RUNTIME_AR  := $(DEVKITARM)/bin/arm-none-eabi-ar
ARCH            := -march=armv6k -mtune=mpcore -mfloat-abi=hard -mtp=soft
M5_GAME_SOURCES := \
	src/main_pre.c \
	src/43F0.c \
	src/evt/evt.c \
	src/state_title_screen.c \
	src/battle/camera.c \
	src/entity/Switch.c \
	src/world/area_mac/mac_00/settings.c \
	src/world/area_mac/mac_00/main.c
M5_GAME_CFLAGS  := $(ARCH) -mword-relocations -ffunction-sections -fdata-sections \
	-O2 -std=gnu11 -Wall -Wextra \
	-Wno-implicit-function-declaration -Wno-int-conversion \
	-Wno-error=incompatible-pointer-types \
	-Wno-initializer-overrides -Wno-return-mismatch -D__3DS__ \
	-D_LANGUAGE_C -DPORT -DMODERN_COMPILER -DVERSION=us -DVERSION_US \
	-DF3DEX_GBI_2 -D__CTX__ -DSPDLOG_ACTIVE_LEVEL=0 \
	-I"$(TOPDIR)/include" -I"$(PAPERBOAT_ROOT)/include" \
	-I"$(PAPERBOAT_ROOT)/src" -I"$(PAPERBOAT_ROOT)/src/port" \
	-I"$(PAPERBOAT_ROOT)/external/libultraship/include"

CFLAGS   := -g -Wall -Wextra -Werror -O2 -mword-relocations \
            -ffunction-sections $(ARCH) $(INCLUDE) -D__3DS__ \
            -DPB3DS_BUILD_SHA=\"$(PB3DS_BUILD_SHA)\" \
            -DPB3DS_BUILD_UTC=\"$(PB3DS_BUILD_UTC)\"
CXXFLAGS := $(CFLAGS) -Wno-unused-parameter -fno-rtti -fno-exceptions \
            -std=gnu++17
ASFLAGS  := -g $(ARCH)
LDFLAGS  := -specs=3dsx.specs -g $(ARCH) -Wl,-Map,$(notdir $*.map) \
            -Wl,--undefined=boot_main
LIBS     := $(M13_RUNTIME_LIB) -lcitro2d -lcitro3d -lctru -lm
LIBDIRS  := $(CTRULIB)

ifneq ($(BUILD),$(notdir $(CURDIR)))

export OUTPUT  := $(CURDIR)/$(TARGET)
export TOPDIR  := $(CURDIR)
export VPATH   := $(foreach dir,$(SOURCES),$(CURDIR)/$(dir)) \
                  $(foreach dir,$(DATA),$(CURDIR)/$(dir)) \
                  $(TORCH_ZLIB_ROOT)
export DEPSDIR := $(CURDIR)/$(BUILD)

CFILES   := $(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.c))) \
            $(ZLIB_CFILES)
CPPFILES := $(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.cpp)))
SFILES   := $(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.s)))
PICAFILES := $(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.v.pica)))
BINFILES := $(foreach dir,$(DATA),$(notdir $(wildcard $(dir)/*.*)))

ifeq ($(strip $(CPPFILES)),)
export LD := $(CC)
else
export LD := $(CXX)
endif

export OFILES_SOURCES := $(CPPFILES:.cpp=.o) $(CFILES:.c=.o) $(SFILES:.s=.o)
export OFILES_BIN     := $(addsuffix .o,$(BINFILES)) \
                         $(PICAFILES:.v.pica=.shbin.o)
export OFILES         := $(OFILES_BIN) $(OFILES_SOURCES)
export HFILES         := $(addsuffix .h,$(subst .,_,$(BINFILES))) \
                         $(PICAFILES:.v.pica=_shbin.h)
export INCLUDE        := $(foreach dir,$(INCLUDES),-I$(CURDIR)/$(dir)) \
                         -I$(PAPERBOAT_ROOT)/external/libultraship/include \
                         -I$(TORCH_ZLIB_ROOT) \
                         $(foreach dir,$(LIBDIRS),-I$(dir)/include) \
                         -I$(CURDIR)/$(BUILD)
export LIBPATHS       := $(foreach dir,$(LIBDIRS),-L$(dir)/lib)
export _3DSXDEPS      := $(OUTPUT).smdh
export _3DSXFLAGS     += --smdh=$(OUTPUT).smdh --romfs=$(CURDIR)/$(ROMFS)

.PHONY: all packages fetch-upstream m5-core-check m13-runtime-lib \
	m6-policy-test \
	m6-budget-check m7-assets-test m8-input-test m9-renderer-test \
	m10-graphics-test \
	m11-frame-test m12-flow-test m12-layout-test m13-world-test \
	m13-core-check clean

all: fetch-upstream m13-runtime-lib $(BUILD)
	@$(MAKE) --no-print-directory -C $(BUILD) -f $(CURDIR)/Makefile
	@NM="$(DEVKITARM)/bin/arm-none-eabi-nm" \
		sh tools/check_m13_heap_alignment.sh "$(TARGET).elf"
	@set -e; for symbol in boot_main step_game_loop gfx_draw_frame \
		Graphics_ThreadUpdate update_player update_player_input \
		update_cameras pb_runtime_begin_toad_town \
		pb_runtime_continue_startup Graphics_PushFrame is_debug_panic; do \
		$(DEVKITARM)/bin/arm-none-eabi-nm --defined-only "$(TARGET).elf" | \
			awk '{ print $$3 }' | grep -qx "$$symbol" || { \
				echo "missing final runtime symbol: $$symbol"; exit 1; \
			}; \
	done

m6-policy-test:
	@HOST_CC="$(HOST_CC)" sh tools/test_memory_policy.sh "$(BUILD)/m6-tests"

m7-assets-test:
	@sh tools/test_asset_pipeline.sh

m6-budget-check: all
	@SIZE="$(DEVKITARM)/bin/arm-none-eabi-size" \
		sh tools/check_memory_budget.sh "$(TARGET).elf" \
		"$(BUILD)/memory-budget.txt"

m8-input-test:
	@HOST_CC="$(HOST_CC)" sh tools/test_input_backend.sh "$(BUILD)/m8-tests"

m9-renderer-test:
	@HOST_CC="$(HOST_CC)" sh tools/test_renderer_contract.sh "$(BUILD)/m9-tests"

m10-graphics-test: fetch-upstream
	@HOST_CC="$(HOST_CC)" sh tools/test_gfx_bridge.sh "$(BUILD)/m10-tests"
	@HOST_CC="$(HOST_CC)" HOST_CXX="$(HOST_CXX)" \
		sh tools/test_gfx_api_contract.sh "$(BUILD)/m10-api-tests"

m11-frame-test: fetch-upstream
	@HOST_CC="$(HOST_CC)" sh tools/test_first_frame.sh "$(BUILD)/m11-tests"

m12-flow-test: fetch-upstream
	@HOST_CC="$(HOST_CC)" sh tools/test_title_flow.sh "$(BUILD)/m12-tests"

m12-layout-test:
	@HOST_CC="$(HOST_CC)" sh tools/test_title_layout.sh \
		"$(BUILD)/m12-layout-tests"

m13-world-test: fetch-upstream
	@python3 tests/test_m13_runtime_generation.py \
		tools/generate_m13_runtime.py "$(PAPERBOAT_ROOT)"
	@HOST_CC="$(HOST_CC)" sh tools/test_world_boot.sh \
		"$(BUILD)/m13-tests"
	@HOST_CC="$(HOST_CC)" sh tools/test_world_scene.sh \
		"$(BUILD)/m13-scene-tests"

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
	@set -e; for source in $(M5_GAME_SOURCES); do \
		object=$$(printf '%s' "$$source" | tr '/.' '__'); \
		echo "  ARM11 $$source"; \
		$(CC) -c "$(PAPERBOAT_ROOT)/$$source" \
			-o "$(M5_BUILD)/$$object.o" $(M5_GAME_CFLAGS); \
		test -s "$(M5_BUILD)/$$object.o"; \
	done
	@test -s "$(M5_BUILD)/decode_yay0.o"
	@test -s "$(M5_BUILD)/libc_compat.o"

m13-runtime-lib: $(M13_RUNTIME_LIB)

$(M13_RUNTIME_LIB): tools/build_m13_runtime.sh \
		tools/generate_m13_runtime.py upstream/M13_RUNTIME_SOURCES.txt \
		| fetch-upstream
	@echo cross-compiling pinned PaperBoat M13 runtime closure
	@sh tools/build_m13_runtime.sh "$(PAPERBOAT_ROOT)" \
		"$(M13_RUNTIME_BUILD)" "$(CC)" "$(M13_RUNTIME_AR)" \
		$(M5_GAME_CFLAGS)

m13-core-check: m13-runtime-lib
	@echo verifying linked PaperBoat runtime entry, player, and camera units
	@set -e; for symbol in boot_main step_game_loop gfx_draw_frame \
		Graphics_ThreadUpdate state_step_world update_player \
		update_player_input update_cameras; do \
		$(DEVKITARM)/bin/arm-none-eabi-nm \
			--defined-only "$(M13_RUNTIME_LIB)" | \
			awk '{ print $$3 }' | grep -qx "$$symbol" || { \
				echo "missing linked upstream runtime symbol: $$symbol"; \
				exit 1; \
			}; \
	done
	@set -e; for symbol in printf puts __printf_chk is_debug_panic; do \
		if $(DEVKITARM)/bin/arm-none-eabi-nm \
			--defined-only "$(M13_RUNTIME_LIB)" | \
			awk '{ print $$3 }' | grep -qx "$$symbol"; then \
			echo "runtime archive must not interpose libc symbol: $$symbol"; \
			exit 1; \
		fi; \
	done
	@$(DEVKITARM)/bin/arm-none-eabi-nm \
		--defined-only "$(M13_RUNTIME_LIB)" | \
		awk '{ print $$3 }' | grep -qx pb_upstream_is_debug_panic || { \
		echo "missing namespaced upstream panic symbol"; exit 1; \
	}
	@test -s "$(M13_RUNTIME_LIB)"
	@mkdir -p "$(M13_RUNTIME_BUILD)"
	@$(CC) -c tests/test_runtime_upstream_consumer.c \
		-o "$(M13_RUNTIME_BUILD)/resource-abi.o" $(M5_GAME_CFLAGS)

$(BUILD):
	@mkdir -p $@

clean:
	@echo clean ...
	@rm -fr $(BUILD) $(TARGET).3dsx $(TARGET).3ds $(TARGET).cia \
		$(TARGET)-stripped.elf $(TARGET).smdh $(TARGET).elf $(TARGET).map

else

runtime_platform.o: CFLAGS += -D_LANGUAGE_C -DPORT -DMODERN_COMPILER \
	-DVERSION=us -DVERSION_US -DF3DEX_GBI_2 -D__CTX__ \
	-DSPDLOG_ACTIVE_LEVEL=0 -I$(PAPERBOAT_ROOT)/include \
	-I$(PAPERBOAT_ROOT)/src -I$(PAPERBOAT_ROOT)/src/port -Wno-error

$(ZLIB_OFILES): CFLAGS += -DNO_GZIP -Wno-endif-labels \
	-Wno-shift-negative-value -Wno-implicit-fallthrough \
	-Wno-old-style-definition

$(OUTPUT).3dsx: $(OUTPUT).elf $(_3DSXDEPS)
$(OFILES_SOURCES): $(HFILES)
$(OUTPUT).elf: $(OFILES) $(M13_RUNTIME_LIB)

%.bin.o %_bin.h: %.bin
	@echo $(notdir $<)
	@$(bin2o)

.PRECIOUS: %.shbin
%.shbin.o %_shbin.h: %.shbin
	@echo $(notdir $<)
	@$(bin2o)

-include $(DEPSDIR)/*.d

endif
