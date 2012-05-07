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
#include "hw/hw.h"
#include "hw/loader.h"
#include "hw/pc.h"
#include "hw/boards.h"
#include "console.h"
#include "serialice.h"
#include "sysemu.h"

#define SERIALICE_LUA_SCRIPT "serialice.lua"

#define DEFAULT_RAM_SIZE 128
#define BIOS_FILENAME "bios.bin"

const SerialICE_target *target;
const SerialICE_filter *filter;

int serialice_active = 0;

// **************************************************************************
// high level communication with the SerialICE shell

uint64_t serialice_rdmsr(uint32_t addr, uint32_t key)
{
    uint32_t hi = 0, lo = 0;
    uint64_t data;

    int mux = filter->rdmsr_pre(addr);

    if (mux & READ_FROM_SERIALICE)
        target->rdmsr(addr, key, &hi, &lo);

    if (mux & READ_FROM_QEMU) {
        data = cpu_rdmsr(addr);
        hi = (data >> 32);
        lo = (data & 0xffffffff);
    }

    filter->rdmsr_post(&hi, &lo);
    data = hi;
    data <<= 32;
    data |= lo;
    return data;
}

void serialice_wrmsr(uint64_t data, uint32_t addr, uint32_t key)
{
    uint32_t hi = (data >> 32);
    uint32_t lo = (data & 0xffffffff);

    int mux = filter->wrmsr_pre(addr, &hi, &lo);

    if (mux & WRITE_TO_SERIALICE)
        target->wrmsr(addr, key, hi, lo);
    if (mux & WRITE_TO_QEMU) {
        data = lo | ((uint64_t)hi)<<32;
        cpu_wrmsr(addr, data);
    }
    filter->wrmsr_post();
}

cpuid_regs_t serialice_cpuid(uint32_t eax, uint32_t ecx)
{
    cpuid_regs_t ret;
    ret.eax = ret.ebx = ret.ecx = ret.edx = 0;

    int mux = filter->cpuid_pre(eax, ecx);

    if (mux & READ_FROM_SERIALICE)
        target->cpuid(eax, ecx, &ret);
    if (mux & READ_FROM_QEMU)
        ret = cpu_cpuid(eax, ecx);

    filter->cpuid_post(&ret);
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
    filter->load_post(&result);
}

/* This function can grab Qemu load ops and forward them to the SerialICE
 * target. 
 *
 * @return 0: pass on to Qemu; 1: handled locally.
 */
int serialice_handle_load(uint32_t addr, uint32_t * data, unsigned int size)
{
    int mux = filter->load_pre(addr, size);

    if (mux & READ_FROM_SERIALICE)
        *data = target->load(addr, size);

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
    int mux = filter->store_pre(addr, size, &data);

    if (mux & WRITE_TO_SERIALICE)
        target->store(addr, size, data);

    filter->store_post();
    return !(mux & WRITE_TO_QEMU);
}

#define mask_data(val,bytes) (val & (((uint64_t)1<<(bytes*8))-1))

uint32_t serialice_io_read(uint16_t port, unsigned int size)
{
    uint32_t data = 0;
    int mux = filter->io_read_pre(port, size);

    if (mux & READ_FROM_QEMU)
        data = cpu_io_read_wrapper(port, size);
    if (mux & READ_FROM_SERIALICE)
        data = target->io_read(port, size);

    data = mask_data(data, size);
    filter->io_read_post(&data);
    return data;
}

void serialice_io_write(uint16_t port, unsigned int size, uint32 data)
{
    data = mask_data(data, size);
    int mux = filter->io_write_pre(&data, port, size);
    data = mask_data(data, size);

    if (mux & WRITE_TO_QEMU)
        cpu_io_write_wrapper(port, size, data);
    if (mux & WRITE_TO_SERIALICE)
        target->io_write(port, size, data);

    filter->io_write_post();
}

// **************************************************************************
// initialization and exit

static void serialice_init(void)
{
    dumb_screen();

    printf("SerialICE: Open connection to target hardware...\n");
    target = serialice_serial_init();
    target->version();
    target->mainboard();

    printf("SerialICE: LUA init...\n");
    filter = serialice_lua_init(SERIALICE_LUA_SCRIPT);

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

static void pc_init_serialice(ram_addr_t ram_size,
                              const char *boot_device,
                              const char *kernel_filename,
                              const char *kernel_cmdline,
                              const char *initrd_filename,
                              const char *cpu_model)
{
    char *filename;
    int ret, i, linux_boot;
    int isa_bios_size, bios_size;
    ram_addr_t bios_offset;
    CPUState *env;

    if (ram_size != (DEFAULT_RAM_SIZE * 1024 * 1024)) {
        printf
            ("Warning: Running SerialICE with non-default ram size is not supported.\n");
        exit(1);
    }

    linux_boot = (kernel_filename != NULL);

    /* init CPUs */
    if (cpu_model == NULL) {
        //printf("Warning: Running SerialICE with generic CPU type might fail.\n");
#ifdef TARGET_X86_64
        cpu_model = "qemu64";
#else
        cpu_model = "qemu32";
#endif
    }

    for (i = 0; i < smp_cpus; i++) {
        env = cpu_init(cpu_model);
        qemu_register_reset((QEMUResetHandler *) cpu_reset, env);
    }

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
    bios_offset = qemu_ram_alloc(NULL, "serialice_bios", bios_size);
    ret = load_image(filename, qemu_get_ram_ptr(bios_offset));
    if (ret != bios_size) {
      bios_error:
        fprintf(stderr, "qemu: could not load PC BIOS '%s'\n", bios_name);
        exit(1);
    }
    if (filename) {
        qemu_free(filename);
    }
    /* map the last 128KB of the BIOS in ISA space */
    isa_bios_size = bios_size;
    if (isa_bios_size > (128 * 1024))
        isa_bios_size = 128 * 1024;

    cpu_register_physical_memory(0x100000 - isa_bios_size,
                                 isa_bios_size,
                                 (bios_offset + bios_size - isa_bios_size));

    /* map all the bios at the top of memory */
    cpu_register_physical_memory((uint32_t) (-bios_size), bios_size,
                                 bios_offset | IO_MEM_ROM);
    if (linux_boot) {
        printf("Booting Linux in SerialICE mode is currently not supported.\n");
        exit(1);
    }

}

static QEMUMachine serialice_machine = {
    .name = "serialice-x86",
    .alias = "serialice",
    .desc = "SerialICE Debugger",
    .init = pc_init_serialice,
    .max_cpus = 255,
    //.is_default = 1,
};

static void serialice_machine_init(void)
{
    qemu_register_machine(&serialice_machine);
}

machine_init(serialice_machine_init);
