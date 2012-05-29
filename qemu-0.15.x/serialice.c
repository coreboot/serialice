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
#ifdef WIN32
#include <windows.h>
#include <conio.h>
#else
#include <fcntl.h>
#include <termios.h>
#include <sys/ioctl.h>
#endif

/* LUA includes */
#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>

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
const char *serialice_lua_script = "serialice.lua";

extern const char *serialice_mainboard;

static lua_State *L;

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

// **************************************************************************
// LUA scripting interface and callbacks

static int serialice_register_physical(lua_State * luastate)
{
    int n = lua_gettop(luastate);
    static uint8_t num = 1;
    uint32_t addr, size;
    ram_addr_t phys;
    char ram_name[16];

    if (n != 2) {
        fprintf(stderr,
                "ERROR: Not called as SerialICE_register_physical(<addr> <size>)\n");
        return 0;
    }

    addr = lua_tointeger(luastate, 1);
    size = lua_tointeger(luastate, 2);

    if (num > 99) {
        fprintf(stderr,"To much memory ranges registered\n");
        exit(1);
    }
    printf("Registering physical memory at 0x%08x (0x%08x bytes)\n", addr, size);
    sprintf(ram_name, "serialice_ram%u", num);
    phys = qemu_ram_alloc(NULL, ram_name, size);
    cpu_register_physical_memory(addr, size, phys);
    num++;
    return 0;
}

static int serialice_system_reset(lua_State * luastate)
{
    printf("Rebooting the emulated host CPU\n");
    qemu_system_reset_request();
    return 0;
}

// **************************************************************************
// LUA register access

// some macros from target-i386/exec.h, which we can't include directly
#define env first_cpu
#define EAX (env->regs[R_EAX])
#define ECX (env->regs[R_ECX])
#define EDX (env->regs[R_EDX])
#define EBX (env->regs[R_EBX])
#define ESP (env->regs[R_ESP])
#define EBP (env->regs[R_EBP])
#define ESI (env->regs[R_ESI])
#define EDI (env->regs[R_EDI])
#define EIP (env->eip)
#define CS  (env->segs[R_CS].base)
static int register_set(lua_State * L)
{
    const char *key = luaL_checkstring(L, 2);
    int val = luaL_checkint(L, 3);
    int ret = 1;

    if (strcmp(key, "eax") == 0) {
        EAX = val;
    } else if (strcmp(key, "ecx") == 0) {
        ECX = val;
    } else if (strcmp(key, "edx") == 0) {
        EDX = val;
    } else if (strcmp(key, "ebx") == 0) {
        EBX = val;
    } else if (strcmp(key, "esp") == 0) {
        ESP = val;
    } else if (strcmp(key, "ebp") == 0) {
        EBP = val;
    } else if (strcmp(key, "esi") == 0) {
        ESI = val;
    } else if (strcmp(key, "edi") == 0) {
        EDI = val;
    } else if (strcmp(key, "eip") == 0) {
        EIP = val;
    } else if (strcmp(key, "cs") == 0) {
        CS = (val << 4);
    } else {
        lua_pushstring(L, "No such register.");
        lua_error(L);
        ret = 0;
    }
    return ret;
}

static int register_get(lua_State * L)
{
    const char *key = luaL_checkstring(L, 2);
    int ret = 1;
    if (strcmp(key, "eax") == 0) {
        lua_pushinteger(L, EAX);
    } else if (strcmp(key, "ecx") == 0) {
        lua_pushinteger(L, ECX);
    } else if (strcmp(key, "edx") == 0) {
        lua_pushinteger(L, EDX);
    } else if (strcmp(key, "ebx") == 0) {
        lua_pushinteger(L, EBX);
    } else if (strcmp(key, "esp") == 0) {
        lua_pushinteger(L, ESP);
    } else if (strcmp(key, "ebp") == 0) {
        lua_pushinteger(L, EBP);
    } else if (strcmp(key, "esi") == 0) {
        lua_pushinteger(L, ESI);
    } else if (strcmp(key, "edi") == 0) {
        lua_pushinteger(L, EDI);
    } else if (strcmp(key, "eip") == 0) {
        lua_pushinteger(L, EIP);
    } else if (strcmp(key, "cs") == 0) {
        lua_pushinteger(L, (CS >> 4));
    } else {
        lua_pushstring(L, "No such register.");
        lua_error(L);
        ret = 0;
    }
    return ret;
}

