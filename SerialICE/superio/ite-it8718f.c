/*
 * SerialICE
 *
 * Copyright (C) 2006 Uwe Hermann <uwe@hermann-uwe.de>
 *               2009 coresystems GmbH
 *               2014 Patrick Georgi
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

#define IT8718F_SP1  0x01 /* Com1 */
#define IT8718F_SP2  0x02 /* Com2 */

/* The base address is 0x2e or 0x4e, depending on config bytes. */
#define SIO_BASE                     0x2e
#define SIO_INDEX                    SIO_BASE
#define SIO_DATA                     (SIO_BASE + 1)

/* Global configuration registers. */
#define IT8718F_CONFIG_REG_CC        0x02 /* Configure Control (write-only). */
#define IT8718F_CONFIG_REG_LDN       0x07 /* Logical Device Number. */
#define IT8718F_CONFIG_REG_CONFIGSEL 0x22 /* Configuration Select. */
#define IT8718F_CONFIG_REG_CLOCKSEL  0x23 /* Clock Selection. */
#define IT8718F_CONFIG_REG_SWSUSP    0x24 /* Software Suspend, Flash I/F. */

static void it8718f_sio_write(char ldn, char index, char value)
{
	outb(IT8718F_CONFIG_REG_LDN, SIO_BASE);
	outb(ldn, SIO_DATA);
	outb(index, SIO_BASE);
	outb(value, SIO_DATA);
}

static void it8718f_enter_conf(void)
{
	u16 port = SIO_BASE;

	outb(0x87, port);
	outb(0x01, port);
	outb(0x55, port);
	outb((port == 0x4e) ? 0xaa : 0x55, port);
}

static void it8718f_exit_conf(void)
{
	it8718f_sio_write(0x00, IT8718F_CONFIG_REG_CC, 0x02);
}

/* Select 24MHz CLKIN (48MHz default). */
static void it8718f_24mhz_clkin(void)
{
	it8718f_enter_conf();
	it8718f_sio_write(0x00, IT8718F_CONFIG_REG_CLOCKSEL, 0x1);
	it8718f_exit_conf();
}

/* Enable the serial port(s). */
static void superio_init(void)
{
	/* (1) Enter the configuration state (MB PnP mode). */
	it8718f_enter_conf();

	/* (2) Modify the data of configuration registers. */

	/*
	 * Select the chip to configure (if there's more than one).
	 * Set bit 7 to select JP3=1, clear bit 7 to select JP3=0.
	 * If this register is not written, both chips are configured.
	 */

	/* it8718f_sio_write(0x00, IT8718F_CONFIG_REG_CONFIGSEL, 0x00); */

	/* Enable serial port(s). */
	it8718f_sio_write(IT8718F_SP1, 0x30, 0x1); /* Serial port 1 */
	it8718f_sio_write(IT8718F_SP2, 0x30, 0x1); /* Serial port 2 */

	/* Clear software suspend mode (clear bit 0). */
	it8718f_sio_write(0x00, IT8718F_CONFIG_REG_SWSUSP, 0x00);

	/* (3) Exit the configuration state (MB PnP mode). */
	it8718f_exit_conf();
}
