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

const char boardname[33]="Tyan S2892                      ";

#define SUPERIO_CONFIG_PORT	0x2e
#define W83627HF_SP1            2

static void sio_setup(void)
{
	unsigned value;
	u32 dword;
	u8 byte;

	byte = pci_read_config8(PCI_ADDR(0, 1 , 0, 0x7b));
	byte |= 0x20;
	pci_write_config8(PCI_ADDR(0, 1 , 0, 0x7b), byte);

	dword = pci_read_config32(PCI_ADDR(0, 1 , 0, 0xa0));
	dword |= (1<<0);
	pci_write_config32(PCI_ADDR(0, 1 , 0, 0xa0), dword);
}

static void superio_init(void)
{
	int i;
	pnp_enter_ext_func_mode(SUPERIO_CONFIG_PORT);

	/* Enable the serial port. */
	pnp_set_logical_device(SUPERIO_CONFIG_PORT, W83627HF_SP1); /* COM1 */
	pnp_set_enable(SUPERIO_CONFIG_PORT, 0);
	pnp_set_iobase0(SUPERIO_CONFIG_PORT, CONFIG_SERIAL_PORT);
	pnp_set_irq0(SUPERIO_CONFIG_PORT, 4);
	pnp_set_enable(SUPERIO_CONFIG_PORT, 1);

	pnp_exit_ext_func_mode(SUPERIO_CONFIG_PORT);
}

static void chipset_init(void)
{
	sio_setup();
	superio_init();

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
