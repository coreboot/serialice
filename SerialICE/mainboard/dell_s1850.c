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

/* This is a chipset init file for the Dell S1850 */

const char boardname[33]="DELL S1850                      ";

/* Hardware specific functions */
static void mainboard_set_ich5(void)
{
	/* coma is 0x3f8 , comb is 0x2f8*/
	pci_write_config8(PCI_ADDR(0, 0x1f, 0, 0xe0), 0x10);
	/* enable decoding of various devices */
	pci_write_config16(PCI_ADDR(0, 0x1f, 0, 0xe6), 0x140f);
	/* 1M flash */
	pci_write_config8(PCI_ADDR(0, 0x1f, 0, 0xe3), 0xc0);
	pci_write_config8(PCI_ADDR(0, 0x1f, 0, 0xf0), 0x0);
	/* disable certain devices -- see data sheet -- this is from
	 * dell settings via lspci
	 * Note that they leave SMBUS disabled -- 8f6f.
	 * we leave it enabled and visible in config space -- 8f66
	 */
	pci_write_config16(PCI_ADDR(0, 0x1f, 0, 0xf2), 0x8f66);
}


/* IPMI garbage. This is all test stuff, if it really works we'll move it somewhere
 */

#define nftransport  0xc

#define OBF (1 << 0)
#define IBF (1 << 1)

#define ipmidata  0xca0
#define ipmicsr  0xca4


static inline void  ibfzero(void)
{
	while(inb(ipmicsr) &  IBF)
		;
}
static inline void  clearobf(void)
{
	(void) inb(ipmidata);
}

static inline void  waitobf(void)
{
	while((inb(ipmicsr) &  OBF) == 0)
		;
}

/* quite possibly the stupidest interface ever designed. */
static inline void  first_cmd_byte(unsigned char byte)
{
	ibfzero();
	clearobf();
	outb(0x61, ipmicsr);
	ibfzero();
	clearobf();
	outb(byte, ipmidata);
}

static inline void  next_cmd_byte(unsigned char byte)
{
	ibfzero();
	clearobf();
	outb(byte, ipmidata);
}

static inline void  last_cmd_byte(unsigned char byte)
{
	outb(0x62, ipmicsr);

	ibfzero();
	clearobf();
	outb(byte,  ipmidata);
}

static inline void read_response_byte(void)
{
	int val = -1;
	if ((inb(ipmicsr)>>6) != 1)
		return;

	ibfzero();
	waitobf();
	val = inb(ipmidata);
	outb(0x68, ipmidata);

	/* see if it is done */
	if ((inb(ipmicsr)>>6) != 1){
		/* wait for the dummy read. Which describes this protocol */
		waitobf();
		(void)inb(ipmidata);
	}
}

static inline void ipmidelay(void)
{
	int i;
	for(i = 0; i < 1000; i++) {
		inb(0x80);
	}
}

static inline void bmc_foad(void)
{
	unsigned char c;
	/* be safe; make sure it is really ready */
	while ((inb(ipmicsr)>>6)) {
		outb(0x60, ipmicsr);
		inb(ipmidata);
	}
	first_cmd_byte(nftransport << 2);
	ipmidelay();
	next_cmd_byte(0x12);
	ipmidelay();
	next_cmd_byte(2);
	ipmidelay();
	last_cmd_byte(3);
	ipmidelay();
}

static void superio_init(void)
{
	pnp_enter_ext_func_mode(0x2e);
	pnp_set_logical_device(0x2e, 3); // COM-A
	pnp_set_enable(0x2e, 0);
	pnp_set_iobase0(0x2e, 0x3f8);
	pnp_set_irq0(0x2e, 4);
	pnp_set_enable(0x2e, 1);
	pnp_exit_ext_func_mode(0x2e);
}

static void chipset_init(void)
{
	mainboard_set_ich5();
	bmc_foad();
	superio_init();
}
