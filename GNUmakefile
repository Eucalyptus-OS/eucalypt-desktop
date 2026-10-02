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

# The anime art and app are their own binary, not part of the compositor, so
# they are excluded here. The art is also what pulls in the gfx library, and the
# compositor has no reason to carry it.
SRCS := $(filter-out src/gui/gui_client.c src/gui/gui_demo.c \
                   src/gui/anime_app.c src/gui/anime_art.c,\
          $(sort $(wildcard src/*.c) $(wildcard src/gui/*.c)))
OBJS := $(patsubst src/%.c,$(BINDIR)/%.o,$(SRCS))
DEPS := $(OBJS:.o=.d)

TARGET := $(BINDIR)/$(PROG)

# A second binary: a plain client that asks the compositor for a window.
DEMO_SRCS := src/gui/gui_demo.c src/gui/gui_client.c
DEMO_OBJS := $(patsubst src/%.c,$(BINDIR)/%.o,$(DEMO_SRCS))
DEMO_TARGET := $(BINDIR)/guitest

# The examples link the standalone gfx library, whose sources are compiled in
# directly rather than taken from a prebuilt archive, so the freestanding flags
# and the cross compiler's ABI apply to them too.
GFX_DIR ?= $(abspath ../eucalypt-gfx)
GFX_SRCS := $(GFX_DIR)/src/gfx.c $(GFX_DIR)/src/gfx_text.c

# Shared example code, compiled once and linked into each example.
EX_SHARED    := examples/examples.c
EX_SHARED_O  := $(BINDIR)/examples/examples.o
GFX_OBJS     := $(patsubst $(GFX_DIR)/%.c,$(BINDIR)/gfx/%.o,$(GFX_SRCS))

# ex_terminal is not a graphics demo: it drives a shell over pipes. It lives here
# because it shares the text and event helpers, and because the desktop needs
# a way to run the other examples without an init hack.
EXAMPLES := ex_gradient ex_shapes ex_text ex_terminal
EXAMPLE_TARGETS := $(addprefix $(BINDIR)/,$(EXAMPLES))
EXAMPLE_OBJS := $(patsubst examples/%.c,$(BINDIR)/examples/%.o,$(EX_SHARED))

.PHONY: all clean

all: $(TARGET) $(DEMO_TARGET) $(EXAMPLE_TARGETS)

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

# Every example is the same shape: its own main, plus the shared helpers, the
# GUI client, and gfx. One pattern rule covers them all. It is spelled with a
# static pattern so it cannot swallow the desktop or demo targets above.
$(EXAMPLE_TARGETS): $(BINDIR)/%: $(BINDIR)/examples/%.o $(EX_SHARED_O) \
		$(BINDIR)/gui/gui_client.o $(GFX_OBJS) \
		$(LDSCRIPT) $(LIBS) $(CRTBEGIN) $(CRTEND)
	@mkdir -p $(dir $@)
	$(CC) $(LDFLAGS) -T $(LDSCRIPT) \
		$(MLIBC_LIB)/crt1.o $(CRTBEGIN) $< $(EX_SHARED_O) \
		$(BINDIR)/gui/gui_client.o $(GFX_OBJS) \
		$(LIBS) $(CRTEND) \
		-o $@
	$(STRIP) --strip-debug $@

# Anything in this tree may include <eucalypt/gfx.h>, so the gfx include path is
# on the common rules rather than only for the gfx objects themselves.
$(BINDIR)/%.o: src/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -Isrc -I$(GFX_DIR)/include -I$(MLIBC_INC) -MMD -MP -c $< -o $@

$(BINDIR)/examples/%.o: examples/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -Iexamples -Isrc -Isrc/gui -I$(GFX_DIR)/include -I$(MLIBC_INC) \
		-MMD -MP -c $< -o $@

$(BINDIR)/gfx/%.o: $(GFX_DIR)/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -I$(GFX_DIR)/include -I$(MLIBC_INC) -MMD -MP -c $< -o $@

clean:
	rm -rf $(BINDIR)

-include $(DEPS)
