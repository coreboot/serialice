/*
 * SerialICE
 *
 * Copyright (C) 2009 Uwe Hermann <uwe@hermann-uwe.de>
 * Copyright (C) 2010 Arne Georg Gleditsch <arne.gleditsch@numascale.com>
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

const char boardname[33]="Tyan S2895                      ";

#define SUPERIO_CONFIG_PORT	0x2e
#define SUPERIO_GPIO_IO_BASE	0x400

#define LPC47B397_SP1		4	/* Com1 */
#define LPC47B397_RT		10	/* Runtime reg */

static void lpc47b397_gpio_offset_out(unsigned iobase, unsigned offset, unsigned value)
{
	outb(value,iobase+offset);
}

static unsigned lpc47b397_gpio_offset_in(unsigned iobase, unsigned offset)
{
	return inb(iobase+offset);
}


/* Functions for the SMSC LPC47B272 & B397 */
static void smsc_enable_serial(u16 port, u8 dev, unsigned iobase)
{
	pnp_enter_ext_func_mode_alt(port);
	pnp_set_logical_device(port, dev);
	pnp_set_enable(port, 0);
	pnp_set_iobase0(port, iobase);
	pnp_set_enable(port, 1);
	pnp_exit_ext_func_mode(port);
}

static void superio_init(void)
{
	unsigned value;
	u32 dword;
	u8 byte;

	pci_write_config32(PCI_ADDR(0, 1, 0, 0xac), 0x047f0400);

	byte = pci_read_config8(PCI_ADDR(0, 1 , 0, 0x7b));
	byte |= 0x20;
	pci_write_config8(PCI_ADDR(0, 1 , 0, 0x7b), byte);

	dword = pci_read_config32(PCI_ADDR(0, 1 , 0, 0xa0));
	dword |= (1<<29)|(1<<0);
	pci_write_config32(PCI_ADDR(0, 1 , 0, 0xa0), dword);

	dword = pci_read_config32(PCI_ADDR(0, 1, 0, 0xa4));
	dword |= (1<<16);
	pci_write_config32(PCI_ADDR(0, 1, 0, 0xa4), dword);

	smsc_enable_serial(SUPERIO_CONFIG_PORT, LPC47B397_RT, SUPERIO_GPIO_IO_BASE);
	value = lpc47b397_gpio_offset_in(SUPERIO_GPIO_IO_BASE, 0x77);
	value &= 0xbf;
	lpc47b397_gpio_offset_out(SUPERIO_GPIO_IO_BASE, 0x77, value);
}

static void chipset_init(void)
{
	superio_init();
	smsc_enable_serial(SUPERIO_CONFIG_PORT, LPC47B397_SP1, CONFIG_SERIAL_PORT);
	__asm__ __volatile__("\
	jmp skip\n\
	.align 128\n\
	.globl ck804_romstrap\n\
	ck804_romstrap:\n\
	.long 0x2b16d065\n\
	.long 0x0\n\
	.long 0x0\n\
	.long linkedlist\n\
	linkedlist:\n\
	.long 0x0003001C                        // 10h\n\
	.long 0x08000000                        // 14h\n\
	.long 0x00000000                        // 18h\n\
	.long 0xFFFFFFFF                        // 1Ch\n\
	.long 0xFFFFFFFF                        // 20h\n\
	.long 0xFFFFFFFF                        // 24h\n\
	.long 0xFFFFFFFF                        // 28h\n\
	.long 0xFFFFFFFF                        // 2Ch\n\
	.long 0x81543266                        // 30h, MAC address low 4 byte\n\
	.long 0x000000E0                        // 34h, MAC address high 4 byte\n\
	.long 0x002309CE                        // 38h, UUID low 4 byte\n\
	.long 0x00E08100                        // 3Ch, UUID high 4 byte\n\
	skip:\n");
}
