/*
 * SerialICE
 *
 * Copyright (C) 2009 Joseph Smith <joe@settoplinux.org>
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

/* This is a chipset init file for the THOMSON IP1000 mainboard */

const char boardname[33]="THOMSON IP1000                  ";

/* Hardware specific functions */

#define PMBASE 0x400
#define TCOBASE 0x60
#define TCO1_CNT 0x08

static void southbridge_init(void)
{
	/* Prevent the TCO timer from rebooting us */
	/* Temporarily set ACPI base address (I/O space). */
	pci_write_config32(PCI_ADDR(0, 0x1f, 0, 0x40), (PMBASE | 1));
	/* Temporarily enable ACPI I/O. */
	pci_write_config8(PCI_ADDR(0, 0x1f, 0, 0x44), 0x10);
	/* Halt the TCO timer, preventing SMI and automatic reboot */
	outw(inw(PMBASE + TCOBASE + TCO1_CNT) | (1 << 11), PMBASE + TCOBASE + TCO1_CNT);
	/* Disable ACPI I/O. */
	pci_write_config8(PCI_ADDR(0, 0x1f, 0, 0x44), 0x00);
}

static void superio_init(void)
{
	/* Enter the configuration state. */
	pnp_enter_ext_func_mode_alt(0x2e);

	/* COMA */
	pnp_set_logical_device(0x2e, 4);
	pnp_set_enable(0x2e, 0);
	pnp_set_iobase0(0x2e, 0x3f8);
	pnp_set_irq0(0x2e, 4);
	pnp_set_enable(0x2e, 1);

	/* Exit the configuration state. */
	pnp_exit_ext_func_mode(0x2e);
}

static void chipset_init(void)
{
	southbridge_init();
	superio_init();
}
