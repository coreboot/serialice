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

/*
 * This is an example chipset init file for the Roda RK886EX (Rocky 3+)
 */
#include "config.h"

const char boardname[33]="Roda RK886EX (Rocky III+)       ";

/* Hardware specific functions */

#define RCBA	0xfed1c000
#define   GCS	0x3410
#define RCBA32(x) *((volatile u32 *)(RCBA + x))

static void southbridge_init(void)
{
	u16 reg16;
	u32 reg32;

	// Set up RCBA
	pci_write_config32(PCI_ADDR(0, 0x1f, 0, 0xf0), RCBA | 1);

#if defined(CONFIG_POST_LPC)
	// port80 writes go to LPC:
	reg32 = RCBA32(GCS);
	reg32 = reg32 & ~0x04;
	RCBA32(GCS) = reg32;
#endif

	// Set up SuperIO LPC forwards
	pci_write_config8(PCI_ADDR(0, 0x1f, 0, 0x64), 0xd0);
	pci_write_config16(PCI_ADDR(0, 0x1f, 0, 0x80), 0x0007);
	pci_write_config16(PCI_ADDR(0, 0x1f, 0, 0x82), 0x3f0f);
	pci_write_config16(PCI_ADDR(0, 0x1f, 0, 0x84), 0x02e1);
	pci_write_config16(PCI_ADDR(0, 0x1f, 0, 0x86), 0x001c);
	pci_write_config32(PCI_ADDR(0, 0x1f, 0, 0x88), 0x00fc0601);
	pci_write_config32(PCI_ADDR(0, 0x1f, 0, 0x8c), 0x00040069);

	// Disable watchdog
#define PMBASE 0x500
#define TCOBASE (PMBASE + 0x60)
	pci_write_config32(PCI_ADDR(0, 0x1f, 0, 0x40), PMBASE | 1);
	pci_write_config8(PCI_ADDR(0, 0x1f, 0, 0x44), 0x80);
	reg16 = inw(TCOBASE + 0x08);
	reg16 |= (1 << 11);
	outw(reg16, TCOBASE + 0x08);
	outw(0x0008, TCOBASE + 0x04);
	outw(0x0002, TCOBASE + 0x06);
}

static void superio_init(void)
{
	pnp_enter_ext_func_mode_alt(0x2e);

        pnp_write_register(0x2e, 0x01, 0x94); // Extended Parport modes
        pnp_write_register(0x2e, 0x02, 0x88); // UART power on
        pnp_write_register(0x2e, 0x03, 0x72); // Floppy
        pnp_write_register(0x2e, 0x04, 0x01); // EPP + SPP
        pnp_write_register(0x2e, 0x14, 0x03); // Floppy
        pnp_write_register(0x2e, 0x20, (0x3f0 >> 2)); // Floppy
        pnp_write_register(0x2e, 0x23, (0x378 >> 2)); // PP base
        pnp_write_register(0x2e, 0x24, (0x3f8 >> 2)); // UART1 base
        pnp_write_register(0x2e, 0x25, (0x2f8 >> 2)); // UART2 base
        pnp_write_register(0x2e, 0x26, (2 << 4) | 0); // FDC + PP DMA
        pnp_write_register(0x2e, 0x27, (6 << 4) | 7); // FDC + PP DMA
        pnp_write_register(0x2e, 0x28, (4 << 4) | 3); // UART1,2 IRQ

        /* These are the SMI status registers in the SIO: */
        pnp_write_register(0x2e, 0x30, (0x600 >> 4)); // Runtime Register Block Base

        pnp_write_register(0x2e, 0x31, 0x00); // GPIO1 DIR
        pnp_write_register(0x2e, 0x32, 0x00); // GPIO1 POL
        pnp_write_register(0x2e, 0x33, 0x40); // GPIO2 DIR
        pnp_write_register(0x2e, 0x34, 0x00); // GPIO2 POL
        pnp_write_register(0x2e, 0x35, 0xff); // GPIO3 DIR
        pnp_write_register(0x2e, 0x36, 0x00); // GPIO3 POL
        pnp_write_register(0x2e, 0x37, 0xe0); // GPIO4 DIR
        pnp_write_register(0x2e, 0x38, 0x00); // GPIO4 POL
        pnp_write_register(0x2e, 0x39, 0x80); // GPIO4 POL

	pnp_exit_ext_func_mode(0x2e);
}

static void chipset_init(void)
{
	southbridge_init();
	superio_init();
}
