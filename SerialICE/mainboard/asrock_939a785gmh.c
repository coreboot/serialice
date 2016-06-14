/*
 * SerialICE
 *
 * Copyright (C) 2006 Uwe Hermann <uwe@hermann-uwe.de>
 * Copyright (C) 2009 Rudolf Marek <r.marek@assembler.cz>
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

const char boardname[33]="Asrock 939a785gmh               ";

#define SUPERIO_CONFIG_PORT		0x2e

static void superio_init(void)
{
	pnp_enter_ext_func_mode(SUPERIO_CONFIG_PORT);

	/* Enable the serial port. */
	pnp_set_logical_device(SUPERIO_CONFIG_PORT, 2); /* COM1 */
	pnp_set_enable(SUPERIO_CONFIG_PORT, 0);
	pnp_set_iobase0(SUPERIO_CONFIG_PORT, 0x3f8);
	pnp_set_irq0(SUPERIO_CONFIG_PORT, 4);
	pnp_set_enable(SUPERIO_CONFIG_PORT, 1);

	pnp_exit_ext_func_mode(SUPERIO_CONFIG_PORT);
}

static void chipset_init(void)
{

	/* Enable LPC decoding  */
	pci_write_config8(PCI_ADDR(0, 0x14, 3, 0x44), (1<<6));
	pci_write_config8(PCI_ADDR(0, 0x14, 3, 0x48), (1 << 1) | (1 << 0));

	superio_init();
}
