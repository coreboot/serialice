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
static inline void outb(uint8_t value, uint16_t port)
{
	__asm__ __volatile__("outb %b0, %w1"::"a"(value), "Nd"(port));
}

static inline void outw(uint16_t value, uint16_t port)
{
	__asm__ __volatile__("outw %w0, %w1"::"a"(value), "Nd"(port));
}

static inline void outl(uint32_t value, uint16_t port)
{
	__asm__ __volatile__("outl %0, %w1"::"a"(value), "Nd"(port));
}

static inline uint8_t inb(uint16_t port)
{
	uint8_t value;
	__asm__ __volatile__("inb %w1, %b0":"=a"(value): "Nd"(port));
	return value;
}

static inline uint16_t inw(uint16_t port)
{
	uint16_t value;
	__asm__ __volatile__("inw %w1, %w0":"=a"(value): "Nd"(port));
	return value;
}

static inline uint32_t inl(uint16_t port)
{
	uint32_t value;
	__asm__ __volatile__("inl %w1, %0":"=a"(value): "Nd"(port));
	return value;
}
#endif /* __ROMCC__ && !__GNUC__ */

/* MSR functions */

#ifdef __GNUC__
typedef struct { uint32_t lo, hi; } msr_t;

static inline msr_t rdmsr(unsigned index)
{
	msr_t result;
	__asm__ __volatile__ (
		"rdmsr"
		: "=a" (result.lo), "=d" (result.hi)
		: "c" (index), "D" (0x9c5a203a)
	);
	return result;
}

static inline void wrmsr(unsigned index, msr_t msr)
{
	__asm__ __volatile__ (
		"wrmsr"
		: /* No outputs */
		: "c" (index), "a" (msr.lo), "d" (msr.hi), "D" (0x9c5a203a)
	);
}

#else

/* Since we use romcc, we're also going to use romcc's builtin functions */
typedef __builtin_msr_t msr_t;

static inline msr_t rdmsr(unsigned long index)
{
	return __builtin_rdmsr(index);
}

static inline void wrmsr(unsigned long index, msr_t msr)
{
	__builtin_wrmsr(index, msr.lo, msr.hi);
}
#endif

/* CPUID functions */

static inline unsigned int cpuid_eax(unsigned int op)
{
        unsigned int eax;

        __asm__("cpuid"
                : "=a" (eax)
                : "0" (op)
                : "ebx", "ecx", "edx");
        return eax;
}

static inline unsigned int cpuid_ebx(unsigned int op)
{
        unsigned int eax, ebx;

        __asm__("cpuid"
                : "=a" (eax), "=b" (ebx)
                : "0" (op)
                : "ecx", "edx" );
        return ebx;
}

static inline unsigned int cpuid_ecx(unsigned int op)
{
        unsigned int eax, ecx;

        __asm__("cpuid"
                : "=a" (eax), "=c" (ecx)
                : "0" (op)
                : "ebx", "edx" );
        return ecx;
}

static inline unsigned int cpuid_edx(unsigned int op)
{
        unsigned int eax, edx;

        __asm__("cpuid"
                : "=a" (eax), "=d" (edx)
                : "0" (op)
                : "ebx", "ecx");
        return edx;
}



