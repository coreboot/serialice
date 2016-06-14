/*
 * SerialICE
 *
 * Copyright (C) 2009 Uwe Hermann <uwe@hermann-uwe.de>
 * Copyright (C) 2010 Arne Georg Gleditsch <arne.gleditsch@numascale.com>
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

const char boardname[33]="HP DL165 G6                     ";

#define SCH4307_CONFIG_PORT	0x162e
#define SUPERIO_CONFIG_PORT	0x2e
#define SUPERIO_SP1             2

static void superio_init(void)
{
	int i;

	pnp_enter_ext_func_mode_alt(SCH4307_CONFIG_PORT);
	pnp_set_logical_device(SCH4307_CONFIG_PORT, 6); /* CMOS/RTC */
	pnp_set_iobase0(SCH4307_CONFIG_PORT, 0x70);
	pnp_set_iobase1(SCH4307_CONFIG_PORT, 0x72);
	pnp_set_enable(SCH4307_CONFIG_PORT, 3);

	pnp_set_logical_device(SCH4307_CONFIG_PORT, 3); /* Debug */
	pnp_set_iobase0(SCH4307_CONFIG_PORT, 0x80);
	pnp_set_enable(SCH4307_CONFIG_PORT, 1);

	pnp_set_logical_device(SCH4307_CONFIG_PORT, 0xa);
	pnp_set_iobase0(SCH4307_CONFIG_PORT, 0x600);
	pnp_set_enable(SCH4307_CONFIG_PORT, 1);
	pnp_exit_ext_func_mode(SCH4307_CONFIG_PORT);

	/* Enable the serial port. */
        outb(0x5a, SUPERIO_CONFIG_PORT);
	pnp_set_logical_device(SUPERIO_CONFIG_PORT, SUPERIO_SP1); /* COM1 */
	pnp_set_enable(SUPERIO_CONFIG_PORT, 0);
	pnp_set_iobase0(SUPERIO_CONFIG_PORT, 0x3f8);
	pnp_set_irq0(SUPERIO_CONFIG_PORT, 4);
	pnp_set_enable(SUPERIO_CONFIG_PORT, 1);
        outb(0xa5, SUPERIO_CONFIG_PORT);
}

static void chipset_init(void)
{
	superio_init();
}
