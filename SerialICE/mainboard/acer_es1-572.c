/* SPDX-License-Identifier: GPL-2.0-or-later */

const char boardname[33]="Acer ES1-572                    ";

#define PID_GPIO_COM1	0xae
#define GPP_C20	0x04a0
#define GPP_C21	0x04a8

static void chipset_init(void)
{
	southbridge_init();

	/* Configure UART2 GPIO */
	write32(P2SB(PID_GPIO_COM1, GPP_C20), 0x80000400); /* GPP_C20 UART_RX2 */
	write32(P2SB(PID_GPIO_COM1, GPP_C21), 0x80000400); /* GPP_C21 UART_TX2 */
}
