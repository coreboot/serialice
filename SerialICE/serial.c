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

#include "config.h"

/* Data */
#define UART_RBR 0x00
#define UART_TBR 0x00

/* Control */
#define UART_IER 0x01
#define UART_IIR 0x02
#define UART_FCR 0x02
#define UART_LCR 0x03
#define UART_MCR 0x04
#define UART_DLL 0x00
#define UART_DLM 0x01

/* Status */
#define UART_LSR 0x05
#define UART_MSR 0x06
#define UART_SCR 0x07

#if CONFIG_SERIAL_BASE_ADDRESS
#define sio_reg_read(reg)        read8(CONFIG_SERIAL_BASE_ADDRESS + 4 * (reg))
#define sio_reg_write(reg, val) write8(CONFIG_SERIAL_BASE_ADDRESS + 4 * (reg), val)
#else
#define sio_reg_read(reg)        inb(CONFIG_SERIAL_PORT + (reg))
#define sio_reg_write(reg, val) outb(val, CONFIG_SERIAL_PORT + (reg))
#endif

/* Serial functions */

static void serial_init(void)
{
#if CONFIG_SERIAL_BAUDRATE > 115200
	/* "high speed" serial requires special chip setup
	 * (to be done in superio_init), and special divisor
	 * values (implement superio_serial_divisor() for that).
	 * Maybe it requires even more, but so far that seems
	 * to be enough.
	 */
	int divisor = superio_serial_divisor(CONFIG_SERIAL_BAUDRATE);
#else
	int divisor = 115200 / CONFIG_SERIAL_BAUDRATE;
#endif
	int lcs = 3;
	sio_reg_write(UART_IER, 0x00);
	sio_reg_write(UART_FCR, 0x01);
	sio_reg_write(UART_MCR, 0x03);
	sio_reg_write(UART_LCR, 0x80 | lcs);
	sio_reg_write(UART_DLL, divisor & 0xff);
	sio_reg_write(UART_DLM, (divisor >> 8) & 0xff);
	sio_reg_write(UART_LCR, lcs);
}

static void serial_putc(u8 data)
{
	while (!(sio_reg_read(UART_LSR) & 0x20)) ;
	sio_reg_write(UART_TBR, data);
	while (!(sio_reg_read(UART_LSR) & 0x40)) ;
}

static u8 serial_getc(void)
{
	u8 val;
	while (!(sio_reg_read(UART_LSR) & 0x01)) ;

	val = sio_reg_read(UART_RBR);

#if ECHO_MODE
	serial_putc(val);
#endif
	return val;
}

/* Serial helpers */

static void serial_putstring(const char *string)
{
	/* Very simple, no %d, %x etc. */
	while (*string) {
		if (*string == '\n')
			serial_putc('\r');
		serial_putc(*string);
		string++;
	}
}

#define serial_put_nibble(nibble)	\
	if (nibble > 9)		\
		nibble += ('a' - 10);	\
	else			\
		nibble += '0';	\
	serial_putc(nibble)

static void serial_put8(u8 data)
{
	int i;
	u8 c;

	c = (data >> 4) & 0xf;
	serial_put_nibble(c);

	c = data & 0xf;
	serial_put_nibble(c);
}

static void serial_put16(u16 data)
{
	int i;
	for (i=12; i >= 0; i -= 4) {
		u8 c = (data >> i) & 0xf;
		serial_put_nibble(c);
	}
}

static void serial_put32(u32 data)
{
	int i;
	for (i=28; i >= 0; i -= 4) {
		u8 c = (data >> i) & 0xf;
		serial_put_nibble(c);
	}
}

static u8 serial_get_nibble(void)
{
	u8 ret = 0;
	u8 nibble = serial_getc();

	if (nibble >= '0' && nibble <= '9') {
		ret = (nibble - '0');
	} else if (nibble >= 'a' && nibble <= 'f') {
		ret = (nibble - 'a') + 0xa;
	} else if (nibble >= 'A' && nibble <= 'F') {
		ret = (nibble - 'A') + 0xa;
	} else {
		serial_putstring("ERROR: parsing number\n");
	}
	return ret;
}

static u8 serial_get8(void)
{
	u8 data;
	data = serial_get_nibble();
	data = data << 4;
	data |= serial_get_nibble();
	return data;
}

static u16 serial_get16(void)
{
	u16 data;

	data = serial_get_nibble();
	data = data << 4;
	data |= serial_get_nibble();
	data = data << 4;
	data |= serial_get_nibble();
	data = data << 4;
	data |= serial_get_nibble();

	return data;
}

static u32 serial_get32(void)
{
	u32 data;

	data = serial_get_nibble();
	data = data << 4;
	data |= serial_get_nibble();
	data = data << 4;
	data |= serial_get_nibble();
	data = data << 4;
	data |= serial_get_nibble();
	data = data << 4;
	data |= serial_get_nibble();
	data = data << 4;
	data |= serial_get_nibble();
	data = data << 4;
	data |= serial_get_nibble();
	data = data << 4;
	data |= serial_get_nibble();

	return data;
}
