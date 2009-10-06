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

/* PCI access functions */

#define PCI_ADDR(_bus, _dev, _fn, _reg) \
	(0x80000000 | (_bus << 16) | (_dev << 11) | (_fn << 8) | (_reg))

static void pci_write_config8(u32 addr, u8 value)
{
        outl(addr & ~3, 0xcf8);
        outb(value, 0xcfc + (addr & 3));
}

static void pci_write_config16(u32 addr, u16 value)
{
        outl(addr & ~3, 0xcf8);
        outw(value, 0xcfc + (addr & 2));
}

static void pci_write_config32(u32 addr, u32 value)
{
        outl(addr & ~3, 0xcf8);
        outl(value, 0xcfc);
}

/* PnP / SuperIO access functions */

static inline void pnp_enter_ext_func_mode(u16 port)
{
        outb(0x87, port);
        outb(0x87, port);
}

static inline void pnp_enter_ext_func_mode_alt(u16 port)
{
        outb(0x55, port);
}

static void pnp_exit_ext_func_mode(u16 port)
{
        outb(0xaa, port);
}

static inline void pnp_write_register(u16 port, u8 reg, u8 value)
{
	outb(reg, port);
	outb(value, port +1);
}

static inline void pnp_set_logical_device(u8 port, u8 device)
{
	pnp_write_register(port, 0x07, device);
}

static inline void pnp_set_enable(u16 port, u8 enable)
{
	pnp_write_register(port, 0x30, enable);
}

static inline void pnp_set_iobase0(u16 port, u16 iobase)
{
	pnp_write_register(port, 0x60, (iobase >> 8) & 0xff);
	pnp_write_register(port, 0x61, iobase & 0xff);
}

static inline void pnp_set_irq0(u16 port, u8 irq)
{
	pnp_write_register(port, 0x70, irq);
}

#include MAINBOARD

