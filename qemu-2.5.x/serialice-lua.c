/*
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

/* LUA includes */
#define LUA_COMPAT_5_2
#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>

/* Local includes */
#include "exec/address-spaces.h"
#include "hw/hw.h"
#include "sysemu/sysemu.h"
#include "serialice.h"

#define LOG_IO		1
#define LOG_MEMORY	2
#define LOG_MSR		4

static lua_State *L;
extern char *serialice_mainboard;
static const SerialICE_filter lua_ops;
static CPUX86State *env;

// **************************************************************************
// LUA scripting interface and callbacks

static int serialice_register_physical(lua_State * luastate)
{
    int n = lua_gettop(luastate);
    static uint8_t num = 1;
    uint32_t addr, size;
    MemoryRegion *phys;
    MemoryRegion *rom_memory = get_system_memory();
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
    phys = g_malloc(sizeof(*phys));
    memory_region_init_ram(phys,  NULL, ram_name, size, &error_fatal);
    memory_region_add_subregion(rom_memory, addr, phys);
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
    int val = luaL_checkinteger(L, 3);
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

const SerialICE_filter * serialice_lua_init(const char *serialice_lua_script)
{
    int status;

    X86CPU *first_x86_cpu = X86_CPU(first_cpu);
    env = &first_x86_cpu->env;

    printf("SerialICE: LUA init...\n");

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

    return &lua_ops;
}

void serialice_lua_exit(void)
{
    lua_close(L);
}

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

static int io_read_pre(uint16_t port, int size)
{
    int ret = 0, result;

    lua_getglobal(L, "SerialICE_io_read_filter");
    lua_pushinteger(L, port);   // port
    lua_pushinteger(L, size);   // datasize

    result = lua_pcall(L, 2, 2, 0);
    if (result) {
        fprintf(stderr, "Failed to run function SerialICE_io_read_filter: %s\n",
                lua_tostring(L, -1));
        exit(1);
    }

    ret |= lua_toboolean(L, -1) ? READ_FROM_QEMU : 0;
    ret |= lua_toboolean(L, -2) ? READ_FROM_SERIALICE : 0;
    lua_pop(L, 2);
    return ret;
}

static int io_write_pre(uint32_t * data, uint16_t port, int size)
{
    int ret = 0, result;

    lua_getglobal(L, "SerialICE_io_write_filter");
    lua_pushinteger(L, port);   // port
    lua_pushinteger(L, size);   // datasize
    lua_pushinteger(L, *data);  // data

    result = lua_pcall(L, 3, 3, 0);
    if (result) {
        fprintf(stderr,
                "Failed to run function SerialICE_io_write_filter: %s\n",
                lua_tostring(L, -1));
        exit(1);
    }

    *data = lua_tointeger(L, -1);
    ret |= lua_toboolean(L, -2) ? WRITE_TO_QEMU : 0;
    ret |= lua_toboolean(L, -3) ? WRITE_TO_SERIALICE : 0;
    lua_pop(L, 3);
    return ret;
}

static int memory_read_pre(uint32_t addr, int size)
{
    int ret = 0, result;

    lua_getglobal(L, "SerialICE_memory_read_filter");
    lua_pushinteger(L, addr);
    lua_pushinteger(L, size);

    result = lua_pcall(L, 2, 2, 0);
    if (result) {
        fprintf(stderr,
                "Failed to run function SerialICE_memory_read_filter: %s\n",
                lua_tostring(L, -1));
        exit(1);
    }

    ret |= lua_toboolean(L, -1) ? READ_FROM_QEMU : 0;
    ret |= lua_toboolean(L, -2) ? READ_FROM_SERIALICE : 0;
    lua_pop(L, 2);
    return ret;
}

static int memory_write_pre(uint32_t addr, int size,
                                         uint32_t * data)
{
    int ret = 0, result;

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
    ret |= lua_toboolean(L, -2) ? WRITE_TO_QEMU : 0;
    ret |= lua_toboolean(L, -3) ? WRITE_TO_SERIALICE : 0;
    lua_pop(L, 3);
    return ret;
}

static int wrmsr_pre(uint32_t addr, uint32_t * hi, uint32_t * lo)
{
    int ret = 0, result;

    lua_getglobal(L, "SerialICE_msr_write_filter");
    lua_pushinteger(L, addr);   // port
    lua_pushinteger(L, *hi);    // high
    lua_pushinteger(L, *lo);    // low

    result = lua_pcall(L, 3, 4, 0);
    if (result) {
        fprintf(stderr,
                "Failed to run function SerialICE_msr_write_filter: %s\n", lua_tostring(L, -1));
        exit(1);
    }

    *lo = lua_tointeger(L, -1);
    *hi = lua_tointeger(L, -2);
    ret |= lua_toboolean(L, -3) ? WRITE_TO_QEMU : 0;
    ret |= lua_toboolean(L, -4) ? WRITE_TO_SERIALICE : 0;
    lua_pop(L, 4);
    return ret;
}

static int rdmsr_pre(uint32_t addr)
{
    int ret = 0, result;

    lua_getglobal(L, "SerialICE_msr_read_filter");
    lua_pushinteger(L, addr);

    result = lua_pcall(L, 1, 2, 0);
    if (result) {
        fprintf(stderr,
                "Failed to run function SerialICE_msr_read_filter: %s\n", lua_tostring(L, -1));
        exit(1);
    }

    ret |= lua_toboolean(L, -1) ? WRITE_TO_QEMU : 0;
    ret |= lua_toboolean(L, -2) ? WRITE_TO_SERIALICE : 0;
    lua_pop(L, 2);
    return ret;
}

static int cpuid_pre(uint32_t eax, uint32_t ecx)
{
    int ret = 0, result;

    lua_getglobal(L, "SerialICE_cpuid_filter");
    lua_pushinteger(L, eax);    // eax before calling
    lua_pushinteger(L, ecx);    // ecx before calling

    result = lua_pcall(L, 2, 2, 0);
    if (result) {
        fprintf(stderr,
                "Failed to run function SerialICE_cpuid_filter: %s\n",
                lua_tostring(L, -1));
        exit(1);
    }

    ret |= lua_toboolean(L, -1) ? WRITE_TO_QEMU : 0;
    ret |= lua_toboolean(L, -2) ? WRITE_TO_SERIALICE : 0;
    lua_pop(L, 2);
    return ret;
}

/* SerialICE output loggers */

static void __read_post(int flags, uint32_t *data)
{
    int result;

    if (flags & LOG_MEMORY) {
        lua_getglobal(L, "SerialICE_memory_read_log");
    } else if (flags & LOG_IO) {
        lua_getglobal(L, "SerialICE_io_read_log");
    } else {
        fprintf(stderr, "serialice_read_log: bad type\n");
        exit(1);
    }

    lua_pushinteger(L, *data);
    result = lua_pcall(L, 1, 1, 0);
    if (result) {
        fprintf(stderr, "Failed to run function SerialICE_%s_read_log: %s\n",
                (flags & LOG_MEMORY) ? "memory" : "io", lua_tostring(L, -1));
        exit(1);
    }
    *data = lua_tointeger(L, -1);
    lua_pop(L, 1);
}

static void __write_post(int flags)
{
    int result;

    if (flags & LOG_MEMORY) {
        lua_getglobal(L, "SerialICE_memory_write_log");
    } else if (flags & LOG_IO) {
        lua_getglobal(L, "SerialICE_io_write_log");
    } else if (flags & LOG_MSR) {
        lua_getglobal(L, "SerialICE_msr_write_log");
    } else {
        fprintf(stderr, "serialice_write_log: bad type\n");
        exit(1);
    }

    result = lua_pcall(L, 0, 0, 0);
    if (result) {
        fprintf(stderr, "Failed to run function SerialICE_%s_write_log: %s\n",
                (flags & LOG_MEMORY) ? "memory" : "io", lua_tostring(L, -1));
        exit(1);
    }
}

static void memory_read_post(uint32_t * data)
{
    __read_post(LOG_MEMORY, data);
}

static void memory_write_post(void)
{
    __write_post(LOG_MEMORY);
}

static void io_read_post(uint32_t * data)
{
    __read_post(LOG_IO, data);
}

static void io_write_post(void)
{
    __write_post(LOG_IO);
}

static void wrmsr_post(void)
{
    __write_post(LOG_MSR);
}

static void rdmsr_post(uint32_t *hi, uint32_t *lo)
{
    int result;

    lua_getglobal(L, "SerialICE_msr_read_log");
    lua_pushinteger(L, *hi);
    lua_pushinteger(L, *lo);

    result = lua_pcall(L, 2, 2, 0);
    if (result) {
        fprintf(stderr, "Failed to run function SerialICE_msr_read_log: %s\n",
			lua_tostring(L, -1));
        exit(1);
    }
    *hi = lua_tointeger(L, -2);
    *lo = lua_tointeger(L, -1);
    lua_pop(L, 2);
}

static void cpuid_post(cpuid_regs_t * res)
{
    int result;

    lua_getglobal(L, "SerialICE_cpuid_log");
    lua_pushinteger(L, res->eax);        // output: eax
    lua_pushinteger(L, res->ebx);        // output: ebx
    lua_pushinteger(L, res->ecx);        // output: ecx
    lua_pushinteger(L, res->edx);        // output: edx

    result = lua_pcall(L, 4, 4, 0);
    if (result) {
        fprintf(stderr, "Failed to run function SerialICE_cpuid_log: %s\n",
                lua_tostring(L, -1));
        exit(1);
    }
    res->edx = lua_tointeger(L, -1);
    res->ecx = lua_tointeger(L, -2);
    res->ebx = lua_tointeger(L, -3);
    res->eax = lua_tointeger(L, -4);
    lua_pop(L, 4);
}

static const SerialICE_filter lua_ops = {
    .io_read_pre = io_read_pre,
    .io_read_post = io_read_post,
    .io_write_pre = io_write_pre,
    .io_write_post = io_write_post,
    .load_pre = memory_read_pre,
    .load_post = memory_read_post,
    .store_pre = memory_write_pre,
    .store_post = memory_write_post,
    .rdmsr_pre = rdmsr_pre,
    .rdmsr_post = rdmsr_post,
    .wrmsr_pre = wrmsr_pre,
    .wrmsr_post = wrmsr_post,
    .cpuid_pre = cpuid_pre,
    .cpuid_post = cpuid_post,
};
