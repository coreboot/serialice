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
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301 USA
 */

#ifndef IO_H
#define IO_H

/* Memory functions */

static inline u8 read8(unsigned long addr)
{
	return *((volatile u8 *)(addr));
}

static inline u16 read16(unsigned long addr)
{
	return *((volatile u16 *)(addr));
}

static inline u32 read32(unsigned long addr)
{
	return *((volatile u32 *)(addr));
}

static inline void write8(unsigned long addr, u8 value)
{
	*((volatile u8 *)(addr)) = value;
}

static inline void write16(unsigned long addr, u16 value)
{
	*((volatile u16 *)(addr)) = value;
}

static inline void write32(unsigned long addr, u32 value)
{
	*((volatile u32 *)(addr)) = value;
}

/* IO functions */

#if defined( __ROMCC__ ) && !defined (__GNUC__)
static inline void outb(u8 value, u16 port)
{
	__builtin_outb(value, port);
}

static inline void outw(u16 value, u16 port)
{
	__builtin_outw(value, port);
}

static inline void outl(u32 value, u16 port)
{
	__builtin_outl(value, port);
}

static inline u8 inb(u16 port)
{
	return __builtin_inb(port);
}

static inline u16 inw(u16 port)
{
	return __builtin_inw(port);
}

static inline u32 inl(u16 port)
{
	return __builtin_inl(port);
}
#else
static inline void outb(u8 value, u16 port)
{
	__asm__ __volatile__("outb %b0, %w1"::"a"(value), "Nd"(port));
}

static inline void outw(u16 value, u16 port)
{
	__asm__ __volatile__("outw %w0, %w1"::"a"(value), "Nd"(port));
}

static inline void outl(u32 value, u16 port)
{
	__asm__ __volatile__("outl %0, %w1"::"a"(value), "Nd"(port));
}

static inline u8 inb(u16 port)
{
	u8 value;
	__asm__ __volatile__("inb %w1, %b0":"=a"(value): "Nd"(port));
	return value;
}

static inline u16 inw(u16 port)
{
	u16 value;
	__asm__ __volatile__("inw %w1, %w0":"=a"(value): "Nd"(port));
	return value;
}

static inline u32 inl(u16 port)
{
	u32 value;
	__asm__ __volatile__("inl %w1, %0":"=a"(value): "Nd"(port));
	return value;
}
#endif /* __ROMCC__ && !__GNUC__ */

/* MSR functions */

typedef struct { u32 lo, hi; } msr_t;

static inline msr_t rdmsr(u32 index, u32 key)
{
	msr_t result;
	__asm__ __volatile__ (
		"rdmsr"
		: "=a" (result.lo), "=d" (result.hi)
		: "c" (index), "D" (key)
	);
	return result;
}

static inline void wrmsr(u32 index, msr_t msr, u32 key)
{
	__asm__ __volatile__ (
		"wrmsr"
		: /* No outputs */
		: "c" (index), "a" (msr.lo), "d" (msr.hi), "D" (key)
	);
}

/* CPUID functions */

static inline u32 cpuid_eax(u32 op, u32 op2)
{
        u32 eax;

        __asm__("cpuid"
                : "=a" (eax)
                : "a" (op), "c" (op2)
                : "ebx", "edx" );
        return eax;
}

static inline u32 cpuid_ebx(u32 op, u32 op2)
{
        u32 eax, ebx;

        __asm__("cpuid"
                : "=b" (ebx)
                : "a" (op), "c" (op2)
                : "edx");
        return ebx;
}

static inline u32 cpuid_ecx(u32 op, u32 op2)
{
        u32 eax, ecx;

        __asm__("cpuid"
                : "=c" (ecx)
                : "a" (op), "c" (op2)
                : "ebx", "edx" );
        return ecx;
}

static inline u32 cpuid_edx(u32 op, u32 op2)
{
        u32 eax, edx;

        __asm__("cpuid"
                : "=d" (edx)
                : "a" (op), "c" (op2)
                : "ebx");
        return edx;
}

#endif
