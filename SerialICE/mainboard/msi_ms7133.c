/*
 * SerialICE
 *
 * Copyright (C) 2009 Uwe Hermann <uwe@hermann-uwe.de>
 * Copyright (C) 2013 Kyösti Mälkki <kyosti.malkki@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
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

const char boardname[33]="MSI MS7133                      ";

#define PMBASE			0x40
#define COM_DEC			0x80
#define LPC_EN			0x82

#define PMBASE_ADDR		0x500
#define TCOBASE			(PMBASE + 0x60)
#define TCO1_STS		(TCOBASE + 0x04)
#define TCO2_STS		(TCOBASE + 0x06)
#define TCO1_CNT		(TCOBASE + 0x08)

#define SUPERIO_CONFIG_PORT	0x4e

static void southbridge_init(void)
{
	u16 reg16;

	/* Disable watchdog. */
	pci_write_config32(PCI_ADDR(0, 0x1f, 0, PMBASE), PMBASE_ADDR | 1);
	reg16 = inw(TCO1_CNT);
	reg16 |= (1 << 11);		/* Halt TCO timer. */
	outw(reg16, TCO1_CNT);
	outw(0x0008, TCO1_STS);		/* Clear timeout status. */
	outw(0x0002, TCO2_STS);		/* Clear second timeout status. */

	/* Set COM1/COM2 decode range. */
	pci_write_config8(PCI_ADDR(0, 0x1f, 0, COM_DEC), 0x10);

	/* Enable COM1, COM2, and Super I/O config registers 0x2e/0x4e. */
	pci_write_config16(PCI_ADDR(0, 0x1f, 0, LPC_EN), 0x3003);
}

static void superio_init(void)
{
	pnp_enter_ext_func_mode(SUPERIO_CONFIG_PORT);

	/* Set CLKSEL=1 to select 48 MHz (otherwise serial won't work). */
	pnp_write_register(SUPERIO_CONFIG_PORT, 0x24, 0xc6);

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
