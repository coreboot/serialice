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

#define SERIALICE_BANNER 1
#if SERIALICE_BANNER
#include "serialice_banner.h"
#endif

#define DEFAULT_RAM_SIZE 128
#define BIOS_FILENAME "bios.bin"

int serialice_active = 0;

static DisplayState *ds;
static int screen_invalid = 1;

static void serialice_refresh(void *opaque)
{
    uint8_t *dest;
    int bpp, linesize;

    if (!screen_invalid) {
        return;
    }

    dest = ds_get_data(ds);
    bpp = (ds_get_bits_per_pixel(ds) + 7) >> 3;
    linesize = ds_get_linesize(ds);

    memset(dest, 0x00, linesize * ds_get_height(ds));
#if SERIALICE_BANNER
    int x, y;
    if (bpp == 4) {
        for (y = 0; y < 240; y++) {
            for (x = 0; x < 320; x++) {
                int doff = (y * linesize) + (x * bpp);
                int soff = (y * (320 * 3)) + (x * 3);
                dest[doff + 0] = serialice_banner[soff + 2];    // blue
                dest[doff + 1] = serialice_banner[soff + 1];    // green
                dest[doff + 2] = serialice_banner[soff + 0];    // red
            }
        }
    } else {
        printf("Banner enabled and BPP = %d (line size = %d)\n", bpp, linesize);
    }
#endif

    dpy_update(ds, 0, 0, ds_get_width(ds), ds_get_height(ds));
    screen_invalid = 0;
}

static void serialice_invalidate(void *opaque)
{
    screen_invalid = 1;
}

static void serialice_screen(void)
{
    ds = graphic_console_init(serialice_refresh, serialice_invalidate,
                              NULL, NULL, ds);
    qemu_console_resize(ds, 320, 240);
}

// **************************************************************************
// high level communication with the SerialICE shell

uint64_t serialice_rdmsr(uint32_t addr, uint32_t key)
{
    uint32_t hi, lo;
    uint64_t ret;
    int filtered;

    filtered = serialice_rdmsr_filter(addr, &hi, &lo);
    if (!filtered) {
        serialice_rdmsr_wrapper(addr, key, &hi, &lo);
    }

    ret = hi;
    ret <<= 32;
    ret |= lo;

    serialice_rdmsr_log(addr, hi, lo, filtered);

    return ret;
}

void serialice_wrmsr(uint64_t data, uint32_t addr, uint32_t key)
{
    uint32_t hi, lo;
    int filtered;

    hi = (data >> 32);
    lo = (data & 0xffffffff);

    filtered = serialice_wrmsr_filter(addr, &hi, &lo);

    if (!filtered) {
        serialice_wrmsr_wrapper(addr, key, hi, lo);
    }

    serialice_wrmsr_log(addr, hi, lo, filtered);
}

cpuid_regs_t serialice_cpuid(uint32_t eax, uint32_t ecx)
{
    cpuid_regs_t ret;
    int filtered;

    serialice_cpuid_wrapper(eax, ecx, &ret);

    filtered = serialice_cpuid_filter(eax, ecx, &ret);

    serialice_cpuid_log(eax, ecx, ret, filtered);

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
    if (caught) {
        serialice_log(LOG_READ | LOG_MEMORY | LOG_TARGET, result, addr,
                      data_size);
    } else {
        serialice_log(LOG_READ | LOG_MEMORY, result, addr, data_size);
    }
}

/* This function can grab Qemu load ops and forward them to the SerialICE
 * target. 
 *
 * @return 0: pass on to Qemu; 1: handled locally.
 */
int serialice_handle_load(uint32_t addr, uint32_t * result,
                          unsigned int data_size)
{
    int source;

    source = serialice_memory_read_filter(addr, result, data_size);

    if (source & READ_FROM_SERIALICE) {
        *result = serialice_load_wrapper(addr, data_size);
        return 1;
    }

    if (source & READ_FROM_QEMU) {
        return 0;
    }

    /* No source for load, so the source is the script */
    return 1;
}

// **************************************************************************
// memory store handling

static void serialice_log_store(int caught, uint32_t addr, uint32_t val,
                                unsigned int data_size)
{
    if (caught) {
        serialice_log(LOG_WRITE | LOG_MEMORY | LOG_TARGET, val, addr,
                      data_size);
    } else {
        serialice_log(LOG_WRITE | LOG_MEMORY, val, addr, data_size);
    }
}

/* This function can grab Qemu store ops and forward them to the SerialICE
 * target
 *
 * @return 0: Qemu exclusive or shared; 1: SerialICE exclusive.
 */

int serialice_handle_store(uint32_t addr, uint32_t val, unsigned int data_size)
{
    int write_to_target, write_to_qemu, ret;
    uint32_t filtered_data = val;

    ret = serialice_memory_write_filter(addr, data_size, &filtered_data);

    write_to_target = ((ret & WRITE_TO_SERIALICE) != 0);
    write_to_qemu = ((ret & WRITE_TO_QEMU) != 0);

    serialice_log_store(write_to_target, addr, filtered_data, data_size);

    if (write_to_target) {
        serialice_store_wrapper(addr, data_size, filtered_data);
    }

    return (write_to_qemu == 0);
}

#define mask_data(val,bytes) (val & (((uint64_t)1<<(bytes*8))-1))

uint32_t serialice_io_read(uint16_t port, unsigned int size)
{
    uint32_t data = 0;
    int filtered;

    filtered = serialice_io_read_filter(&data, port, size);
    if (!filtered) {
	return serialice_io_read_wrapper(port, size);
    }

    data = mask_data(data, size);
    serialice_log(LOG_READ | LOG_IO, data, port, size);
    return data;
}

void serialice_io_write(uint16_t port, unsigned int size, uint32 data)
{
    uint32_t filtered_data = mask_data(data, size);
    int filtered;

    filtered = serialice_io_write_filter(&filtered_data, port, size);

    if (filtered) {
        data = mask_data(filtered_data, size);
    } else {
        data = mask_data(filtered_data, size);
	serialice_io_write_wrapper(port, size, data);
    }

    serialice_log(LOG_WRITE | LOG_IO, data, port, size);
}

// **************************************************************************
// initialization and exit

static void serialice_init(void)
{
    serialice_screen();

    printf("SerialICE: Open connection to target hardware...\n");
    serialice_serial_init();

    printf("SerialICE: LUA init...\n");
    serialice_lua_init();

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
