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

OUTPUT_FORMAT("elf32-i386", "elf32-i386", "elf32-i386")
OUTPUT_ARCH(i386)

ALIGNED_ROMBASE = 0x100000000 - (ALIGNED_ROMSIZE);

_ROMSIZE = (1024 * 64);
_ROMBASE = 0x100000000 - (_ROMSIZE);

SECTIONS {

	. = ALIGNED_ROMBASE;

	.rom : {
		. = ALIGNED_ROMSIZE - _ROMSIZE;
		_main = . ;
		*(.rom.text);
		*(.text);
		*(.rom.data);
		*(.data);
		*(.rodata);
		*(.rodata.*);
	} =0xff

	. = 0xffffff40;
	.messages . : {
		*(.messages)
		. = 111;
		BYTE(0x00);
	}

	. = 0xffffffd0;
	.romstrap . : {
		*(.romstrap)
	}


	. = 0xfffffff0;
	.reset . : {
		*(.reset)
		. = 15;
		BYTE(0x00);
	}

	/DISCARD/ : {
		*(.comment)
		*(.note.*)
		*(.note)
	}
}
