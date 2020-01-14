##
## SerialICE
##
## Copyright (C) 2009 coresystems GmbH
##
## This program is free software; you can redistribute it and/or modify
## it under the terms of the GNU General Public License as published by
## the Free Software Foundation; version 2 of the License.
##
## This program is distributed in the hope that it will be useful,
## but WITHOUT ANY WARRANTY; without even the implied warranty of
## MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
## GNU General Public License for more details.
##
## You should have received a copy of the GNU General Public License
## along with this program; if not, write to the Free Software
## Foundation, Inc.
##

VERSION="1.6"

# romcc requires lots of stack space, and win32 seems to have a fixed size
# stack. Give romcc some room on mingw and cygwin.
_OS=$(shell uname -s | cut -c -7)
STACK=
ifeq ($(_OS),MINGW32)
	STACK=-Wl,--stack,16384000
endif
ifeq ($(_OS),CYGWIN_)
	STACK=-Wl,--stack,16384000
endif

export src := $(shell pwd)
export srctree := $(src)
export srck := $(src)/util/kconfig
export obj := $(src)/build
export objk := $(src)/build/util/kconfig

export KERNELVERSION      := $(shell echo $(VERSION) )
export KCONFIG_AUTOHEADER := $(obj)/config.h
export KCONFIG_AUTOCONFIG := $(obj)/auto.conf

CONFIG_SHELL := sh
KBUILD_DEFCONFIG := configs/defconfig
UNAME_RELEASE := $(shell uname -r)
HAVE_DOTCONFIG := $(wildcard .config)
MAKEFLAGS += -rR --no-print-directory

# Make is silent per default, but 'make V=1' will show all compiler calls.
ifneq ($(V),1)
Q := @
endif

PCREFLAGS=-I/opt/local/include -L/opt/local/lib -lpcre

ifneq ($(shell which i386-elf-gcc),)
CROSS=i386-elf-
endif
AS=$(CROSS)as --32
CC=$(CROSS)gcc -m32
CPP=$(CROSS)cpp
OBJCOPY=$(CROSS)objcopy
NM=$(CROSS)nm

LDFLAGS=-Wa,--divide -nostdlib -nostartfiles -static -T serialice.ld -Wl,--defsym,ALIGNED_ROMSIZE=$(CONFIG_ROMSIZE)

