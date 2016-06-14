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

/* This initializes Intel's ICH7 that
 * o the Super IO is accessible,
 * o legacy serial port I/O ports are decoded,
 * o the watchdog is turned off
 */

#include "config.h"

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
	// Set COM1/COM2 decode range
	pci_write_config16(PCI_ADDR(0, 0x1f, 0, 0x80), 0x0010);
	// Enable COM1/COM2/KBD/SuperIO1+2
	pci_write_config16(PCI_ADDR(0, 0x1f, 0, 0x82), 0x340b);
	// Enable HWM at 0xa00
	pci_write_config32(PCI_ADDR(0, 0x1f, 0, 0x84), 0x00fc0a01);
	// COM3 decode
	pci_write_config32(PCI_ADDR(0, 0x1f, 0, 0x88), 0x000403e9);
	// COM4 decode
	pci_write_config32(PCI_ADDR(0, 0x1f, 0, 0x8c), 0x000402e9);
	// io 0x300 decode
	pci_write_config32(PCI_ADDR(0, 0x1f, 0, 0x90), 0x00000301);

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
