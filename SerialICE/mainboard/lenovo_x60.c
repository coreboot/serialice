/*
 * SerialICE
 *
 * Adapted by:
 *   Copyright (C) 2013 Denis 'GNUtoo' Carikli <GNUtoo@no-log.org>
 * from Coreboot's src/mainboard/lenovo/x60/romstage.c which has:
 *   Copyright (C) 2007-2009 coresystems GmbH
 *   Copyright (C) 2011 Sven Schnelle <svens@stackframe.org>
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

#define PC87392_GPIO_PIN_DEBOUNCE       0x40
#define PC87392_GPIO_PIN_PULLUP         0x04
#define PC87392_GPIO_PIN_TRIGGERS_SMI   0x02
#define PC87392_GPIO_PIN_OE             0x01
#define PC87392_GPIO_PIN_TYPE_PUSH_PULL 0x02
#define PNP_IDX_IO0                     0x60
#define BCTRL                           0x3e
#define SBR                             (1 << 6)
#define DEFAULT_GPIOBASE                 0x0480

#define PNP_DEV(PORT, FUNC) (((PORT) << 8) | (FUNC))

typedef unsigned device_t;
typedef unsigned char uint8_t;

const char boardname[33]="Lenovo X60                      ";

void udelay(unsigned usecs)
{
	int i;
	for(i = 0; i < usecs; i++)
		inb(0x80);
}

static u8 dock_read_register(int reg)
{
	outb(reg, 0x2e);
	return inb(0x2f);
}

static void dlpc_write_register(int reg, int value)
{
	outb(reg, 0x164e);
	outb(value, 0x164f);
}

static void dock_write_register(int reg, int value)
{
	outb(reg, 0x2e);
	outb(value, 0x2f);
}

static void dock_gpio_set_mode(int port, int mode, int irq)
{
	dock_write_register(0xf0, port);
	dock_write_register(0xf1, mode);
	dock_write_register(0xf2, irq);
}

int dock_connect(void)
{
	int timeout = 1000;

	outb(0x07, 0x164c);

	timeout = 1000;

	while(!(inb(0x164c) & 8) && timeout--)
		udelay(1000);

	if (!timeout) {
		/* docking failed, disable DLPC switch */
		outb(0x00, 0x164c);
		dlpc_write_register(0x30, 0x00);
		return 1;
	}

	/* Assert D_PLTRST# */
	outb(0xfe, 0x1680);
	udelay(100000);
	/* Deassert D_PLTRST# */
	outb(0xff, 0x1680);

	udelay(100000);

	/* startup 14.318MHz Clock */
	dock_write_register(0x29, 0x06);
	/* wait until clock is settled */
	timeout = 1000;
	while(!(dock_read_register(0x29) & 0x08) && timeout--)
		udelay(1000);

	if (!timeout)
		return 1;

	/* Pin  6: CLKRUN
	 * Pin 72:  #DR1
	 * Pin 19: #SMI
	 * Pin 73: #MTR
	 */
	dock_write_register(0x24, 0x37);

	/* PNF active HIGH */
	dock_write_register(0x25, 0xa0);

	/* disable FDC */
	dock_write_register(0x26, 0x01);

	/* Enable GPIO IRQ to #SMI */
	dock_write_register(0x28, 0x02);

	/* select GPIO */
	dock_write_register(0x07, 0x07);

	/* set base address */
	dock_write_register(0x60, 0x16);
	dock_write_register(0x61, 0x20);

	/* init GPIO pins */
	dock_gpio_set_mode(0x00, PC87392_GPIO_PIN_DEBOUNCE |
			   PC87392_GPIO_PIN_PULLUP, 0x00);

	dock_gpio_set_mode(0x01, PC87392_GPIO_PIN_DEBOUNCE |
			   PC87392_GPIO_PIN_PULLUP,
			   PC87392_GPIO_PIN_TRIGGERS_SMI);

	dock_gpio_set_mode(0x02, PC87392_GPIO_PIN_PULLUP, 0x00);
	dock_gpio_set_mode(0x03, PC87392_GPIO_PIN_PULLUP, 0x00);
	dock_gpio_set_mode(0x04, PC87392_GPIO_PIN_PULLUP, 0x00);
	dock_gpio_set_mode(0x05, PC87392_GPIO_PIN_PULLUP, 0x00);
	dock_gpio_set_mode(0x06, PC87392_GPIO_PIN_PULLUP, 0x00);
	dock_gpio_set_mode(0x07, PC87392_GPIO_PIN_PULLUP, 0x02);

	dock_gpio_set_mode(0x10, PC87392_GPIO_PIN_DEBOUNCE |
			   PC87392_GPIO_PIN_PULLUP,
			   PC87392_GPIO_PIN_TRIGGERS_SMI);

	dock_gpio_set_mode(0x11, PC87392_GPIO_PIN_PULLUP, 0x00);
	dock_gpio_set_mode(0x12, PC87392_GPIO_PIN_PULLUP, 0x00);
	dock_gpio_set_mode(0x13, PC87392_GPIO_PIN_PULLUP, 0x00);
	dock_gpio_set_mode(0x14, PC87392_GPIO_PIN_PULLUP, 0x00);
	dock_gpio_set_mode(0x15, PC87392_GPIO_PIN_PULLUP, 0x00);
	dock_gpio_set_mode(0x16, PC87392_GPIO_PIN_PULLUP |
			   PC87392_GPIO_PIN_OE , 0x00);

	dock_gpio_set_mode(0x17, PC87392_GPIO_PIN_PULLUP, 0x00);

	dock_gpio_set_mode(0x20, PC87392_GPIO_PIN_TYPE_PUSH_PULL |
			   PC87392_GPIO_PIN_OE, 0x00);

	dock_gpio_set_mode(0x21, PC87392_GPIO_PIN_TYPE_PUSH_PULL |
			   PC87392_GPIO_PIN_OE, 0x00);

	dock_gpio_set_mode(0x22, PC87392_GPIO_PIN_PULLUP, 0x00);
	dock_gpio_set_mode(0x23, PC87392_GPIO_PIN_PULLUP, 0x00);
	dock_gpio_set_mode(0x24, PC87392_GPIO_PIN_PULLUP, 0x00);
	dock_gpio_set_mode(0x25, PC87392_GPIO_PIN_PULLUP, 0x00);
	dock_gpio_set_mode(0x26, PC87392_GPIO_PIN_PULLUP, 0x00);
	dock_gpio_set_mode(0x27, PC87392_GPIO_PIN_PULLUP, 0x00);

	dock_gpio_set_mode(0x30, PC87392_GPIO_PIN_PULLUP, 0x00);
	dock_gpio_set_mode(0x31, PC87392_GPIO_PIN_PULLUP, 0x00);
	dock_gpio_set_mode(0x32, PC87392_GPIO_PIN_PULLUP, 0x00);
	dock_gpio_set_mode(0x33, PC87392_GPIO_PIN_PULLUP, 0x00);
	dock_gpio_set_mode(0x34, PC87392_GPIO_PIN_PULLUP, 0x00);

	dock_gpio_set_mode(0x35, PC87392_GPIO_PIN_PULLUP |
			   PC87392_GPIO_PIN_OE, 0x00);

	dock_gpio_set_mode(0x36, PC87392_GPIO_PIN_PULLUP, 0x00);
	dock_gpio_set_mode(0x37, PC87392_GPIO_PIN_PULLUP, 0x00);

	/* enable GPIO */
	dock_write_register(0x30, 0x01);

	outb(0x00, 0x1628);
	outb(0x00, 0x1623);
	outb(0x82, 0x1622);
	outb(0xff, 0x1624);

	/* Enable USB and Ultrabay power */
	outb(0x03, 0x1628);

	dock_write_register(0x07, 0x03);
	dock_write_register(0x30, 0x01);

	return 0;
}

