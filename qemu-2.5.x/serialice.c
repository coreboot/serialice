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

/* Indented with:
 * gnuindent -npro -kr -i4 -nut -bap -sob -l80 -ss -ncs serialice.*
 */

/* System includes */
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <unistd.h>
#include <string.h>

/* Local includes */
#include "cpu.h"
#include "hw/hw.h"
#include "hw/loader.h"
#include "hw/i386/pc.h"
#include "hw/boards.h"
#include "ui/console.h"
#include "sysemu/sysemu.h"
#include "serialice.h"

#define SERIALICE_LUA_SCRIPT "serialice.lua"

#define DEFAULT_RAM_SIZE 128
#define BIOS_FILENAME "bios.bin"

const SerialICE_target *s_target;
const SerialICE_filter *s_filter;

int serialice_active = 0;

// **************************************************************************
// high level communication with the SerialICE shell

uint64_t serialice_rdmsr(CPUX86State *env, uint32_t addr, uint32_t key)
{
    uint32_t hi = 0, lo = 0;
    uint64_t data;

    int mux = s_filter->rdmsr_pre(addr);

    if (mux & READ_FROM_SERIALICE)
        s_target->rdmsr(addr, key, &hi, &lo);

    if (mux & READ_FROM_QEMU) {
        data = cpu_rdmsr(env, addr);
        hi = (data >> 32);
        lo = (data & 0xffffffff);
    }

    s_filter->rdmsr_post(&hi, &lo);
    data = hi;
    data <<= 32;
    data |= lo;
    return data;
}

void serialice_wrmsr(CPUX86State *env, uint64_t data, uint32_t addr, uint32_t key)
{
    uint32_t hi = (data >> 32);
    uint32_t lo = (data & 0xffffffff);

    int mux = s_filter->wrmsr_pre(addr, &hi, &lo);

    if (mux & WRITE_TO_SERIALICE)
        s_target->wrmsr(addr, key, hi, lo);
    if (mux & WRITE_TO_QEMU) {
        data = lo | ((uint64_t)hi)<<32;
        cpu_wrmsr(env, addr, data);
    }
    s_filter->wrmsr_post();
}

cpuid_regs_t serialice_cpuid(CPUX86State *env, uint32_t eax, uint32_t ecx)
{
    cpuid_regs_t ret;
    ret.eax = ret.ebx = ret.ecx = ret.edx = 0;

    int mux = s_filter->cpuid_pre(eax, ecx);

    if (mux & READ_FROM_SERIALICE)
        s_target->cpuid(eax, ecx, &ret);
    if (mux & READ_FROM_QEMU)
        ret = cpu_cpuid(env, eax, ecx);

    s_filter->cpuid_post(&ret);
    return ret;
}

// **************************************************************************
// memory load handling

/**
 * This function is called by the softmmu engine to update the status
 * of a load cycle
 */
void serialice_log_load(int caught, uint32_t addr, uint32_t result,
                        unsigned int data_size)
{
    s_filter->load_post(&result);
}

/* This function can grab Qemu load ops and forward them to the SerialICE
 * target. 
 *
 * @return 0: pass on to Qemu; 1: handled locally.
 */
int serialice_handle_load(uint32_t addr, uint32_t * data, unsigned int size)
{
    int mux = s_filter->load_pre(addr, size);

    if (mux & READ_FROM_SERIALICE)
        *data = s_target->load(addr, size);

    return !(mux & READ_FROM_QEMU);
}

// **************************************************************************
// memory store handling

/* This function can grab Qemu store ops and forward them to the SerialICE
 * target
 *
 * @return 0: Qemu exclusive or shared; 1: SerialICE exclusive.
 */

int serialice_handle_store(uint32_t addr, uint32_t data, unsigned int size)
{
    int mux = s_filter->store_pre(addr, size, &data);

    if (mux & WRITE_TO_SERIALICE)
        s_target->store(addr, size, data);

    s_filter->store_post();
    return !(mux & WRITE_TO_QEMU);
}

#define mask_data(val,bytes) (val & (((uint64_t)1<<(bytes*8))-1))

