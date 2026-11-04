.DEFAULT_GOAL := all
ifeq ($(strip $(DEVKITARM)),)
$(error Set DEVKITARM to your devkitPro/devkitARM directory)
endif
APP_TITLE := SaveRelay
APP_DESCRIPTION := Direct 3DS save transfer
APP_AUTHOR := SaveRelay contributors
include $(DEVKITARM)/3ds_rules

TARGET := SaveRelay
BUILD := build
MAKEROM ?= makerom
BANNERTOOL ?= bannertool
ARCH := -march=armv6k -mtune=mpcore -mfloat-abi=hard -mtp=soft
CPPFLAGS := -Iinclude -I$(CTRULIB)/include -D__3DS__
CXXFLAGS := $(ARCH) -O2 -g -Wall -Wextra -Wno-psabi -mword-relocations -ffunction-sections -fno-rtti -fno-exceptions -std=gnu++17
LDFLAGS := $(ARCH) -specs=3dsx.specs -Wl,-Map,$(BUILD)/SaveRelay.map
SOURCES := $(wildcard source/*.cpp)
OBJECTS := $(patsubst source/%.cpp,$(BUILD)/%.o,$(SOURCES))
VERSION_CODE := 1

.PHONY: all 3dsx cia clean test
all: 3dsx cia
3dsx: $(TARGET).3dsx $(TARGET).smdh
cia: $(TARGET).cia
$(BUILD):
	mkdir -p $@
$(BUILD)/%.o: source/%.cpp | $(BUILD)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -MMD -MP -c $< -o $@
$(TARGET).elf: $(OBJECTS)
	$(CXX) $(LDFLAGS) $^ -L$(CTRULIB)/lib -lctru -lm -o $@
$(BUILD)/icon.icn: assets/icon.png | $(BUILD)
	smdhtool --create "$(APP_TITLE)" "$(APP_DESCRIPTION)" "$(APP_AUTHOR)" $< $@
$(TARGET).smdh: $(BUILD)/icon.icn
	cp $< $@
$(TARGET).3dsx: $(TARGET).elf $(BUILD)/icon.icn
	3dsxtool $< $@ --smdh=$(BUILD)/icon.icn
$(BUILD)/banner.bnr: assets/banner.png assets/banner.wav | $(BUILD)
	$(BANNERTOOL) makebanner -i assets/banner.png -a assets/banner.wav -o $@
$(BUILD)/SaveRelay.cxi: $(TARGET).elf $(BUILD)/icon.icn $(BUILD)/banner.bnr assets/app.rsf
	$(MAKEROM) -f ncch -target t -exefslogo -elf $(TARGET).elf -rsf assets/app.rsf -icon $(BUILD)/icon.icn -banner $(BUILD)/banner.bnr -o $@
$(TARGET).cia: $(BUILD)/SaveRelay.cxi
	$(MAKEROM) -f cia -target t -content $<:0:0 -ver $(VERSION_CODE) -o $@
test:
	bash tests/run.sh
clean:
	rm -rf $(BUILD) $(TARGET).elf $(TARGET).3dsx $(TARGET).smdh $(TARGET).cia
-include $(OBJECTS:.o=.d)
