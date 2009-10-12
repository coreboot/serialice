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
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301 USA
 */

#include <types.h>
#include <serialice.h>
#include <io.h>

/* SIO functions */
#include "serial.c"

/* Hardware specific functions */

#include "chipset.c"

/* Accessor functions */

static void serialice_read_memory(void)
{
	u8 width;
	u32 addr;

	// Format:
	// *rm00000000.w
	addr = sio_get32();
	sio_getc();	// skip .
	width = sio_getc();

	sio_putc('\r'); sio_putc('\n');

	switch (width) {
	case 'b': sio_put8(read8(addr)); break;
	case 'w': sio_put16(read16(addr)); break;
	case 'l': sio_put32(read32(addr)); break;
	}
}

static void serialice_write_memory(void)
{
	u8 width;
	u32 addr;
	u32 data;

	// Format:
	// *wm00000000.w=0000
	addr = sio_get32();
	sio_getc();	// skip .
	width = sio_getc();
	sio_getc();	// skip =

	switch (width) {
	case 'b': data = sio_get8(); write8(addr, (u8)data); break;
	case 'w': data = sio_get16(); write16(addr, (u16)data); break;
	case 'l': data = sio_get32(); write32(addr, (u32)data); break;
	}
}

static void serialice_read_io(void)
{
	u8 width;
	u16 port;

	// Format:
	// *ri0000.w
	port = sio_get16();
	sio_getc();	// skip .
	width = sio_getc();

	sio_putc('\r'); sio_putc('\n');

	switch (width) {
	case 'b': sio_put8(inb(port)); break;
	case 'w': sio_put16(inw(port)); break;
	case 'l': sio_put32(inl(port)); break;
	}
}

static void serialice_write_io(void)
{
	u8 width;
	u16 port;
	u32 data;

	// Format:
	// *wi0000.w=0000
	port = sio_get16();
	sio_getc();	// skip .
	width = sio_getc();
	sio_getc();	// skip =

	switch (width) {
	case 'b': data = sio_get8(); outb((u8)data, port); break;
	case 'w': data = sio_get16(); outw((u16)data, port); break;
	case 'l': data = sio_get32(); outl((u32)data, port); break;
	}
}

static void serialice_read_msr(void)
{
	u32 addr;
	msr_t msr;

	// Format:
	// *rc00000000
	addr = sio_get32();

	sio_putc('\r'); sio_putc('\n');

	msr = rdmsr(addr);
	sio_put32(msr.hi);
	sio_putc('.');
	sio_put32(msr.lo);
}

static void serialice_write_msr(void)
{
	u32 addr;
	msr_t msr;

	// Format:
	// *wc00000000=00000000.00000000
	addr = sio_get32();
	sio_getc();	// skip =
	msr.hi = sio_get32();
	sio_getc();	// skip .
	msr.lo = sio_get32();

	wrmsr(addr, msr);
}

static void serialice_cpuinfo(void)
{
	u32 idx;
	u32 reg32;

	// Format:
	// *ci00000000
	idx = sio_get32();

	sio_putc('\r'); sio_putc('\n');

	/* This code looks quite crappy but this way we don't
 	 * have to worry about running out of registers if we
 	 * occupy eax, ebx, ecx, edx at the same time 
 	 */
	reg32 = cpuid_eax(idx);
	sio_put32(reg32);
	sio_putc('.');

	reg32 = cpuid_ebx(idx);
	sio_put32(reg32);
	sio_putc('.');

	reg32 = cpuid_ecx(idx);
	sio_put32(reg32);
	sio_putc('.');

	reg32 = cpuid_edx(idx);
	sio_put32(reg32);
}

int main(void)
{
	chipset_init();

	sio_init();

	sio_putstring("\nSerialICE v" VERSION " (" __DATE__ ")\n");

	while(1) {
		u16 c;
		sio_putstring("\n> ");

		c = sio_getc();
		if (c != '*')
			continue;

		c = sio_getc() << 8;
		c |= sio_getc();

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
		case (('w' << 8)|'i'): // Read IO *wi
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
		default:
			sio_putstring("ERROR\n");
			break;
		}
	}

	// Never get here:
	return 0;
}
