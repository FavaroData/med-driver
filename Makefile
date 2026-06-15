# Cross-compilação Linux → Windows x64
# Requer: sudo apt install mingw-w64

MAKEFILE_DIR := $(dir $(abspath $(lastword $(MAKEFILE_LIST))))

CC      = x86_64-w64-mingw32-gcc
TARGET  = $(MAKEFILE_DIR)meddrivemon.dll
SRCS    = src/monitor.c
OBJS    = $(SRCS:.c=.o)
DEF     = src/monitor.def

CFLAGS  = -Wall -Wextra -O2 -municode
LDFLAGS = -shared -static-libgcc -lkernel32 -ladvapi32

all: $(TARGET)

$(TARGET): $(OBJS) $(DEF)
	$(CC) $(LDFLAGS) -o $(TARGET) $(OBJS) $(DEF)

src/%.o: src/%.c src/monitor.h
	$(CC) $(CFLAGS) -c -o $@ $<

installer: $(TARGET)
	makensis installer/win10-11/setup.nsi

installer-win7: $(TARGET)
	makensis installer/win7/setup.nsi

installers: installer installer-win7

clean:
	rm -f src/*.o $(TARGET)

.PHONY: all clean installer installer-win7 installers
