/*
 * SerialICE
 *
 * Copyright (C) 2010 Peter Bannis
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

const char boardname[33] = "Televideo TC7010                ";

#define SUPERIO_CONFIG_PORT	0x2e
#define PM_BASE			0xe8

static void superio_init(void)
{
	/* Set base address of power management unit */
	pnp_set_logical_device(SUPERIO_CONFIG_PORT, 8);
	pnp_set_enable(SUPERIO_CONFIG_PORT, 0);
	pnp_set_iobase0(SUPERIO_CONFIG_PORT, PM_BASE);
	pnp_set_enable(SUPERIO_CONFIG_PORT, 1);

	/* Use on-chip clock multiplier */
	outb(0x03, PM_BASE);
	outb(inb(PM_BASE + 1) | 0x07, PM_BASE + 1);

	/* Wait for the clock to stabilise */
	while (!(inb(PM_BASE + 1) & 0x80)) ;

	/* Enable the serial ports. */
	pnp_set_logical_device(SUPERIO_CONFIG_PORT, 6);	/* COM1 */
	pnp_set_enable(SUPERIO_CONFIG_PORT, 0);
	pnp_set_iobase0(SUPERIO_CONFIG_PORT, 0x3f8);
	pnp_set_irq0(SUPERIO_CONFIG_PORT, 4);
	pnp_set_enable(SUPERIO_CONFIG_PORT, 1);

	/* Set LDN 5 UART Mode */
	outb(0x21, SUPERIO_CONFIG_PORT);
	outb(inb(SUPERIO_CONFIG_PORT + 1) | (1 << 3), SUPERIO_CONFIG_PORT + 1);
	pnp_set_logical_device(SUPERIO_CONFIG_PORT, 5);	/* COM2 */
	pnp_set_enable(SUPERIO_CONFIG_PORT, 0);
	pnp_set_iobase0(SUPERIO_CONFIG_PORT, 0x2f8);
	pnp_set_irq0(SUPERIO_CONFIG_PORT, 3);
	pnp_set_enable(SUPERIO_CONFIG_PORT, 1);
}

static void chipset_init(void)
{
	superio_init();
}
