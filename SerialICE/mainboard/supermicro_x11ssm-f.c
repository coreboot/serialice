/* SPDX-License-Identifier: GPL-2.0-or-later */

const char boardname[33]="Supermicro X11SSM-F             ";

static void chipset_init(void)
{
	southbridge_init();
	superio_init();
}
