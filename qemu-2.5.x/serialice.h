/*
 * QEMU PC System Emulator
 *
 * Copyright (c) 2009 coresystems GmbH
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#ifndef SERIALICE_H
#define SERIALICE_H

#include "config-host.h"

#include "config.h"
#if !defined(HOST_X86_64) && !defined(HOST_I386)
#error "SerialICE currently only supports x86 and x64 platforms."
#endif

#define READ_FROM_QEMU		(1 << 0)
#define READ_FROM_SERIALICE	(1 << 1)

#define WRITE_TO_QEMU		(1 << 0)
#define WRITE_TO_SERIALICE	(1 << 1)

extern const char *serialice_device;
extern int serialice_active;

uint32_t serialice_io_read(uint16_t port, unsigned int size);
void serialice_io_write(uint16_t port, unsigned int size, uint32_t data);

typedef struct CPUX86State CPUX86State;

uint64_t serialice_rdmsr(CPUX86State *env, uint32_t addr, uint32_t key);
void serialice_wrmsr(CPUX86State *env, uint64_t data, uint32_t addr, uint32_t key);

typedef struct {
    uint32_t eax, ebx, ecx, edx;
} cpuid_regs_t;

cpuid_regs_t serialice_cpuid(CPUX86State *env, uint32_t eax, uint32_t ecx);

int serialice_handle_load(uint32_t addr, uint32_t * result,
                          unsigned int data_size);
void serialice_log_load(int caught, uint32_t addr, uint32_t result,
                        unsigned int data_size);
int serialice_handle_store(uint32_t addr, uint32_t val, unsigned int data_size);

/* serialice protocol */
typedef struct {
    void (*version) (void);
    void (*mainboard) (void);
    uint32_t (*io_read) (uint16_t port, unsigned int size);
    void (*io_write) (uint16_t port, unsigned int size, uint32_t data);
    uint32_t (*load) (uint32_t addr, unsigned int size);
    void (*store) (uint32_t addr, unsigned int size, uint32_t data);
    void (*rdmsr) (uint32_t addr, uint32_t key, uint32_t * hi, uint32_t * lo);
    void (*wrmsr) (uint32_t addr, uint32_t key, uint32_t hi, uint32_t lo);
    void (*cpuid) (uint32_t eax, uint32_t ecx, cpuid_regs_t * ret);
} SerialICE_target;

const SerialICE_target *serialice_serial_init(void);
void serialice_serial_exit(void);

/* serialice LUA */
typedef struct {
    int (*io_read_pre) (uint16_t port, int size);
    void (*io_read_post) (uint32_t * data);
    int (*io_write_pre) (uint32_t * data, uint16_t port, int size);
    void (*io_write_post) (void);

    int (*load_pre) (uint32_t addr, int size);
    void (*load_post) (uint32_t * data);
    int (*store_pre) (uint32_t addr, int size, uint32_t * data);
    void (*store_post) (void);

    int (*rdmsr_pre) (uint32_t addr);
    void (*rdmsr_post) (uint32_t * hi, uint32_t * lo);
    int (*wrmsr_pre) (uint32_t addr, uint32_t * hi, uint32_t * lo);
    void (*wrmsr_post) (void);

    int (*cpuid_pre) (uint32_t eax, uint32_t ecx);
    void (*cpuid_post) (cpuid_regs_t * res);
} SerialICE_filter;

const SerialICE_filter *serialice_lua_init(const char *serialice_lua_script);
void serialice_lua_exit(void);
const char *serialice_lua_execute(const char *cmd);

#endif