void pnp_write_config(device_t dev, uint8_t reg, uint8_t value)
{
	unsigned port = dev >> 8;
	outb(reg, port );
	outb(value, port +1);
}
uint8_t pnp_read_config(device_t dev, uint8_t reg)
{
	unsigned port = dev >> 8;
	outb(reg, port);
	return inb(port +1);
}

void pnp_set_iobase(device_t dev, unsigned index, unsigned iobase)
{
	pnp_write_config(dev, index + 0, (iobase >> 8) & 0xff);
	pnp_write_config(dev, index + 1, iobase & 0xff);
}

#if 0 //TODO
void pnp_set_logical_device(device_t dev)
{
	unsigned device = dev & 0xff;
	pnp_write_config(dev, 0x07, device);
}
#endif
static void early_superio_config(void)
{
	int timeout = 100000;
	device_t dev = PNP_DEV(0x2e, 3);

	pnp_write_config(dev, 0x29, 0x06);

	while (!(pnp_read_config(dev, 0x29) & 0x08) && timeout--)
		udelay(1000);

	/* Enable COM1 */
	pnp_set_logical_device(0x2e, 3);
	pnp_set_iobase(dev, PNP_IDX_IO0, 0x3f8);
	pnp_set_enable(dev, 1);
}

static void ich7_enable_lpc(void)
{
	// Enable Serial IRQ
	pci_write_config8(PCI_ADDR(0, 0x1f, 0, 0x64), 0xd0);
	// decode range
	pci_write_config16(PCI_ADDR(0, 0x1f, 0, 0x80), 0x0210);
	// decode range
	pci_write_config16(PCI_ADDR(0, 0x1f, 0, 0x82), 0x1f0d);

	/* range 0x1600 - 0x167f */
	pci_write_config16(PCI_ADDR(0, 0x1f, 0, 0x84), 0x1601);
	pci_write_config16(PCI_ADDR(0, 0x1f, 0, 0x86), 0x007c);

	/* range 0x15e0 - 0x10ef */
	pci_write_config16(PCI_ADDR(0, 0x1f, 0, 0x88), 0x15e1);
	pci_write_config16(PCI_ADDR(0, 0x1f, 0, 0x8a), 0x000c);

	/* range 0x1680 - 0x169f */
	pci_write_config16(PCI_ADDR(0, 0x1f, 0, 0x8c), 0x1681);
	pci_write_config16(PCI_ADDR(0, 0x1f, 0, 0x8e), 0x001c);
}