SOURCES = serialice.c serialice.h chipset.c serial.c types.h mainboard/*.c

HOSTCC ?= gcc
HOSTCXX ?= g++
HOSTCFLAGS := -O2 -Wall -I$(srck) -I$(objk)
HOSTCXXFLAGS := -I$(srck) -I$(objk)

INCLUDES = -I$(obj)
CFLAGS := -Wall -Werror -Os $(INCLUDES)
OBJECTS = serialice.o
OBJS    = $(patsubst %,$(obj)/%,$(OBJECTS))

ifeq ($(strip $(HAVE_DOTCONFIG)),)

all: config

else

include $(src)/.config

TARGET-$(CONFIG_BUILD_ROMCC) = $(obj)/serialice.rom
TARGET-$(CONFIG_BUILD_XMMSTACK)  = $(obj)/serialice-gcc.rom
all: $(TARGET-y)

endif

prepare:
	$(Q)mkdir -p $(obj)/util/kconfig/lxdialog

clean:
	$(Q)printf "  CLEAN\n"
	$(Q)rm -rf $(obj)/*.elf $(obj)/*.o
	$(Q)cd $(obj); rm -f romcc serialice.S *.o *.o.s
	$(Q)cd $(obj); rm -f serialice.elf serialice.rom serialice.map
	$(Q)cd $(obj); rm -f serialice-gcc.elf serialice-gcc.rom serialice-gcc.map
	$(Q)cd $(obj); rm -f serialice-gcc.S serialice-pre.s xmmstack serialice-gcc.map

distclean: clean
	$(Q)printf "  DISTCLEAN\n"
	$(Q)rm -rf build
	$(Q)rm -f .config .config.old ..config.tmp .kconfig.d .tmpconfig*

include util/kconfig/Makefile

.PHONY: $(PHONY) prepare clean distclean

$(obj)/serialice.rom: $(obj)/serialice.elf
	$(Q)printf "  OBJCOPY  $(subst $(shell pwd)/,,$(@))\n"
	$(Q)$(OBJCOPY) -O binary $< $@

$(obj)/serialice.elf: $(obj)/serialice.o $(obj)/start.o $(src)/serialice.ld
	$(Q)printf "  LINK     $(subst $(shell pwd)/,,$(@))\n"
	$(Q)$(CC) $(LDFLAGS) -o $@ $(obj)/serialice.o $(obj)/start.o
	$(Q)$(NM) $@ | sort -u > $(obj)/serialice.map

$(obj)/serialice.S: $(SOURCES) $(obj)/romcc
	$(Q)printf "  ROMCC    $(subst $(shell pwd)/,,$(@))\n"
	$(Q)$(obj)/romcc -mcpu=i386 $(INCLUDES) -I. -Imainboard -Isouthbridge -Isuperio -DVERSION=\"$(VERSION)\" -o $@.tmp $<
	$(Q)printf ".section \".rom.text\"\n.globl main\nmain:\n" > $@
	$(Q)cat $@.tmp >> $@
	$(Q)rm $@.tmp


$(obj)/romcc.o: $(src)/util/romcc.c
	$(Q)printf "  HOSTCC   $(subst $(shell pwd)/,,$(@))\n"
	$(Q)$(HOSTCC) $(HOSTCFLAGS) -c -o $@ $^

$(obj)/romcc: $(obj)/romcc.o
	$(Q)printf "  HOSTCC   $(subst $(shell pwd)/,,$(@))\n"
	$(Q)$(HOSTCC) $(HOSTCFLAGS) $(STACK) -o $@ $^

# #####################################################################

$(obj)/serialice-gcc.rom: $(obj)/serialice-gcc.elf
	$(Q)printf "  OBJCOPY  $(subst $(shell pwd)/,,$(@))\n"
	$(Q)$(OBJCOPY) -O binary $< $@

$(obj)/serialice-gcc.elf: $(obj)/serialice-gcc.o $(obj)/start.o serialice.ld
	$(Q)printf "  LINK     $(subst $(shell pwd)/,,$(@))\n"
	$(Q)$(CC) $(LDFLAGS) -o $@ $(obj)/serialice-gcc.o $(obj)/start.o
	$(Q)$(NM) $@ | sort -u > $(obj)/serialice-gcc.map

$(obj)/serialice-pre.s: $(SOURCES) $(obj)/xmmstack
	$(Q)printf "  CC       $(subst $(shell pwd)/,,$(@))\n"
	$(Q)$(CC) -O2 -march=i486 -mno-stackrealign -mpreferred-stack-boundary=2 $(INCLUDES) -I. -Imainboard -fomit-frame-pointer -fno-stack-protector -DVERSION=\"$(VERSION)\" -S $< -o $(obj)/serialice-pre.s

$(obj)/serialice-gcc.S: $(obj)/serialice-pre.s
	$(Q)printf "  XMMSTACK $(subst $(shell pwd)/,,$(@))\n"
	$(Q)$(obj)/xmmstack -xmm $(obj)/serialice-pre.s
	$(Q)mv $(obj)/serialice-pre.sn.s $(obj)/serialice-gcc.S

$(obj)/xmmstack: $(src)/util/xmmstack.c
	$(Q)printf "  HOSTCC   $(subst $(shell pwd)/,,$(@))\n"
	$(Q)$(HOSTCC) $(HOSTCFLAGS) $(PCREFLAGS) -o $@ $^

# #####################################################################

$(obj)/%.o: $(src)/%.S
	$(Q)printf "  CPP      $(subst $(shell pwd)/,,$(@))\n"
	$(Q)$(CPP) $(INCLUDES) -DVERSION=\"$(VERSION)\" -o $@.s $^
	$(Q)printf "  AS       $(subst $(shell pwd)/,,$(@))\n"
	$(Q)$(AS) -o $@ $@.s

$(obj)/%.o: $(obj)/%.S
	$(Q)printf "  CPP      $(subst $(shell pwd)/,,$(@))\n"
	$(Q)$(CPP) $(INCLUDES) -DVERSION=\"$(VERSION)\" -o $@.s $^
	$(Q)printf "  AS       $(subst $(shell pwd)/,,$(@))\n"
	$(Q)$(AS) -o $@ $@.s

# #####################################################################

dongle: $(TARGET-y)
	dongle.py -v -c /dev/cu.usbserial-00* $(TARGET-y) EOF
