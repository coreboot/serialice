/*
 * SerialICE
 *
 * Copyright (C) 2011 Nils Jacobs
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

/* This is a chipset init file for the WYSE S50 thin client.  */

const char boardname[33]="WYSE S50                        ";

#define GPIO_IO_BASE		0x6100

#define GL0_GLIU1		2
#define GL1_PCI			4
#define MSR_GLIU1		(GL0_GLIU1	<< 29)		/* 4000xxxx */
#define MSR_PCI			(GL1_PCI << 26) + MSR_GLIU1	/* 5000xxxx */
#define GLPCI_ExtMSR		(MSR_PCI + 0x201E)

#define CS5536_DEV_NUM		0x0F				/* default PCI device number for CS5536 */
#define CS5536_GLINK_PORT_NUM	0x02				/* port of the SouthBridge */
#define NB_PCI			((2	<< 29) + (4 << 26))	/* NB GLPCI is in the same location on all Geodes. */
#define SB_SHIFT		20				/* 29 -> 26 -> 23 -> 20...... When making a SB address uses this shift. */
#define MSR_SB			((CS5536_GLINK_PORT_NUM << 23) + NB_PCI)/* address to the SouthBridge */
#define MSR_SB_MDD		((4 << SB_SHIFT) + MSR_SB)	/* 5140xxxx, a.k.a. DIVIL = Diverse Integrated Logic device */

#define MDD_LBAR_GPIO		(MSR_SB_MDD + 0x0C)
#define MDD_LEG_IO		(MSR_SB_MDD + 0x14)
#define MDD_IRQM_YHIGH		(MSR_SB_MDD + 0x21)
#define MDD_UART1_CONF		(MSR_SB_MDD + 0x3A)

#define GPIOL_8_SET		(1 << 8)
#define GPIOL_9_SET		(1 << 9)

#define GPIOL_OUTPUT_ENABLE	(0x04)
#define GPIOL_OUT_AUX1_SELECT	(0x10)
#define GPIOL_INPUT_ENABLE	(0x20)
#define GPIOL_IN_AUX1_SELECT	(0x34)
#define GPIOL_PULLUP_ENABLE	(0x18)

static void cs5536_setup_extmsr(void)
{
	msr_t msr;

	/* forward MSR access to CS5536_GLINK_PORT_NUM to CS5536_DEV_NUM */
	msr.hi = msr.lo = 0x00000000;
	msr.lo = CS5536_DEV_NUM << (unsigned char)((CS5536_GLINK_PORT_NUM - 1) * 8);
	wrmsr(GLPCI_ExtMSR, msr, 0x9c5a203a);
}

static void cs5536_setup_iobase(void)
{
	msr_t msr;

	/* setup LBAR for GPIO */
	msr.hi = 0x0000f001;
	msr.lo = GPIO_IO_BASE;
	wrmsr(MDD_LBAR_GPIO, msr, 0x9c5a203a);
}

static void wyse_s50_serial_init(void)
{
	msr_t msr;
	/* COM1 */

	/* Set the address to 0x3F8. */
	msr = rdmsr(MDD_LEG_IO, 0x9c5a203a);
	msr.lo |= 0x7 << 16;
	wrmsr(MDD_LEG_IO, msr, 0x9c5a203a);

	/* Set the IRQ to 4. */
	msr = rdmsr(MDD_IRQM_YHIGH, 0x9c5a203a);
	msr.lo |= 0x4 << 24;
	wrmsr(MDD_IRQM_YHIGH, msr, 0x9c5a203a);

	/* GPIO8 - UART1_TX */
	/* Set: Output Enable (0x4) */
	outl(GPIOL_8_SET, GPIO_IO_BASE + GPIOL_OUTPUT_ENABLE);
	/* Set: OUTAUX1 Select (0x10) */
	outl(GPIOL_8_SET, GPIO_IO_BASE + GPIOL_OUT_AUX1_SELECT);

	/* GPIO9 - UART1_RX */
	/* Set: Input Enable   (0x20) */
	outl(GPIOL_9_SET, GPIO_IO_BASE + GPIOL_INPUT_ENABLE);
	/* Set: INAUX1 Select (0x34) */
	outl(GPIOL_9_SET, GPIO_IO_BASE + GPIOL_IN_AUX1_SELECT);

	/* Set: GPIO 8 + 9 Pull Up (0x18) */
	outl(GPIOL_8_SET | GPIOL_9_SET,
	     GPIO_IO_BASE + GPIOL_PULLUP_ENABLE);

	/* Enable COM1.
	 *
	 * Bit 1 = device enable
	 * Bit 4 = allow access to the upper banks
	 */
	msr.lo = (1 << 4) | (1 << 1);
	msr.hi = 0;
	wrmsr(MDD_UART1_CONF, msr, 0x9c5a203a);
}

static void chipset_init(void)
{
cs5536_setup_extmsr();
cs5536_setup_iobase();
wyse_s50_serial_init();
}
