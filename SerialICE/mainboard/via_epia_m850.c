/*
 * This file is part of the SerialICE project.
 *
 * Copyright (C) 2012  Alexandru Gagniuc <mr.nuke.me@gmail.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

const char boardname[33] = "VIA EPIA M-850                  ";

#define SUPERIO_CONFIG_PORT		0x2e

#define LPC(x)		PCI_ADDR(0, 0x11, 0, x)

static inline void pnp_enter_conf_state(u16 port)
{
	outb(0x87, port);
	outb(0x87, port);
}

static inline void pnp_exit_conf_state(u16 port)
{
	outb(0xaa, port);
}

static inline void vx900_disable_auto_reboot(void)
{
	/* Disable the GP3 timer, which is the root of all evil */
	pci_write_config8(LPC(0x98), 0);
	/* Yep, that's all it takes */
}

static void superio_init(void)
{
	vx900_disable_auto_reboot();

	pnp_enter_conf_state(SUPERIO_CONFIG_PORT);
	pnp_set_logical_device(SUPERIO_CONFIG_PORT, 0);
	pnp_set_enable(SUPERIO_CONFIG_PORT, 0);
	pnp_set_iobase0(SUPERIO_CONFIG_PORT, 0x03f8);
	pnp_set_enable(SUPERIO_CONFIG_PORT, 1);
	pnp_exit_conf_state(SUPERIO_CONFIG_PORT);
}

static void chipset_init(void)
{
	superio_init();

	__asm__ __volatile__("\
	jmp skip\n\
	.align 128\n\
	.global via_romstrap\n\
	via_romstrap:\n\
	.long 0x77886047\n\
	.long 0x00777777\n\
	.long 0x00000000\n\
	.long 0x00000000\n\
	.long 0x00888888\n\
	.long 0x00AA1111\n\
	.long 0x00000000\n\
	.long 0x00000000\n\
	skip:\n");
}
