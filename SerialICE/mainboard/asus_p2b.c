/*
 * SerialICE
 *
 * Copyright (C) 2009 Mark Marshall <mark.marshall@csr.com>
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

/* This is a chipset init file for the ASUS P2B mainboard.  */

const char boardname[33]="ASUS P2B                        ";

#define PNP_PORT                  0x3f0

static void superio_init(void)
{
	/* Enter the configuration state. */
	pnp_enter_ext_func_mode(PNP_PORT);

	/* COMA */
	pnp_set_logical_device(PNP_PORT, 2);
	pnp_set_enable(PNP_PORT, 0);
	pnp_set_iobase0(PNP_PORT, CONFIG_SERIAL_PORT);
	pnp_set_irq0(PNP_PORT, 4);
	pnp_set_enable(PNP_PORT, 1);

	/* Exit the configuration state. */
	pnp_exit_ext_func_mode(PNP_PORT);
}

static void chipset_init(void)
{
	superio_init();
}