uint32_t serialice_io_read(uint16_t port, unsigned int size)
{
    uint32_t data = 0;
    int mux = s_filter->io_read_pre(port, size);

    if (mux & READ_FROM_QEMU)
        data = cpu_io_read_wrapper(port, size);
    if (mux & READ_FROM_SERIALICE)
        data = s_target->io_read(port, size);

    data = mask_data(data, size);
    s_filter->io_read_post(&data);
    return data;
}

void serialice_io_write(uint16_t port, unsigned int size, uint32 data)
{
    data = mask_data(data, size);
    int mux = s_filter->io_write_pre(&data, port, size);
    data = mask_data(data, size);

    if (mux & WRITE_TO_QEMU)
        cpu_io_write_wrapper(port, size, data);
    if (mux & WRITE_TO_SERIALICE)
        s_target->io_write(port, size, data);

    s_filter->io_write_post();
}

// **************************************************************************
// initialization and exit

static void serialice_init(void)
{
    dumb_screen();

    printf("SerialICE: Open connection to target hardware...\n");
    s_target = serialice_serial_init();
    s_target->version();
    s_target->mainboard();

    printf("SerialICE: LUA init...\n");
    s_filter = serialice_lua_init(SERIALICE_LUA_SCRIPT);

    /* Let the rest of Qemu know we're alive */
    serialice_active = 1;
}

#if 0
/* no one actually uses this */
static void serialice_exit(void)
{
    serialice_lua_exit();
    serialice_serial_exit();
}
#endif

static void pc_init_serialice(MachineState *state)
{
    char *filename;
    int ret, linux_boot;
    int isa_bios_size, bios_size;
    MemoryRegion *bios, *isa_bios;
    MemoryRegion *rom_memory = get_system_memory();

    if (ram_size != (DEFAULT_RAM_SIZE * 1024 * 1024)) {
        printf
            ("Warning: Running SerialICE with non-default ram size is not supported.\n");
        exit(1);
    }

    linux_boot = (state->kernel_filename != NULL);

    pc_cpus_init((PCMachineState *)state);

    /* Must not happen before CPUs are initialized */
    serialice_init();

    /* BIOS load */
    if (bios_name == NULL)
        bios_name = BIOS_FILENAME;
    filename = qemu_find_file(QEMU_FILE_TYPE_BIOS, bios_name);
    if (filename) {
        bios_size = get_image_size(filename);
    } else {
        bios_size = -1;
    }
    if (bios_size <= 0 || (bios_size % 65536) != 0) {
        goto bios_error;
    }
    bios = g_malloc(sizeof(*bios));
    memory_region_init_ram(bios,  NULL, "serialice_bios", bios_size,
		    &error_fatal);
    vmstate_register_ram_global(bios);

    ret = rom_add_file_fixed(bios_name, (uint32_t)(-bios_size), -1);
    if (ret != 0) {
    bios_error:
        fprintf(stderr, "qemu: could not load PC BIOS '%s'\n", bios_name);
        exit(1);
    }
    g_free(filename);

    /* map the last 128KB of the BIOS in ISA space */
    isa_bios_size = bios_size;
    if (isa_bios_size > (128 * 1024))
        isa_bios_size = 128 * 1024;

    isa_bios = g_malloc(sizeof(*isa_bios));
    memory_region_init_alias(isa_bios, NULL, "isa-bios", bios,
                             bios_size - isa_bios_size, isa_bios_size);
    memory_region_add_subregion_overlap(rom_memory,
                                        0x100000 - isa_bios_size,
                                        isa_bios,
                                        1);

    /* map all the bios at the top of memory */
    memory_region_add_subregion(rom_memory,
                                (uint32_t)(-bios_size),
                                bios);
    if (linux_boot) {
        fprintf(stderr, "Booting Linux in SerialICE mode is currently not supported.\n");
        exit(1);
    }

}

static void serialice_machine_init(MachineClass *mc)
{
    mc->alias = "serialice";
    mc->desc = "SerialICE Debugger";
    mc->init = pc_init_serialice;
    mc->max_cpus = 255;
}

DEFINE_MACHINE("serialice-x86", serialice_machine_init)
