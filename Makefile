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

HOSTCC=gcc
HOSTCFLAGS= -O2 -Wall
PCREFLAGS=-I/opt/local/include -L/opt/local/lib -lpcre

ifneq ($(shell which i386-elf-gcc),)
CROSS=i386-elf-
endif
AS=$(CROSS)as
CC=$(CROSS)gcc
OBJCOPY=$(CROSS)objcopy
NM=$(CROSS)nm

LDFLAGS=-Wa,--divide -nostdlib -nostartfiles -static -T serialice.ld

SOURCES = serialice.c chipset.c config.h serial.c types.h mainboard/*.c

all: serialice.rom

serialice.rom: serialice.elf
	$(OBJCOPY) -O binary $< $@

serialice.elf: serialice.o start.o serialice.ld
	$(CC) $(LDFLAGS) -o $@ serialice.o start.o 
	$(NM) $@ | sort -u > serialice.map

serialice.S: $(SOURCES) ./romcc
	./romcc -mcpu=i386 -I. -Imainboard -DVERSION=\"$(VERSION)\" -o $@.tmp $<
	printf ".section \".rom.text\"\n.globl main\nmain:\n" > $@
	cat $@.tmp >> $@
	rm $@.tmp

romcc: util/romcc.c
	$(HOSTCC) $(HOSTCFLAGS) -o $@ $^

# #####################################################################

serialice-gcc.rom: serialice-gcc.elf
	$(OBJCOPY) -O binary $< $@

serialice-gcc.elf: serialice-gcc.o start.o serialice.ld
	$(CC) $(LDFLAGS) -o $@ serialice-gcc.o start.o 
	$(NM) $@ | sort -u > serialice-gcc.map

serialice-pre.s: $(SOURCES) ./xmmstack
	$(CC) -O2 -march=i486 -mno-stackrealign  -mpreferred-stack-boundary=2  -I. -Imainboard -fomit-frame-pointer -fno-stack-protector -DVERSION=\"$(VERSION)\" -S $< -o serialice-pre.s

serialice-gcc.S: serialice-pre.s
	./xmmstack -xmm serialice-pre.s
	mv serialice-pre.sn.s serialice-gcc.S

xmmstack: util/xmmstack.c
	$(HOSTCC) $(HOSTCFLAGS) $(PCREFLAGS) -o $@ $^

# #####################################################################

clean:
	rm -f romcc serialice.S *.o *.o.s
	rm -f serialice.elf serialice.rom serialice.map
	rm -f serialice-gcc.elf serialice-gcc.rom serialice-gcc.map
	rm -f serialice-gcc.S serialice-pre.s xmmstack serialice-gcc.map

dongle: serialice.rom
	dongle.py -v -c /dev/cu.usbserial-00* serialice.rom  4032K

%.o: %.S
	$(CPP) -DVERSION=\"$(VERSION)\" -o $@.s $^
	$(AS) -o $@ $@.s
