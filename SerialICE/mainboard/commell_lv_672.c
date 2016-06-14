/*
 * SerialICE
 *
 * Copyright (C) 2013 Kyösti Mälkki <kyosti.malkki@gmail.com>
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

const char boardname[33]="Commell LV-672                  ";

#define SUPERIO_CONFIG_PORT	0x2e

/* Hardware specific functions */
static void southbridge_init(void)
{
	u16 reg16;

	/* Disable watchdog */
#define PMBASE 0x500
#define TCOBASE (PMBASE + 0x60)
	pci_write_config32(PCI_ADDR(0, 0x1f, 0, 0x40), PMBASE | 1);
	pci_write_config8(PCI_ADDR(0, 0x1f, 0, 0x44), 0x80);
	reg16 = inw(TCOBASE + 0x08);
	reg16 |= (1 << 11);
	outw(reg16, TCOBASE + 0x08);
	outw(0x0008, TCOBASE + 0x04);
	outw(0x0002, TCOBASE + 0x06);

	/* Select COM1 COM2 I/O ranges. */
	pci_write_config8(PCI_ADDR(0, 0x1f, 0, 0x80), 0x10);

	/* Enable COM1, COM2, KBD, SIO config registers 0x2e. */
	pci_write_config16(PCI_ADDR(0, 0x1f, 0, 0x82), 0x1403);

	/* Enable Serial IRQ */
	pci_write_config8(PCI_ADDR(0, 0x1f, 0, 0x64), 0xd0);
}

static void superio_init(void)
{
	pnp_enter_ext_func_mode(SUPERIO_CONFIG_PORT);

	/* Settings for Winbond W83627THF/THG */
	pnp_set_logical_device(SUPERIO_CONFIG_PORT, 0);
	pnp_write_register(SUPERIO_CONFIG_PORT, 0x24, 0xc2); /* Select oscillator */

	pnp_set_logical_device(SUPERIO_CONFIG_PORT, 2); /* COM1 */
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
