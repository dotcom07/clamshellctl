CC ?= clang
PREFIX ?= /usr/local
BINDIR ?= $(PREFIX)/bin
BUILD_DIR ?= build
TARGET := $(BUILD_DIR)/brightness_m4

CFLAGS ?= -std=c11 -Wall -Wextra -Wpedantic -Wno-deprecated-declarations -O2
CPPFLAGS ?=
LDFLAGS ?=
LDLIBS := -framework ApplicationServices \
	-framework CoreAudio \
	-framework CoreFoundation \
	-framework IOKit \
	-weak_framework CoreDisplay \
	-F/System/Library/PrivateFrameworks \
	-weak_framework DisplayServices

.PHONY: all clean install uninstall

all: $(TARGET)

$(TARGET): src/brightness_m4.c | $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(LDFLAGS) $< $(LDLIBS) -o $@

$(BUILD_DIR):
	mkdir -p $@

install: $(TARGET)
	install -d "$(DESTDIR)$(BINDIR)"
	install -m 0755 "$(TARGET)" "$(DESTDIR)$(BINDIR)/brightness_m4"

uninstall:
	rm -f "$(DESTDIR)$(BINDIR)/brightness_m4"

clean:
	rm -rf "$(BUILD_DIR)"
