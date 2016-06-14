/*
 * SerialICE
 *
 * Copyright (C) 2012 Kyösti Mälkki <kyosti.malkki@gmail.com>
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

const char boardname[33]="Intel D845GBV2                  ";

#define SUPERIO_CONFIG_PORT	0x2e

/* Hardware specific functions */
static void southbridge_init(void)
{
	/* Set NO_REBOOT flag */
	pci_write_config8(PCI_ADDR(0, 0x1f, 0, 0xd4), 0x02);

	/* Select COM1 COM2 I/O ranges. */
	pci_write_config8(PCI_ADDR(0, 0x1f, 0, 0xe0), 0x10);

	/* Enable COM1, COM2, KBD, SIO config registers 0x2e. */
	pci_write_config16(PCI_ADDR(0, 0x1f, 0, 0xe6), 0x1403);

	/* Enable Serial IRQ */
	pci_write_config8(PCI_ADDR(0, 0x1f, 0, 0x64), 0xd0);
}

static void superio_init(void)
{
	pnp_enter_ext_func_mode_alt(SUPERIO_CONFIG_PORT);

	/* Settings for LPC47M172 with LD_NUM = 0. */
	pnp_set_logical_device(SUPERIO_CONFIG_PORT, 3); /* COM1 */
	pnp_set_enable(SUPERIO_CONFIG_PORT, 0);
	pnp_set_iobase0(SUPERIO_CONFIG_PORT, 0x3f8);
	pnp_set_irq0(SUPERIO_CONFIG_PORT, 4);
	pnp_set_enable(SUPERIO_CONFIG_PORT, 1);

	pnp_exit_ext_func_mode(SUPERIO_CONFIG_PORT);
}

static void chipset_init(void)
{
	southbridge_init();
	superio_init();
}