#undef env

static int serialice_lua_registers(void)
{
    const struct luaL_Reg registermt[] = {
        {"__index", register_get},
        {"__newindex", register_set},
        {NULL, NULL}
    };

    lua_newuserdata(L, sizeof(void *));
    luaL_newmetatable(L, "registermt");
#if LUA_VERSION_NUM <= 501
    luaL_register(L, NULL, registermt);
#elif LUA_VERSION_NUM >= 502
    luaL_setfuncs(L, registermt, 0);
#endif
    lua_setmetatable(L, -2);
    lua_setglobal(L, "regs");

    return 0;
}

static int serialice_lua_init(void)
{
    int status;

    /* Create a LUA context and load LUA libraries */
    L = luaL_newstate();
    luaL_openlibs(L);

    /* Register C function callbacks */
    lua_register(L, "SerialICE_register_physical", serialice_register_physical);
    lua_register(L, "SerialICE_system_reset", serialice_system_reset);

    /* Set global variable SerialICE_mainboard */
    lua_pushstring(L, serialice_mainboard);
    lua_setglobal(L, "SerialICE_mainboard");

    /* Enable Register Access */
    serialice_lua_registers();

    /* Load the script file */
    status = luaL_loadfile(L, serialice_lua_script);
    if (status) {
        fprintf(stderr, "Couldn't load SerialICE script: %s\n",
                lua_tostring(L, -1));
        exit(1);
    }

    /* Ask Lua to run our little script */
    status = lua_pcall(L, 0, 1, 0);
    if (status) {
        fprintf(stderr, "Failed to run script: %s\n", lua_tostring(L, -1));
        exit(1);
    }
    lua_pop(L, 1);

    return 0;
}

#if 0
/* not used yet */
static int serialice_lua_exit(void)
{
    lua_close(L);
    return 0;
}
#endif

const char *serialice_lua_execute(const char *cmd)
{
    int error;
    char *errstring = NULL;
    error = luaL_loadbuffer(L, cmd, strlen(cmd), "line")
        || lua_pcall(L, 0, 0, 0);
    if (error) {
        errstring = strdup(lua_tostring(L, -1));
        lua_pop(L, 1);
    }

    return errstring;
}

static int serialice_io_read_filter(uint32_t * data, uint16_t port, int size)
{
    int ret, result;

    lua_getglobal(L, "SerialICE_io_read_filter");
    lua_pushinteger(L, port);   // port
    lua_pushinteger(L, size);   // datasize
    result = lua_pcall(L, 2, 2, 0);
    if (result) {
        fprintf(stderr, "Failed to run function SerialICE_io_read_filter: %s\n",
                lua_tostring(L, -1));
        exit(1);
    }
    *data = lua_tointeger(L, -1);
    ret = lua_toboolean(L, -2);
    lua_pop(L, 2);

    return ret;
}

static int serialice_io_write_filter(uint32_t * data, uint16_t port, int size)
{
    int ret, result;

    lua_getglobal(L, "SerialICE_io_write_filter");
    lua_pushinteger(L, port);   // port
    lua_pushinteger(L, size);   // datasize
    lua_pushinteger(L, *data);  // data

    result = lua_pcall(L, 3, 2, 0);
    if (result) {
        fprintf(stderr,
                "Failed to run function SerialICE_io_write_filter: %s\n",
                lua_tostring(L, -1));
        exit(1);
    }
    *data = lua_tointeger(L, -1);
    ret = lua_toboolean(L, -2);
    lua_pop(L, 2);

    return ret;
}

