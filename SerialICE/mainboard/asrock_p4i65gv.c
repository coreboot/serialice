/*
 * SerialICE
 *
 * Copyright (C) 2011 Idwer Vollering <vidwer@gmail.com>
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

const char boardname[33]="ASRock P4i65GV                  ";

#define SUPERIO_CONFIG_PORT 0x2e

/* Hardware specific functions */
static void mainboard_set_ich5(void)
{
	/* COM_DEC */ /* COMA at 0x3f8, COMB at 0x3f8 */
        pci_write_config8(PCI_ADDR(0, 0x1f, 0, 0xe0), 0x0);
	/* LPC_EN */ /* FDD_LPC_EN=1, KBC_LPC_EN=1, CNF1_LPC_EN=1 */
        pci_write_config16(PCI_ADDR(0, 0x1f, 0, 0xe6), 0x1408);
	/* FB_DEC_EN1 */ /* FN_F8_EN=1, decode two 512 kilobyte flash ranges */
        pci_write_config8(PCI_ADDR(0, 0x1f, 0, 0xe3), 0x80);
	/* FB_DEC_EN2 */ /* don't decode two 1 megabyte ranges */
        pci_write_config8(PCI_ADDR(0, 0x1f, 0, 0xf0), 0x0);
	/* FUNC_DIS */ /* D31_F6_DISABLE=1 (AC97 modem) */
        pci_write_config16(PCI_ADDR(0, 0x1f, 0, 0xf2), 0x0040);
}

/* Winbond W83627HG */
static void superio_init(void)
{
        pnp_enter_ext_func_mode(SUPERIO_CONFIG_PORT);
	/* Set the clock to 48MHz */
        pnp_write_register(SUPERIO_CONFIG_PORT, 0x24, 0xc0);
        pnp_set_logical_device(SUPERIO_CONFIG_PORT, 2);
        pnp_set_enable(SUPERIO_CONFIG_PORT, 0);
        pnp_set_iobase0(SUPERIO_CONFIG_PORT, 0x3f8);
        pnp_set_irq0(SUPERIO_CONFIG_PORT, 4);
        pnp_set_enable(SUPERIO_CONFIG_PORT, 1);
        pnp_exit_ext_func_mode(SUPERIO_CONFIG_PORT);
}

static void chipset_init(void)
{
        mainboard_set_ich5();
        superio_init();
}
