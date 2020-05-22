/* SPDX-License-Identifier: GPL-2.0-only */

const char boardname[33]="ASRock H81M-HDS                 ";

static void chipset_init(void)
{
	southbridge_init();
	superio_init(0x2e);
}
