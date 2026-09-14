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
OBJDIR = ./obj

# Basic compile options
OPTIMIZE = -Os
WARNINGS = -Wall -Wextra -pedantic -Werror

# Below here probably doesn't need to be touched

LDSCRIPT = ./tools/loopy.ld

CFLAGS  = $(OPTIMIZE) -g -gdwarf-4
CFLAGS += -m1 -mrenesas
CFLAGS += -ffreestanding
CFLAGS += -falign-functions=4 -ffunction-sections -fdata-sections
CFLAGS += -fomit-frame-pointer -fno-asynchronous-unwind-tables -fno-unwind-tables
CFLAGS += -Wstack-usage=$(shell numfmt --from=iec $(STACKSIZE))
CFLAGS += -I./include -I./lib
CFLAGS += $(WARNINGS)
CFLAGS += -DWEB_BUILD=0

CXXFLAGS = -std=c++23 -fno-exceptions -fno-non-call-exceptions -fno-rtti -fno-threadsafe-statics

SIZEDEFS  = -Wl,--defsym=SRAMSIZE=$(SRAMSIZE)
SIZEDEFS += -Wl,--defsym=STACKSIZE=$(STACKSIZE)

LDFLAGS  = -nostartfiles -nolibc -Wl,--gc-sections -Wl,--no-warn-rwx-segment -Wl,--orphan-handling=error -Wl,--print-memory-usage
LDFLAGS += $(SIZEDEFS) -Wl,-T $(LDSCRIPT)



# Add targets here.

.PHONY: all clean data bbb_data

ROMS = bbb.bin gallery.bin

all: $(ROMS)

data: bbb_data

clean:
	$(RMDIR) $(OBJDIR)
	$(RM) $(ROMS) $(ROMS:.bin=.elf)
	$(RM) $(BBB_DATA_C)



# Loopy library.