#define READ_FROM_QEMU		(1 << 0)
#define READ_FROM_SERIALICE	(1 << 1)
static int serialice_memory_read_filter(uint32_t addr, uint32_t * data,
                                        int size)
{
    int ret = 0, result;

    lua_getglobal(L, "SerialICE_memory_read_filter");
    lua_pushinteger(L, addr);   // addr
    lua_pushinteger(L, size);   // datasize
    result = lua_pcall(L, 2, 3, 0);
    if (result) {
        fprintf(stderr,
                "Failed to run function SerialICE_memory_read_filter: %s\n",
                lua_tostring(L, -1));
        exit(1);
    }

    *data = lua_tointeger(L, -1);       // result

    ret |= lua_toboolean(L, -2) ? READ_FROM_QEMU : 0;   // to_qemu
    ret |= lua_toboolean(L, -3) ? READ_FROM_SERIALICE : 0;      // to_hw

    lua_pop(L, 3);

    return ret;
}

#define WRITE_TO_QEMU		(1 << 0)
#define WRITE_TO_SERIALICE	(1 << 1)

static int serialice_memory_write_filter(uint32_t addr, int size,
                                         uint32_t * data)
{
    int ret = 0, result;
    int write_to_qemu, write_to_serialice;

    lua_getglobal(L, "SerialICE_memory_write_filter");
    lua_pushinteger(L, addr);   // address
    lua_pushinteger(L, size);   // datasize
    lua_pushinteger(L, *data);  // data
    result = lua_pcall(L, 3, 3, 0);
    if (result) {
        fprintf(stderr,
                "Failed to run function SerialICE_memory_write_filter: %s\n",
                lua_tostring(L, -1));
        exit(1);
    }
    *data = lua_tointeger(L, -1);
    write_to_qemu = lua_toboolean(L, -2);
    write_to_serialice = lua_toboolean(L, -3);
    lua_pop(L, 3);

    ret |= write_to_qemu ? WRITE_TO_QEMU : 0;
    ret |= write_to_serialice ? WRITE_TO_SERIALICE : 0;

    return ret;
}

#define FILTER_READ	0
#define FILTER_WRITE	1

static int serialice_msr_filter(int flags, uint32_t addr, uint32_t * hi,
                                uint32_t * lo)
{
    int ret, result;

    if (flags & FILTER_WRITE) {
        lua_getglobal(L, "SerialICE_msr_write_filter");
    } else {
        lua_getglobal(L, "SerialICE_msr_read_filter");
    }

    lua_pushinteger(L, addr);   // port
    lua_pushinteger(L, *hi);    // high
    lua_pushinteger(L, *lo);    // low
    result = lua_pcall(L, 3, 3, 0);
    if (result) {
        fprintf(stderr,
                "Failed to run function SerialICE_msr_%s_filter: %s\n",
                (flags & FILTER_WRITE) ? "write" : "read", lua_tostring(L, -1));
        exit(1);
    }
    ret = lua_toboolean(L, -3);
    if (ret) {
        *hi = lua_tointeger(L, -1);
        *lo = lua_tointeger(L, -2);
    }
    lua_pop(L, 3);

    return ret;
}

static int serialice_cpuid_filter(uint32_t eax, uint32_t ecx,
                                  cpuid_regs_t * regs)
{
    int ret, result;

    lua_getglobal(L, "SerialICE_cpuid_filter");

    lua_pushinteger(L, eax);    // eax before calling
    lua_pushinteger(L, ecx);    // ecx before calling
    // and the registers after calling cpuid
    lua_pushinteger(L, regs->eax);      // eax
    lua_pushinteger(L, regs->ebx);      // ebx
    lua_pushinteger(L, regs->ecx);      // ecx
    lua_pushinteger(L, regs->edx);      // edx
    result = lua_pcall(L, 6, 5, 0);
    if (result) {
        fprintf(stderr,
                "Failed to run function SerialICE_cpuid_filter: %s\n",
                lua_tostring(L, -1));
        exit(1);
    }

    ret = lua_toboolean(L, -5);
    if (ret) {
        regs->eax = lua_tointeger(L, -4);
        regs->ebx = lua_tointeger(L, -3);
        regs->ecx = lua_tointeger(L, -2);
        regs->edx = lua_tointeger(L, -1);
    }
    lua_pop(L, 5);

    return ret;
}

