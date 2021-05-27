/*
 * SerialICE
 *
 * Copyright (C) 2009 coresystems GmbH
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2 of the License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc.
 */

#include <types.h>
#include <serialice.h>
#include <io.h>

/* Hardware specific functions */

#include "chipset.c"

/* Serial functions */
#include "serial.c"

/* Accessor functions */

static void serialice_read_memory(void)
{
	u8 width;
	u32 addr;

	// Format:
	// *rm00000000.w
	addr = serial_get32();
	serial_getc();	// skip .
	width = serial_getc();

	serial_putc('\r'); serial_putc('\n');

	switch (width) {
	case 'b': serial_put8(read8(addr)); break;
	case 'w': serial_put16(read16(addr)); break;
	case 'l': serial_put32(read32(addr)); break;
#ifdef CONFIG_SUPPORT_64_BIT_ACCESS
	case 'q': serial_put64(read64(addr)); break;
#endif
	}
}

static void serialice_write_memory(void)
{
	u8 width;
	u32 addr;
	u32 data;
#ifdef CONFIG_SUPPORT_64_BIT_ACCESS
	u64_t data64;
#endif

	// Format:
	// *wm00000000.w=0000
	addr = serial_get32();
	serial_getc();	// skip .
	width = serial_getc();
	serial_getc();	// skip =

	switch (width) {
	case 'b': data = serial_get8(); write8(addr, (u8)data); break;
	case 'w': data = serial_get16(); write16(addr, (u16)data); break;
	case 'l': data = serial_get32(); write32(addr, (u32)data); break;
#ifdef CONFIG_SUPPORT_64_BIT_ACCESS
	case 'q': data64 = serial_get64(); write64(addr, data64); break;
#endif
	}
}

static void serialice_read_io(void)
{
	u8 width;
	u16 port;

	// Format:
	// *ri0000.w
	port = serial_get16();
	serial_getc();	// skip .
	width = serial_getc();

	serial_putc('\r'); serial_putc('\n');

	switch (width) {
	case 'b': serial_put8(inb(port)); break;
	case 'w': serial_put16(inw(port)); break;
	case 'l': serial_put32(inl(port)); break;
	}
}

static void serialice_write_io(void)
{
	u8 width;
	u16 port;
	u32 data;

	// Format:
	// *wi0000.w=0000
	port = serial_get16();
	serial_getc();	// skip .
	width = serial_getc();
	serial_getc();	// skip =

	switch (width) {
	case 'b': data = serial_get8(); outb((u8)data, port); break;
	case 'w': data = serial_get16(); outw((u16)data, port); break;
	case 'l': data = serial_get32(); outl((u32)data, port); break;
	}
}

static void serialice_read_msr(void)
{
	u32 addr, key;
	msr_t msr;

	// Format:
	// *rc00000000.9c5a203a
	addr = serial_get32();
	serial_getc();	   // skip .
	key = serial_get32(); // key in %edi

	serial_putc('\r'); serial_putc('\n');

	msr = rdmsr(addr, key);
	serial_put32(msr.hi);
	serial_putc('.');
	serial_put32(msr.lo);
}

static void serialice_write_msr(void)
{
	u32 addr, key;
	msr_t msr;

	// Format:
	// *wc00000000.9c5a203a=00000000.00000000
	addr = serial_get32();
	serial_getc();	// skip .
	key = serial_get32(); // read key in %edi
	serial_getc();	// skip =
	msr.hi = serial_get32();
	serial_getc();	// skip .
	msr.lo = serial_get32();

#ifdef __ROMCC__
	/* Cheat to avoid register outage */
	wrmsr(addr, msr, 0x9c5a203a);
#else
	wrmsr(addr, msr, key);
#endif
}

static void serialice_cpuinfo(void)
{
	u32 eax, ecx;
	u32 reg32;

	// Format:
	//    --EAX--- --ECX---
	// *ci00000000.00000000
	eax = serial_get32();
	serial_getc(); // skip .
	ecx = serial_get32();

	serial_putc('\r'); serial_putc('\n');

	/* This code looks quite crappy but this way we don't
 	 * have to worry about running out of registers if we
 	 * occupy eax, ebx, ecx, edx at the same time
 	 */
	reg32 = cpuid_eax(eax, ecx);
	serial_put32(reg32);
	serial_putc('.');

	reg32 = cpuid_ebx(eax, ecx);
	serial_put32(reg32);
	serial_putc('.');

	reg32 = cpuid_ecx(eax, ecx);
	serial_put32(reg32);
	serial_putc('.');

	reg32 = cpuid_edx(eax, ecx);
	serial_put32(reg32);
}

static void serialice_mainboard(void)
{
	serial_putc('\r'); serial_putc('\n');

	/* must be defined in mainboard/<boardname>.c */
	serial_putstring(boardname);
}

static void serialice_version(void)
{
	serial_putstring("\nSerialICE v" VERSION " (" __DATE__ ")\n");
}

int main(void)
{
	chipset_init();

	serial_init();

	serialice_version();

	while(1) {
		u16 c;
		serial_putstring("\n> ");

		c = serial_getc();
		if (c != '*')
			continue;

		c = serial_getc() << 8;
		c |= serial_getc();

		switch(c) {
		case (('r' << 8)|'m'): // Read Memory *rm
			serialice_read_memory();
			break;
		case (('w' << 8)|'m'): // Write Memory *wm
			serialice_write_memory();
			break;
		case (('r' << 8)|'i'): // Read IO *ri
			serialice_read_io();
			break;
		case (('w' << 8)|'i'): // Write IO *wi
			serialice_write_io();
			break;
		case (('r' << 8)|'c'): // Read CPU MSR *rc
			serialice_read_msr();
			break;
		case (('w' << 8)|'c'): // Write CPU MSR *wc
			serialice_write_msr();
			break;
		case (('c' << 8)|'i'): // Read CPUID *ci
			serialice_cpuinfo();
			break;
		case (('m' << 8)|'b'): // Read mainboard type *mb
			serialice_mainboard();
			break;
		case (('v' << 8)|'i'): // Read version info *vi
			serialice_version();
			break;
		default:
			serial_putstring("ERROR\n");
			break;
		}
	}

	// Never get here:
	return 0;
}
