/* SPDX-License-Identifier: GPL-2.0-or-later */
/* adapted ASPEED serial init code from coreboot */

#include <types.h>
#include <io.h>

#include "config.h"

#define ASPEED_SIO_PORT		0x2e
#define ASPEED_ENTRY_KEY	0xa5

static inline void aspeed_pnp_enter_conf_state(void)
{
	outb(ASPEED_ENTRY_KEY, ASPEED_SIO_PORT);
	outb(ASPEED_ENTRY_KEY, ASPEED_SIO_PORT);
}

/* Bring up early serial debugging output before the RAM is initialized. */
static void superio_init(void)
{
#if CONFIG_SERIAL

	aspeed_pnp_enter_conf_state();

	/* Enable SUARTX */
#if CONFIG_SERIAL_COM1
	pnp_set_logical_device(ASPEED_SIO_PORT, 2);
#elif CONFIG_SERIAL_COM2
	pnp_set_logical_device(ASPEED_SIO_PORT, 3);
#endif
	pnp_set_enable(ASPEED_SIO_PORT, 0);
	pnp_set_iobase0(ASPEED_SIO_PORT, CONFIG_SERIAL_PORT);
	pnp_set_enable(ASPEED_SIO_PORT, 1);

	pnp_exit_ext_func_mode(ASPEED_SIO_PORT);

	/* Delay workaround for garbled console on cold boot from G3/S5 with BMC off */
	int i;
	for(i = 0; i < 500000; i++)
		inb(0x80);

#endif // CONFIG_SERIAL
}