/* SerialICE output loggers */

#define LOG_IO		0
#define LOG_MEMORY	1
#define LOG_READ	0
#define LOG_WRITE	2
// these two are separate
#define LOG_QEMU	4
#define LOG_TARGET	8

static void serialice_log(int flags, uint32_t data, uint32_t addr, int size)
{
    int result;

    if ((flags & LOG_WRITE) && (flags & LOG_MEMORY)) {
        lua_getglobal(L, "SerialICE_memory_write_log");
    } else if (!(flags & LOG_WRITE) && (flags & LOG_MEMORY)) {
        lua_getglobal(L, "SerialICE_memory_read_log");
    } else if ((flags & LOG_WRITE) && !(flags & LOG_MEMORY)) {
        lua_getglobal(L, "SerialICE_io_write_log");
    } else {                    // if (!(flags & LOG_WRITE) && !(flags & LOG_MEMORY))
        lua_getglobal(L, "SerialICE_io_read_log");
    }

    lua_pushinteger(L, addr);   // addr/port
    lua_pushinteger(L, size);   // datasize
    lua_pushinteger(L, data);   // data
    lua_pushboolean(L, ((flags & LOG_TARGET) != 0));

    result = lua_pcall(L, 4, 0, 0);
    if (result) {
        fprintf(stderr, "Failed to run function SerialICE_%s_%s_log: %s\n",
                (flags & LOG_MEMORY) ? "memory" : "io",
                (flags & LOG_WRITE) ? "write" : "read", lua_tostring(L, -1));
        exit(1);
    }
}

static void serialice_msr_log(int flags, uint32_t addr, uint32_t hi,
                              uint32_t lo, int filtered)
{
    int result;

    if (flags & LOG_WRITE) {
        lua_getglobal(L, "SerialICE_msr_write_log");
    } else {                    // if (!(flags & LOG_WRITE))
        lua_getglobal(L, "SerialICE_msr_read_log");
    }

    lua_pushinteger(L, addr);   // addr/port
    lua_pushinteger(L, hi);     // datasize
    lua_pushinteger(L, lo);     // data
    lua_pushboolean(L, filtered);       // data
    result = lua_pcall(L, 4, 0, 0);
    if (result) {
        fprintf(stderr, "Failed to run function SerialICE_msr_%s_log: %s\n",
                (flags & LOG_WRITE) ? "write" : "read", lua_tostring(L, -1));
        exit(1);
    }
}

static void serialice_cpuid_log(uint32_t eax, uint32_t ecx, cpuid_regs_t res,
                                int filtered)
{
    int result;

    lua_getglobal(L, "SerialICE_cpuid_log");

    lua_pushinteger(L, eax);    // input: eax
    lua_pushinteger(L, ecx);    // input: ecx
    lua_pushinteger(L, res.eax);        // output: eax
    lua_pushinteger(L, res.ebx);        // output: ebx
    lua_pushinteger(L, res.ecx);        // output: ecx
    lua_pushinteger(L, res.edx);        // output: edx
    lua_pushboolean(L, filtered);       // data
    result = lua_pcall(L, 7, 0, 0);
    if (result) {
        fprintf(stderr, "Failed to run function SerialICE_cpuid_log: %s\n",
                lua_tostring(L, -1));
        exit(1);
    }
}

// **************************************************************************
// high level communication with the SerialICE shell

