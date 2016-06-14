/*
 * SerialICE
 *
 * Copyright (C) 2006 Uwe Hermann <uwe@hermann-uwe.de>
 * Copyright (C) 2010 Rudolf Marek <r.marek@assembler.cz>
 * Copyright (C) 2010 Xavi Drudis Ferran <xdrudis@tinet.cat>
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

const char boardname[33]="ASUS M4A77TD-PRO                ";

#define SUPERIO_CONFIG_PORT		0x2e

static void superio_init(void)
{
	pnp_enter_ext_func_mode_ite(SUPERIO_CONFIG_PORT);

	/* Disable the watchdog. */
	pnp_set_logical_device(SUPERIO_CONFIG_PORT, 7);
	pnp_write_register(SUPERIO_CONFIG_PORT, 0x72, 0x00);

	/* Enable the serial port. */
	pnp_set_logical_device(SUPERIO_CONFIG_PORT, 1); /* COM1 */
	pnp_set_enable(SUPERIO_CONFIG_PORT, 0);
	pnp_set_iobase0(SUPERIO_CONFIG_PORT, 0x3f8);
	pnp_set_irq0(SUPERIO_CONFIG_PORT, 4);
	pnp_set_enable(SUPERIO_CONFIG_PORT, 1);

	pnp_exit_ext_func_mode_ite(SUPERIO_CONFIG_PORT);
}

static void chipset_init(void)
{
        /* stop the mainboard from rebooting */
        /* inspired by coreboot, src/southbridge/amd/sb700/sb700_early_init.c,
         * sb700_lpc_init(), where the comment says:
         * NOTE: Set BootTimerDisable, otherwise it would keep rebooting!!
         * This bit has no meaning if debug strap is not enabled. So if the
         * board keeps rebooting and the code fails to reach here, we could
         * disable the debug strap first.
         */
        u32 reg32 = pci_read_config32(PCI_ADDR(0, 0x14, 0, 0x4C));
        reg32 |= 1 << 31;
	pci_write_config32(PCI_ADDR(0, 0x14, 0, 0x4C), reg32);


	/* Enable LPC decoding  */
	pci_write_config8(PCI_ADDR(0, 0x14, 3, 0x44), (1<<6));
	pci_write_config8(PCI_ADDR(0, 0x14, 3, 0x48), (1 << 1) | (1 << 0));

	superio_init();
}
