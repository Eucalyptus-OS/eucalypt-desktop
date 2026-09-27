CROSS   := x86_64-eucalypt-elf

CC      := $(CROSS)-gcc
CXX     := $(CROSS)-g++
AR      := $(CROSS)-ar

CFLAGS  := -O2 -ffreestanding -fno-stack-protector -fno-stack-check \
           -fno-asynchronous-unwind-tables -mno-red-zone -m64 -mcmodel=small -Wall -Wextra

MLIBC := $(abspath ../eucalypt-distro/eucalypt-mlibc/build/install/src/usr/local)
MLIBC_INC := $(MLIBC)/include
MLIBC_LIB := $(MLIBC)/lib

CRTBEGIN := $(shell $(CC) -print-file-name=crtbegin.o)
CRTEND   := $(shell $(CC) -print-file-name=crtend.o)

LDFLAGS := -nostdlib -static -z max-page-size=0x1000 -z noexecstack

LDSCRIPT := $(abspath src/linker.ld)
BINDIR   := $(abspath src/bin)

LIBS := $(MLIBC_LIB)/libc.a $(MLIBC_LIB)/libssp_nonshared.a $(MLIBC_LIB)/libssp.a \
$(MLIBC_LIB)/libpthread.a $(MLIBC_LIB)/libm.a $(MLIBC_LIB)/libutil.a

SRCS := $(shell find src -type f -name '*.c' -print)
BINS := $(patsubst src/%.c,$(BINDIR)/%,$(SRCS))

.PHONY: all clean

all: $(BINS)

$(BINDIR)/%: src/%.c $(LDSCRIPT) $(LIBS)
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -I$(MLIBC_INC) -c $< -o $@.o
	$(CC) $(LDFLAGS) -T $(LDSCRIPT) \
		$(MLIBC_LIB)/crt1.o $(CRTBEGIN) $@.o \
		$(LIBS) $(CRTEND) \
		-o $@
	strip --strip-debug $@
	rm -f $@.o

clean:
	rm -rf $(BINDIR)