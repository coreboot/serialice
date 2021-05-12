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

#include "serialice.h"

	.section ".start", "a"
	.code32

_c_main:
	movl	%eax, %ebp

	/* Enable FPU registers */
	movl	%cr0, %eax
	andl	$~(1 << 2), %eax
	movl	%eax, %cr0

#ifdef CONFIG_HAVE_SSE
	/* enable SSE */
	movl	%cr4, %eax
	orl	$(3<<9), %eax
	movl	%eax, %cr4
#endif

	movl	%ebp, %eax

	/* Setting up the stack (xmmstack only?) */
	xorl	%esp,%esp
	movl	$0x7f,%esp #Start address of XMM/MMX stack.
	movl	%esp,%dr2  #DR2 contains stack(low byte) and offset(hi byte)

	jmp	main

_pmode_start:
	/* Set up protected mode registers */
	lgdt	%cs:gdt
	ljmp	$0x08, $1f
1:
	movl	%eax, %ebp
	movw	$0x10, %ax
	movw	%ax, %ds
	movw	%ax, %es
	movw	%ax, %ss
	movw	%ax, %fs
	movw	%ax, %gs

	movl	%ebp, %eax

	jmp	_c_main

	.code16
_start:
	/* Go to protected mode */
	cli
	movl	%eax, %ebp
	xorl	%eax, %eax
	movl	%eax, %cr3
	movw	%cs, %ax
	shlw	$4, %ax
	movw	$gdt, %bx
	subw	%ax, %bx
	data32	lgdt %cs:(%bx)

	movl	%cr0, %eax
	andl	$0x7ffaffd1, %eax
	orl	$0x60000001, %eax
	movl	%eax, %cr0

	movl	%ebp, %eax
	data32	ljmp $0x08, $_pmode_start


	/* GDT */
	.align 4
gdt:
	.word	gdt_size
	.long	gdt
	.word	0

	.word	0xffff, 0x0000
	.byte	0x00, 0x9b, 0xcf, 0x00

	.word	0xffff, 0x0000
	.byte	0x00, 0x93, 0xcf, 0x00
gdt_size = (. - gdt - 1)

	/* Messages */
	.section ".messages", "a", @progbits
	.globl	program_name
program_name:
	.ascii "SerialICE v" VERSION " (" __DATE__ ")"
	.byte 0x00
	.globl program_copyright
program_copyright:
	.string "Copyright (C) 2009 coresystems GmbH."

	/* Reset Vector */
	.section ".reset", "a"
	.globl	reset_vector
reset_vector:
	.code16
	jmp _start

	/* VIA romstrap */
	.section ".romstrap", "a", @progbits
#ifdef CONFIG_VIA_ROMSTRAP
/* The pointer to above table should be at 0xffffd,
 * the table itself MUST be aligned to 128B it seems!
 */
	.long via_romstrap
#endif

#ifdef CONFIG_CK804_ROMSTRAP
/* Pointers to nvidia tables at 0xffffe0 */
	. = 0x10;
	.long ck804_romstrap
	.long ck804_romstrap
	.long ck804_romstrap
	.long ck804_romstrap
#endif

#ifdef CONFIG_MCP55_ROMSTRAP
/* Pointers to nvidia tables at 0xffffe0 */
	. = 0x10;
	.long mcp55_romstrap
	.long mcp55_romstrap
	.long mcp55_romstrap
	.long mcp55_romstrap
#endif
