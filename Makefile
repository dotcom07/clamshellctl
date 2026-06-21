CC ?= clang
PREFIX ?= /usr/local
BINDIR ?= $(PREFIX)/bin
BUILD_DIR ?= build
TARGET := $(BUILD_DIR)/clamshellctl
VERSION ?= dev
APP_NAME := ClamshellCtl
APP := $(BUILD_DIR)/$(APP_NAME).app
APP_CONTENTS := $(APP)/Contents
APP_MACOS := $(APP_CONTENTS)/MacOS
APP_RESOURCES := $(APP_CONTENTS)/Resources
APP_TARGET := $(APP_MACOS)/$(APP_NAME)

CFLAGS ?= -std=c11 -Wall -Wextra -Wpedantic -Wno-deprecated-declarations -O2
CPPFLAGS += -DCLAMSHELLCTL_VERSION=\"$(VERSION)\"
LDFLAGS ?=
LDLIBS := -framework ApplicationServices \
	-framework CoreAudio \
	-framework CoreFoundation \
	-framework IOKit \
	-weak_framework CoreDisplay \
	-F/System/Library/PrivateFrameworks \
	-weak_framework DisplayServices

.PHONY: all app clean install uninstall

all: $(TARGET)

app: $(APP_TARGET)

$(TARGET): src/clamshellctl.c | $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(LDFLAGS) $< $(LDLIBS) -o $@

$(APP_TARGET): gui/ClamshellCtlApp.swift gui/Info.plist gui/Assets/ClamshellCtl.icns gui/Assets/Twemoji_1f41a.png gui/Assets/Twemoji_1f41a.svg NOTICE $(TARGET)
	rm -rf "$(APP)"
	install -d "$(APP_MACOS)" "$(APP_RESOURCES)"
	cp gui/Info.plist "$(APP_CONTENTS)/Info.plist"
	cp gui/Assets/ClamshellCtl.icns "$(APP_RESOURCES)/ClamshellCtl.icns"
	cp gui/Assets/Twemoji_1f41a.png "$(APP_RESOURCES)/Twemoji_1f41a.png"
	cp gui/Assets/Twemoji_1f41a.svg "$(APP_RESOURCES)/Twemoji_1f41a.svg"
	cp NOTICE "$(APP_RESOURCES)/NOTICE"
	cp "$(TARGET)" "$(APP_RESOURCES)/clamshellctl"
	swiftc -parse-as-library -O -framework AppKit -framework IOKit gui/ClamshellCtlApp.swift -o "$(APP_TARGET)"
	codesign --force --sign - "$(APP)" >/dev/null 2>&1 || true

$(BUILD_DIR):
	mkdir -p $@

install: $(TARGET)
	install -d "$(DESTDIR)$(BINDIR)"
	install -m 0755 "$(TARGET)" "$(DESTDIR)$(BINDIR)/clamshellctl"

uninstall:
	rm -f "$(DESTDIR)$(BINDIR)/clamshellctl"

clean:
	rm -rf "$(BUILD_DIR)"
