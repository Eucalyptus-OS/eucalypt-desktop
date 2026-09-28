# eucalypt-desktop
#
# Builds the desktop environment as a single static binary linked against the
# cross-compiled mlibc. Everything is overridable so this can be built either
# from a standalone clone or from the eucalypt-distro top-level Makefile.
#
#   make            build ./bin/desktop
#   make clean      remove build artifacts

.SUFFIXES:

CROSS   ?= x86_64-eucalypt-elf

# Where the cross toolchain lives, when it is not already on PATH.
CROSS_PREFIX ?= $(abspath ../tools/cross/bin)
CC      = $(CROSS_PREFIX)/$(CROSS)-gcc
STRIP   = $(CROSS_PREFIX)/$(CROSS)-strip

CFLAGS  := -O2 -ffreestanding -fno-stack-protector -fno-stack-check \
           -fno-asynchronous-unwind-tables -mno-red-zone -m64 -mcmodel=small -Wall -Wextra

# Staged mlibc. Its <abi/...> headers are symlinks into the kernel tree, so this
# include path also provides syscalls.h.
MLIBC := $(abspath ../eucalypt-mlibc/build/install/usr/local)
MLIBC_INC := $(MLIBC)/include
MLIBC_LIB := $(MLIBC)/lib

CRTBEGIN := $(shell $(CC) -print-file-name=crtbegin.o)
CRTEND   := $(shell $(CC) -print-file-name=crtend.o)

LDFLAGS := -nostdlib -static -z max-page-size=0x1000 -z noexecstack

LDSCRIPT := src/linker.ld
BINDIR   ?= $(abspath bin)
PROG     := desktop

LIBS := $(MLIBC_LIB)/libc.a $(MLIBC_LIB)/libssp_nonshared.a $(MLIBC_LIB)/libssp.a \
$(MLIBC_LIB)/libpthread.a $(MLIBC_LIB)/libm.a $(MLIBC_LIB)/libutil.a

SRCS := $(filter-out src/gui/gui_client.c src/gui/gui_demo.c,$(sort $(wildcard src/*.c) $(wildcard src/gui/*.c)))
OBJS := $(patsubst src/%.c,$(BINDIR)/%.o,$(SRCS))
DEPS := $(OBJS:.o=.d)

TARGET := $(BINDIR)/$(PROG)

# A second binary: a plain client that asks the compositor for a window.
DEMO_SRCS := src/gui/gui_demo.c src/gui/gui_client.c
DEMO_OBJS := $(patsubst src/%.c,$(BINDIR)/%.o,$(DEMO_SRCS))
DEMO_TARGET := $(BINDIR)/guitest

.PHONY: all clean

all: $(TARGET) $(DEMO_TARGET)

$(TARGET): $(OBJS) $(LDSCRIPT) $(LIBS) $(CRTBEGIN) $(CRTEND)
	@mkdir -p $(dir $@)
	$(CC) $(LDFLAGS) -T $(LDSCRIPT) \
		$(MLIBC_LIB)/crt1.o $(CRTBEGIN) $(OBJS) \
		$(LIBS) $(CRTEND) \
		-o $@
	$(STRIP) --strip-debug $@

$(DEMO_TARGET): $(DEMO_OBJS) $(LDSCRIPT) $(LIBS) $(CRTBEGIN) $(CRTEND)
	@mkdir -p $(dir $@)
	$(CC) $(LDFLAGS) -T $(LDSCRIPT) \
		$(MLIBC_LIB)/crt1.o $(CRTBEGIN) $(DEMO_OBJS) \
		$(LIBS) $(CRTEND) \
		-o $@
	$(STRIP) --strip-debug $@

$(BINDIR)/%.o: src/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -Isrc -I$(MLIBC_INC) -MMD -MP -c $< -o $@

clean:
	rm -rf $(BINDIR)

-include $(DEPS)
