/*
 * SerialICE
 *
 * Copyright (C) 2009 coresystems GmbH
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

/* This sets up the Super IO so up to 4 COM ports are usable */

static void superio_init(void)
{
	pnp_enter_ext_func_mode(0x2e);

	pnp_set_logical_device(0x2e, 2); // COM-A
	pnp_set_enable(0x2e, 0);
	pnp_set_iobase0(0x2e, 0x3f8);
	pnp_set_irq0(0x2e, 4);
	pnp_set_enable(0x2e, 1);

	pnp_set_logical_device(0x2e, 3); // COM-B
	pnp_set_enable(0x2e, 0);
	pnp_set_iobase0(0x2e, 0x2f8);
	pnp_set_irq0(0x2e, 3);
	pnp_set_enable(0x2e, 1);

	pnp_exit_ext_func_mode(0x2e);

	pnp_enter_ext_func_mode(0x4e);

	// Set COM3 to sane non-conflicting values
	pnp_set_logical_device(0x4e, 2); // COM-A
	pnp_set_enable(0x4e, 0);
	pnp_set_iobase0(0x4e, 0x3e8);
	pnp_set_irq0(0x4e, 11);
	pnp_set_enable(0x4e, 1);

	// Set COM4 to sane non-conflicting values
	pnp_set_logical_device(0x4e, 3); // COM-B
	pnp_set_enable(0x4e, 0);
	pnp_set_iobase0(0x4e, 0x2e8);
	pnp_set_irq0(0x4e, 10);
	pnp_set_enable(0x4e, 1);

	pnp_exit_ext_func_mode(0x4e);
}
