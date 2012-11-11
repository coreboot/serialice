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
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301 USA
 */

/* This is a chipset init file for the Intel D945GCLF mainboard */
#include "config.h"

const char boardname[33]="Intel D945GCLF                  ";

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
	outb(0x23, 0x80);
#endif

	// Enable Serial IRQ
	pci_write_config8(PCI_ADDR(0, 0x1f, 0, 0x64), 0xd0);
	// Set COM1 decode range
	pci_write_config16(PCI_ADDR(0, 0x1f, 0, 0x80), 0x0010);
	// Enable COM1
	pci_write_config16(PCI_ADDR(0, 0x1f, 0, 0x82), 0x140d);
	// Enable SIO PM Events at 0x680
	pci_write_config32(PCI_ADDR(0, 0x1f, 0, 0x84), 0x007c0681);

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

	pnp_set_logical_device(0x2e, 4); // COM-A
	pnp_set_enable(0x2e, 0);
	pnp_set_iobase0(0x2e, 0x3f8);
	pnp_set_irq0(0x2e, 4);
	pnp_set_enable(0x2e, 1);

	pnp_set_logical_device(0x2e, 10); // PM
	pnp_set_enable(0x2e, 0);
	pnp_set_iobase0(0x2e, 0x680);
	pnp_set_irq0(0x2e, 3);
	pnp_set_enable(0x2e, 1);

	pnp_exit_ext_func_mode(0x2e);
}

static void chipset_init(void)
{
	southbridge_init();
	superio_init();
}

