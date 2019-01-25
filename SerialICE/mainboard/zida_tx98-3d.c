/*
 * SerialICE
 *
 * Copyright (C) 2009 Joseph Smith <joe@settoplinux.org>
 * Copyright (C) 2019 Felix Held <felix-coreboot@felixheld.de>
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

/* This is a chipset init file for the Zida TX98-3D mainboard */

#define IT8661F_ISA_PNP_PORT	0x0279	/* Write-only */
#define IT8661F_PORT		0x0370
#define IT8661F_SP1		0x01

const char boardname[33]="Zida TX98-3D                    ";

static const unsigned char init_values[] = {
	0x6a, 0xb5, 0xda, 0xed, 0xf6, 0xfb, 0x7d, 0xbe,
	0xdf, 0x6f, 0x37, 0x1b, 0x0d, 0x86, 0xc3, 0x61,
	0xb0, 0x58, 0x2c, 0x16, 0x8b, 0x45, 0xa2, 0xd1,
	0xe8, 0x74, 0x3a, 0x9d, 0xce, 0xe7, 0x73, 0x39,
};

static void superio_init(void)
{
	int i;

	/* enter mainboard pnp configuration mode for SIO at 0x370 */
	outb(0x86, IT8661F_ISA_PNP_PORT);
	outb(0x61, IT8661F_ISA_PNP_PORT);
	outb(0xaa, IT8661F_ISA_PNP_PORT);
	outb(0x55, IT8661F_ISA_PNP_PORT);

	/* Sequentially write the 32 special values */
	for (i = 0; i < sizeof(init_values); i++)
		outb(init_values[i], IT8661F_PORT);

	/* The clock default of the SIO is 24MHz which is also the frequency
	   supplied by the clock generator, so nothing to change. */

	/* COM A */
	pnp_set_logical_device(IT8661F_PORT, IT8661F_SP1);
	/* IO base and IRQ already have the right default values */
	pnp_set_enable(IT8661F_PORT, 1);

	/* Exit the configuration state */
	pnp_write_register(IT8661F_PORT, 0x20, 0x20);
}

static void chipset_init(void)
{
	superio_init();
}
