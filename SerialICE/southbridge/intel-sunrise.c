/* SPDX-License-Identifier: GPL-2.0-only */

/* This initializes Intel's PCH that
 * o the watchdog is turned off,
 * o the Super IO is accessible,
 * o LPSS UART is configured if enabled,
 * o legacy serial port I/O ports are decoded if enabled
 */

#include "config.h"

/* Addresses */
#define PID_DMI		0xef
#define TCO_BASE_ADDR	0x400
#define PCIE_BASE_ADDR	0xe0000000
#define P2SB_BASE_ADDR	0xfd000000
#define DEV_UART0	PCI_ADDR(0, 0x1e, 0, 0)
#define DEV_UART1	PCI_ADDR(0, 0x1e, 1, 0)
#define DEV_UART2	PCI_ADDR(0, 0x19, 0, 0)
#define DEV_UART(x)	DEV_UART##x

/* Host bridge PCI cfg registers */
#define PCIEXBAR	0x60

/* LPC PCI cfg registers */
#define LGIR1		0x84
#define LGIR2		0x88
#define LPC_SCNT	0x64
#define  IRQEN		(1 << 7)
#define  CONTINUOUS	(1 << 6)
#define LPC_IOD		0x80
#define  COM2_RANGE	0x01 /* 0x2f8 */
#define  COM1_RANGE	0x00 /* 0x3f8 */
#define LPC_IOE		0x82
#define  COMB_EN	(1 << 1)
#define  COMA_EN	(1 << 0)

/* TCO PCI cfg registers */
#define TCOBASE		0x50
#define TCOCTL		0x54
#define  TCO_BASE_EN	(1 << 8)

/* Generic PCI cfg registers */
#define PCICMD		0x04
#define  CMD_BAR0_EN	0x02
#define PCIBAR0		0x10
#define BAREN		(1 << 0)

/* DMI PCR registers */
#define DMI_LPCLGIR1	0x2730
#define DMI_LPCLGIR2	0x2734
#define DMI_LPCIOD	0x2770
#define DMI_LPCIOE	0x2774
#define DMI_TCOBASE	0x2778
#define  TCOEN		(1 << 1)

/* TCO I/O registers */
#define TCO1_STS	(TCO_BASE_ADDR + 0x04)
#define  TIMEOUT	(1 << 3)
#define TCO2_STS	(TCO_BASE_ADDR + 0x06)
#define  SECOND_TO_STS	(1 << 1)
#define TCO1_CNT	(TCO_BASE_ADDR + 0x08)
#define  TCO_TMR_HLT	(1 << 11)

/* P2SB */
#define P2SB(pid, reg) (P2SB_BASE_ADDR + ((pid) << 16) + (reg))

/* LPSS UART MEM registers */
#define LPSS_CLK_CTL	0x200
#define  CLK_UPDATE	(1 << 31)
#define  CLK_EN		(1 << 0)
#define LPSS_RST_CTL	0x204
#define  RST_RELEASE	0x03

#define LPSS_CLOCK_DIV_M(m)	(((m) & 0x7fff) << 1)
#define LPSS_CLOCK_DIV_N(n)	(((n) & 0x7fff) << 16)

static void southbridge_init(void)
{
	u16 reg16;
	u32 reg32;

	/* Set up PCIe base */
	pci_write_config32(PCI_ADDR(0, 0, 0, PCIEXBAR), PCIE_BASE_ADDR | BAREN);

	/* Set up P2SB */
	pci_write_config32(PCI_ADDR(0, 0x1f, 1, PCIBAR0), P2SB_BASE_ADDR);
	pci_write_config32(PCI_ADDR(0, 0x1f, 1, PCICMD), CMD_BAR0_EN);

	/* Set up TCO */
	pci_write_config32(PCI_ADDR(0, 0x1f, 4, TCOBASE), TCO_BASE_ADDR);
	pci_write_config32(PCI_ADDR(0, 0x1f, 4, TCOCTL), TCO_BASE_EN);
	write32(P2SB(PID_DMI, DMI_TCOBASE), TCO_BASE_ADDR | TCOEN);

	/* Disable TCO watchdog */
	reg16 = inw(TCO1_CNT);
	reg16 |= TCO_TMR_HLT;		/* Halt TCO timer */
	outw(reg16, TCO1_CNT);
	outw(TIMEOUT, TCO1_STS);	/* Clear timeout status */
	outw(SECOND_TO_STS, TCO2_STS);	/* Clear second timeout status */

	/* Enable KBD/SuperIO/EC 2e/2f, 4e/4f */
	u16 ioe = 0x3c00;

#if CONFIG_SERIAL && CONFIG_LPSS_UART
	/* Configure LPSS UART */
	pci_write_config32(DEV_UART(CONFIG_LPSS_UART_INDEX) + PCIBAR0, CONFIG_SERIAL_BASE_ADDRESS);
	pci_write_config32(DEV_UART(CONFIG_LPSS_UART_INDEX) + PCICMD, CMD_BAR0_EN);

	/* Release reset */
	write32(CONFIG_SERIAL_BASE_ADDRESS + LPSS_RST_CTL, RST_RELEASE);

	/* Configure clock */
	write32(CONFIG_SERIAL_BASE_ADDRESS + LPSS_CLK_CTL,
		LPSS_CLOCK_DIV_N(0xc35) | LPSS_CLOCK_DIV_M(0x30) | CLK_UPDATE | CLK_EN);
#elif CONFIG_SERIAL
	/* Enable Serial IRQ */
	pci_write_config8(PCI_ADDR(0, 0x1f, 0, LPC_SCNT), IRQEN | CONTINUOUS);

	/* Set COM1/COM2 decode ranges */
	reg16 = (COM2_RANGE << 4) | COM1_RANGE;
	pci_write_config16(PCI_ADDR(0, 0x1f, 0, LPC_IOD), reg16);
	write32(P2SB(PID_DMI, DMI_LPCIOD), reg16);

	/* Enable COM1/COM2 */
	ioe |= COMB_EN | COMA_EN;

	/* COM 3/4 decode */
	pci_write_config32(PCI_ADDR(0, 0x1f, 0, LGIR1), 0x000403e8 | BAREN);	/* 0x3e8 */
	pci_write_config32(PCI_ADDR(0, 0x1f, 0, LGIR2), 0x000402e8 | BAREN);	/* 0x2e8 */
	write32(P2SB(PID_DMI, DMI_LPCLGIR1), 0x000403e8 | BAREN);
	write32(P2SB(PID_DMI, DMI_LPCLGIR2), 0x000402e8 | BAREN);
#endif

	/* Enable decodes */
	pci_write_config16(PCI_ADDR(0, 0x1f, 0, LPC_IOE), ioe);
	write32(P2SB(PID_DMI, DMI_LPCIOE), ioe);
}
