# Emby5's independently developed PS5 updater payload.
# Uses the PS5 Payload SDK's normal payload entry point, not the app-title CRT.
ifndef PS5_PAYLOAD_SDK
PS5_PAYLOAD_SDK := /opt/ps5-payload-sdk
endif
include $(PS5_PAYLOAD_SDK)/toolchain/prospero.mk

OUTPUT ?= ../build/update-helper.elf
WORK := ../build/update-helper-obj
CXXFLAGS := -std=c++17 -O2 -Wall -Wextra -fno-exceptions -fno-rtti -gno-record-command-line -I../src
LDFLAGS := -Wl,--build-id=none

.PHONY: all clean
all: $(OUTPUT)

$(WORK)/helper.o: main.cpp ../src/app/update_install.h
	@mkdir -p "$(WORK)"
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(WORK)/install.o: ../src/app/update_install.cpp ../src/app/update_install.h
	@mkdir -p "$(WORK)"
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(OUTPUT): $(WORK)/helper.o $(WORK)/install.o
	@mkdir -p "$(@D)"
	$(CXX) $(LDFLAGS) -o "$@" $^

clean:
	rm -rf "$(WORK)"