LOOPYLIB_HDRS = $(wildcard lib/*.h)
LOOPYLIB_S_SRC = $(wildcard lib/*.s)
LOOPYLIB_C_SRC = $(wildcard lib/*.c)
LOOPYLIB_CXX_SRC = $(wildcard lib/*.cc)
LOOPYLIB_S_OBJS = $(patsubst lib/%.s,$(OBJDIR)/lib_%.o,$(LOOPYLIB_S_SRC))
LOOPYLIB_C_OBJS = $(patsubst lib/%.c,$(OBJDIR)/lib_%.o,$(LOOPYLIB_C_SRC))
LOOPYLIB_CXX_OBJS = $(patsubst lib/%.cc,$(OBJDIR)/lib_%.o,$(LOOPYLIB_CXX_SRC))
LOOPYLIB_OBJS = $(LOOPYLIB_S_OBJS) $(LOOPYLIB_C_OBJS) $(LOOPYLIB_CXX_OBJS)

$(OBJDIR)/lib_%.o: lib/%.s $(LOOPYLIB_HDRS) | $(OBJDIR)
	$(CC) $(CFLAGS) -c $< -o $@
$(OBJDIR)/lib_%.o: lib/%.c $(LOOPYLIB_HDRS) | $(OBJDIR)
	$(CC) $(CFLAGS) -c $< -o $@
$(OBJDIR)/lib_%.o: lib/%.cc $(LOOPYLIB_HDRS) | $(OBJDIR)
	$(CXX) $(CFLAGS) $(CXXFLAGS) -c $< -o $@



# BBB.

BBB_HDRS = $(wildcard bbb/*.h)
BBB_S_SRC = $(wildcard bbb/*.s)
BBB_C_SRC = $(wildcard bbb/*.c)
BBB_CXX_SRC = $(wildcard bbb/*.cc)
BBB_S_OBJS = $(patsubst bbb/%.s,$(OBJDIR)/bbb_%.o,$(BBB_S_SRC))
BBB_C_OBJS = $(patsubst bbb/%.c,$(OBJDIR)/bbb_%.o,$(BBB_C_SRC))
BBB_CXX_OBJS = $(patsubst bbb/%.cc,$(OBJDIR)/bbb_%.o,$(BBB_CXX_SRC))
BBB_OBJS = $(BBB_S_OBJS) $(BBB_C_OBJS) $(BBB_CXX_OBJS)

$(OBJDIR)/bbb_%.o: bbb/%.s $(LOOPYLIB_HDRS) $(BBB_HDRS) | $(OBJDIR)
	$(CC) $(CFLAGS) -c $< -o $@
$(OBJDIR)/bbb_%.o: bbb/%.c $(LOOPYLIB_HDRS) $(BBB_HDRS) | $(OBJDIR)
	$(CC) $(CFLAGS) -c $< -o $@
$(OBJDIR)/bbb_%.o: bbb/%.cc $(LOOPYLIB_HDRS) $(BBB_HDRS) | $(OBJDIR)
	$(CXX) $(CFLAGS) $(CXXFLAGS) -c $< -o $@

bbb.elf: $(BBB_OBJS) $(LOOPYLIB_OBJS)



BBB_MIDIS = $(wildcard bbb_data/*.mid)
BBB_MIDIS_C = $(patsubst bbb_data/%.mid,bbb/data_%.mid.cc,$(BBB_MIDIS))
bbb/data_%.mid.cc: bbb_data/%.mid
	$(CONVMIDI) $^ $@ $$(basename $^)

BBB_TILES = $(wildcard bbb_data/*.png)
BBB_TILES_C = $(patsubst bbb_data/%.png,bbb/data_%.png.cc,$(BBB_TILES))
bbb/data_%.png.cc: bbb_data/%.png
	$(CONVTILES) $^ $@

bbb/data_wormhole.cc: bbb/wormhole.py
	cd bbb && $(PYTHON3) wormhole.py

BBB_DATA_C = $(BBB_MIDIS_C) $(BBB_TILES_C) bbb/data_wormhole.cc
bbb_data: $(BBB_DATA_C)



# Gallery.

GALLERY_HDRS = $(wildcard gallery/*.h)
GALLERY_S_SRC = $(wildcard gallery/*.s)
GALLERY_C_SRC = $(wildcard gallery/*.c)
GALLERY_CXX_SRC = $(wildcard gallery/*.cc)
GALLERY_S_OBJS = $(patsubst gallery/%.s,$(OBJDIR)/gallery_%.o,$(GALLERY_S_SRC))
GALLERY_C_OBJS = $(patsubst gallery/%.c,$(OBJDIR)/gallery_%.o,$(GALLERY_C_SRC))
GALLERY_CXX_OBJS = $(patsubst gallery/%.cc,$(OBJDIR)/gallery_%.o,$(GALLERY_CXX_SRC))
GALLERY_OBJS = $(GALLERY_S_OBJS) $(GALLERY_C_OBJS) $(GALLERY_CXX_OBJS)

$(OBJDIR)/gallery_%.o: gallery/%.s $(LOOPYLIB_HDRS) $(GALLERY_HDRS) | $(OBJDIR)
	$(CC) $(CFLAGS) -c $< -o $@
$(OBJDIR)/gallery_%.o: gallery/%.c $(LOOPYLIB_HDRS) $(GALLERY_HDRS) | $(OBJDIR)
	$(CC) $(CFLAGS) -c $< -o $@
$(OBJDIR)/gallery_%.o: gallery/%.cc $(LOOPYLIB_HDRS) $(GALLERY_HDRS) | $(OBJDIR)
	$(CXX) $(CFLAGS) $(CXXFLAGS) -c $< -o $@

gallery.elf: $(GALLERY_OBJS) $(LOOPYLIB_OBJS)



# Helpers.

%.elf:
	$(CC) $(LDFLAGS) $^ -o $@

%.bin: %.elf
	$(OBJ) -O binary $< $@
	$(FIXROM) $@
	$(CONVLE) $@

$(OBJDIR):
	$(MKDIR) $@

