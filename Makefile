# Early makefile for Casio Loopy (Kasami, August 2023)
# Based on Wonderful Toolchain package for SuperH

# Normal use: "make clean && make"

# Memory sizes; should use K/M suffix or decimal integer
# Cart battery-backed save RAM, most have at least 8K if any
# "Floopy Drive" flash cart has 128K
SRAMSIZE = 8K
# Allocated stack size, increase if necessary
STACKSIZE = 2K

# Toolchain programs
WONDERFUL_TOOLCHAIN ?= /opt/wonderful
TOOLBIN ?= $(WONDERFUL_TOOLCHAIN)/toolchain/gcc-sh-elf/bin/
#TOOLBIN =
PREFIX ?= sh-elf-
#PREFIX = sh1-none-elf-
CC  = $(TOOLBIN)$(PREFIX)gcc
CXX = $(TOOLBIN)$(PREFIX)g++
LD  = $(TOOLBIN)$(PREFIX)ld
OBJ = $(TOOLBIN)$(PREFIX)objcopy

# File manipulation progs
MV     = mv
MKDIR  = mkdir -p
RMDIR  = rm -rf
PYTHON3 = /usr/bin/env python3 # Change to "python" if necessary
FIXROM = $(PYTHON3) ./tools/fixrom.py
CONVLE = $(PYTHON3) ./tools/convert_le.py
CONVMIDI = $(PYTHON3) ./tools/convert_midi.py
CONVTILES = $(PYTHON3) ./tools/convert_tiles.py

# File/dir locations
SRCDIR = ./src
INCDIR = ./include
OBJDIR = ./obj
DATADIR = ./data
ROM    = ./rom.bin

# Basic compile options
OPTIMIZE = -Os
LIBS =
WARNINGS = -Wall -Wextra -pedantic -Werror

# Below here probably doesn't need to be touched

LDSCRIPT = ./tools/loopy.ld

# Source/object lists
SRCS_C = $(wildcard $(SRCDIR)/*.c)
OBJS_C = $(patsubst $(SRCDIR)/%.c,$(OBJDIR)/%.o,$(SRCS_C))
SRCS_CXX = $(wildcard $(SRCDIR)/*.cc)
OBJS_CXX = $(patsubst $(SRCDIR)/%.cc,$(OBJDIR)/%.o,$(SRCS_CXX))
SRCS_S = $(wildcard $(SRCDIR)/*.s)
OBJS_S = $(patsubst $(SRCDIR)/%.s,$(OBJDIR)/%.o,$(SRCS_S))
HDRS = $(wildcard $(SRCDIR)/*.h $(INCDIR)/*.h $(INCDIR)/loopy/*.h)

CFLAGS  = $(OPTIMIZE) -g -gdwarf-4
CFLAGS += -m1 -mrenesas
CFLAGS += -ffreestanding
CFLAGS += -falign-functions=4 -ffunction-sections -fdata-sections
CFLAGS += -fomit-frame-pointer -fno-asynchronous-unwind-tables -fno-unwind-tables
CFLAGS += -Wstack-usage=$(shell numfmt --from=iec $(STACKSIZE)) -I$(INCDIR)
CFLAGS += $(WARNINGS)

CXXFLAGS = -std=c++23 -fno-exceptions -fno-non-call-exceptions -fno-rtti -fno-threadsafe-statics

SIZEDEFS  = -Wl,--defsym=SRAMSIZE=$(SRAMSIZE)
SIZEDEFS += -Wl,--defsym=STACKSIZE=$(STACKSIZE)

LDFLAGS  = -nostartfiles -nolibc -Wl,--gc-sections -Wl,--no-warn-rwx-segment -Wl,--orphan-handling=error -Wl,--print-memory-usage
LDFLAGS += $(SIZEDEFS) -Wl,-T $(LDSCRIPT) $(LIBS)

.PHONY: clean rom data music images

all: rom

rom: $(ROM)

%.elf:
	$(CC) $(LDFLAGS) $^ -o $@

%.bin: %.elf
	$(OBJ) -O binary $< $@
	$(FIXROM) $(ROM)
	$(CONVLE) $(ROM)

$(ROM:.bin=.elf): $(OBJS_S) $(OBJS_C) $(OBJS_CXX)

$(OBJDIR)/%.o: $(SRCDIR)/%.s $(HDRS) | $(OBJDIR)
	$(CC) $(CFLAGS) -c $< -o $@
$(OBJDIR)/%.o: $(SRCDIR)/%.c $(HDRS) | $(OBJDIR)
	$(CC) $(CFLAGS) -c $< -o $@
$(OBJDIR)/%.o: $(SRCDIR)/%.cc $(HDRS) | $(OBJDIR)
	$(CXX) $(CFLAGS) $(CXXFLAGS) -c $< -o $@

$(OBJDIR):
	$(MKDIR) $@


MIDIS = $(wildcard $(DATADIR)/*.mid)
MIDIS_C = $(patsubst $(DATADIR)/%.mid,$(SRCDIR)/data_%.mid.cc,$(MIDIS))
music: $(MIDIS)
	for f in $(MIDIS) ; do \
		$(CONVMIDI) $${f} $(SRCDIR)/data_$$(basename $${f}).cc $$(basename $${f}) || exit 1; \
	done

TILES = $(wildcard $(DATADIR)/*.png)
TILES_C = $(patsubst $(DATADIR)/%.png,$(SRCDIR)/data_%.png.cc,$(TILES))
images: $(TILES)
	for f in $(TILES) ; do \
		$(CONVTILES) $${f} $(SRCDIR)/data_$$(basename $${f}).cc || exit 1; \
	done

wormhole:
	cd src && $(PYTHON3) wormhole.py

data: music images wormhole


clean:
	$(RMDIR) $(OBJDIR)
	$(RM) $(ROM)
	$(RM) $(MIDIS_C)
	$(RM) $(TILES_C)

