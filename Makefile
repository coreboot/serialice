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
## Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301 USA
##

VERSION="1.3"

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
OBJCOPY=$(CROSS)objcopy
NM=$(CROSS)nm

LDFLAGS=-Wa,--divide -nostdlib -nostartfiles -static -T serialice.ld

SOURCES = serialice.c chipset.c config.h serial.c types.h mainboard/*.c

HOSTCC = gcc
HOSTCXX = g++
HOSTCFLAGS := -O2 -Wall -I$(srck) -I$(objk)
HOSTCXXFLAGS := -I$(srck) -I$(objk)

INCLUDES = -Ibuild
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
	$(Q)rm -rf $(obj)/*.elf $(obj)/*.o
	$(Q)cd $(obj); rm -f romcc serialice.S *.o *.o.s
	$(Q)cd $(obj); rm -f serialice.elf serialice.rom serialice.map
	$(Q)cd $(obj); rm -f serialice-gcc.elf serialice-gcc.rom serialice-gcc.map
	$(Q)cd $(obj); rm -f serialice-gcc.S serialice-pre.s xmmstack serialice-gcc.map

distclean: clean
	$(Q)rm -rf build
	$(Q)rm -f .config .config.old ..config.tmp .kconfig.d .tmpconfig*

include util/kconfig/Makefile

.PHONY: $(PHONY) prepare clean distclean

$(obj)/serialice.rom: $(obj)/serialice.elf
	$(OBJCOPY) -O binary $< $@

$(obj)/serialice.elf: $(obj)/serialice.o $(obj)/start.o $(src)/serialice.ld
	$(CC) $(LDFLAGS) -o $@ $(obj)/serialice.o $(obj)/start.o 
	$(NM) $@ | sort -u > $(obj)/serialice.map

$(obj)/serialice.S: $(SOURCES) $(obj)/romcc
	$(obj)/romcc -mcpu=i386 -I. -Imainboard -DVERSION=\"$(VERSION)\" -o $@.tmp $<
	printf ".section \".rom.text\"\n.globl main\nmain:\n" > $@
	cat $@.tmp >> $@
	rm $@.tmp

$(obj)/romcc: $(src)/util/romcc.c
	$(HOSTCC) $(HOSTCFLAGS) -o $@ $^

# #####################################################################

$(obj)/serialice-gcc.rom: $(obj)/serialice-gcc.elf
	$(OBJCOPY) -O binary $< $@

$(obj)/serialice-gcc.elf: $(obj)/serialice-gcc.o $(obj)/start.o serialice.ld
	$(CC) $(LDFLAGS) -o $@ $(obj)/serialice-gcc.o $(obj)/start.o 
	$(NM) $@ | sort -u > $(obj)/serialice-gcc.map

$(obj)/serialice-pre.s: $(SOURCES) $(obj)/xmmstack
	$(CC) -O2 -march=i486 -mno-stackrealign  -mpreferred-stack-boundary=2 -I. -Imainboard -fomit-frame-pointer -fno-stack-protector -DVERSION=\"$(VERSION)\" -S $< -o $(obj)/serialice-pre.s

$(obj)/serialice-gcc.S: $(obj)/serialice-pre.s
	$(obj)/xmmstack -xmm $(obj)/serialice-pre.s
	mv $(obj)/serialice-pre.sn.s $(obj)/serialice-gcc.S

$(obj)/xmmstack: $(src)/util/xmmstack.c
	$(HOSTCC) $(HOSTCFLAGS) $(PCREFLAGS) -o $@ $^

# #####################################################################

clean:
dongle: serialice.rom
	dongle.py -v -c /dev/cu.usbserial-00* serialice.rom  4032K

$(obj)/%.o: $(src)/%.S
	$(CPP) -DVERSION=\"$(VERSION)\" -o $@.s $^
	$(AS) -o $@ $@.s