uint8_t serialice_inb(uint16_t port)
{
    uint8_t ret;
    uint32_t data;
    int filtered;

    filtered = serialice_io_read_filter(&data, port, 1);

    if (filtered) {
        ret = data & 0xff;
    } else {
	return serialice_io_read_wrapper(port, 1);
    }

    serialice_log(LOG_READ | LOG_IO, ret, port, 1);

    return ret;
}

uint16_t serialice_inw(uint16_t port)
{
    uint16_t ret;
    uint32_t data;
    int filtered;

    filtered = serialice_io_read_filter(&data, port, 2);

    if (filtered) {
        ret = data & 0xffff;
    } else {
	return serialice_io_read_wrapper(port, 2);
    }

    serialice_log(LOG_READ | LOG_IO, ret, port, 2);

    return ret;
}

uint32_t serialice_inl(uint16_t port)
{
    uint32_t ret;
    uint32_t data;
    int filtered;

    filtered = serialice_io_read_filter(&data, port, 4);

    if (filtered) {
        ret = data;
    } else {
	return serialice_io_read_wrapper(port, 4);
    }

    serialice_log(LOG_READ | LOG_IO, ret, port, 4);

    return ret;
}

void serialice_outb(uint8_t data, uint16_t port)
{
    uint32_t filtered_data = (uint32_t) data;
    int filtered;

    filtered = serialice_io_write_filter(&filtered_data, port, 1);

    if (filtered) {
        data = (uint8_t) filtered_data;
    } else {
        data = (uint8_t) filtered_data;
	serialice_io_write_wrapper(port, 1, data);
    }

    serialice_log(LOG_WRITE | LOG_IO, data, port, 1);
}

void serialice_outw(uint16_t data, uint16_t port)
{
    uint32_t filtered_data = (uint32_t) data;
    int filtered;

    filtered = serialice_io_write_filter(&filtered_data, port, 2);

    if (filtered) {
        data = (uint16_t) filtered_data;
    } else {
        data = (uint16_t) filtered_data;
	serialice_io_write_wrapper(port, 2, data);
    }

    serialice_log(LOG_WRITE | LOG_IO, data, port, 2);
}

void serialice_outl(uint32_t data, uint16_t port)
{
    uint32_t filtered_data = data;
    int filtered;

    filtered = serialice_io_write_filter(&filtered_data, port, 4);

    if (filtered) {
        data = filtered_data;
    } else {
        data = filtered_data;
	serialice_io_write_wrapper(port, 4, data);
    }

    serialice_log(LOG_WRITE | LOG_IO, data, port, 4);
}

uint64_t serialice_rdmsr(uint32_t addr, uint32_t key)
{
    uint32_t hi, lo;
    uint64_t ret;
    int filtered;

    filtered = serialice_msr_filter(FILTER_READ, addr, &hi, &lo);
    if (!filtered) {
        serialice_rdmsr_wrapper(addr, key, &hi, &lo);
    }

    ret = hi;
    ret <<= 32;
    ret |= lo;

    serialice_msr_log(LOG_READ, addr, hi, lo, filtered);

    return ret;
}

void serialice_wrmsr(uint64_t data, uint32_t addr, uint32_t key)
{
    uint32_t hi, lo;
    int filtered;

    hi = (data >> 32);
    lo = (data & 0xffffffff);

    filtered = serialice_msr_filter(FILTER_WRITE, addr, &hi, &lo);

    if (!filtered) {
        serialice_wrmsr_wrapper(addr, key, hi, lo);
    }

    serialice_msr_log(LOG_WRITE, addr, hi, lo, filtered);
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

// **************************************************************************
// initialization and exit

static void serialice_init(void)
{
    ds = graphic_console_init(serialice_refresh, serialice_invalidate,
                              NULL, NULL, ds);
    qemu_console_resize(ds, 320, 240);

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
    qemu_free(s->command);
    qemu_free(s->buffer);
    qemu_free(s);
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
