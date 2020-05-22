/* SPDX-License-Identifier: GPL-2.0-or-later */
/* nuvoton serial init code from coreboot */

#include <types.h>
#include <io.h>

#define NUVOTON_ENTRY_KEY 0x87
#define NUVOTON_EXIT_KEY 0xAA

static inline void nuvoton_pnp_enter_conf_state(u16 port)
{
	outb(NUVOTON_ENTRY_KEY, port);
	outb(NUVOTON_ENTRY_KEY, port);
}

/* Disable configuration: pass exit key '0xAA' into index port dev. */
static inline void nuvoton_pnp_exit_conf_state(u16 port)
{
	outb(NUVOTON_EXIT_KEY, port);
}

/* Bring up early serial debugging output before the RAM is initialized. */
static void superio_init(u16 port)
{
	nuvoton_pnp_enter_conf_state(port);

	/* Route COM A to GPIO8 pin group */
	pnp_write_register(port, 0x2a, 0x40);

	/* Enable NCT6776_SP1 */
	pnp_set_logical_device(port, 2);
	pnp_set_enable(port, 0);
	pnp_set_iobase0(port, CONFIG_SERIAL_PORT);
	pnp_set_enable(port, 1);
	nuvoton_pnp_exit_conf_state(port);
}