static void dlpc_gpio_set_mode(int port, int mode)
{
	dlpc_write_register(0xf0, port);
	dlpc_write_register(0xf1, mode);
}

static void dlpc_gpio_init(void)
{
	/* Select GPIO module */
	dlpc_write_register(0x07, 0x07);
	/* GPIO Base Address 0x1680 */
	dlpc_write_register(0x60, 0x16);
	dlpc_write_register(0x61, 0x80);

	/* Activate GPIO */
	dlpc_write_register(0x30, 0x01);

	dlpc_gpio_set_mode(0x00, 3);
	dlpc_gpio_set_mode(0x01, 3);
	dlpc_gpio_set_mode(0x02, 0);
	dlpc_gpio_set_mode(0x03, 3);
	dlpc_gpio_set_mode(0x04, 4);
	dlpc_gpio_set_mode(0x20, 4);
	dlpc_gpio_set_mode(0x21, 4);
	dlpc_gpio_set_mode(0x23, 4);
}


static void southbridge_init(void)
{
#if 0 //TODO
	if (bist == 0)
		enable_lapic();
#endif
	/* Force PCIRST# */
	pci_write_config16(PCI_ADDR(0, 0x1e, 0, BCTRL), SBR);
	udelay(200 * 1000);
	pci_write_config16(PCI_ADDR(0, 0x1e, 0, BCTRL), 0);

	ich7_enable_lpc();
}

int dock_present(void)
{
	return !((inb(DEFAULT_GPIOBASE + 0x0c) >> 13) & 1);
}

static u8 dlpc_read_register(int reg)
{
	outb(reg, 0x164e);
	return inb(0x164f);
}

int dlpc_init(void)
{
	int timeout = 1000;

	/* Enable 14.318MHz CLK on CLKIN */
	dlpc_write_register(0x29, 0xa0);
	while(!(dlpc_read_register(0x29) & 0x10) && timeout--)
		udelay(1000);

	if (!timeout)
		return 1;

	/* Select DLPC module */
	dlpc_write_register(0x07, 0x19);
	/* DLPC Base Address 0x164c */
	dlpc_write_register(0x60, 0x16);
	dlpc_write_register(0x61, 0x4c);
	/* Activate DLPC */
	dlpc_write_register(0x30, 0x01);

	dlpc_gpio_init();

	return 0;
}

static void superio_init(void)
{
	dlpc_init();
	/* dock_init initializes the DLPC switch on
	 *  thinpad side, so this is required even
	 *  if we're undocked.
	 */
	if (dock_present()) {
		dock_connect();
		early_superio_config();
	}
}

static void chipset_init(void)
{
	southbridge_init();
	superio_init();
}
