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
#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>

/* Local includes */
#include "hw/hw.h"
#include "serialice.h"
#include "sysemu.h"

static lua_State *L;

extern const char *serialice_mainboard;
static const char *serialice_lua_script = "serialice.lua";

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

int serialice_lua_init(void)
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
int serialice_lua_exit(void)
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

int serialice_io_read_filter(uint32_t * data, uint16_t port, int size)
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

int serialice_io_write_filter(uint32_t * data, uint16_t port, int size)
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


int serialice_memory_read_filter(uint32_t addr, uint32_t * data,
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


int serialice_memory_write_filter(uint32_t addr, int size,
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

int serialice_wrmsr_filter(uint32_t addr, uint32_t * hi, uint32_t * lo)
{
    int ret, result;

    lua_getglobal(L, "SerialICE_msr_write_filter");
    lua_pushinteger(L, addr);   // port
    lua_pushinteger(L, *hi);    // high
    lua_pushinteger(L, *lo);    // low
    result = lua_pcall(L, 3, 3, 0);
    if (result) {
        fprintf(stderr,
                "Failed to run function SerialICE_msr_write_filter: %s\n",
                lua_tostring(L, -1));
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

int serialice_rdmsr_filter(uint32_t addr, uint32_t * hi, uint32_t * lo)
{
    int ret, result;

    lua_getglobal(L, "SerialICE_msr_read_filter");
    lua_pushinteger(L, addr);   // port
    lua_pushinteger(L, *hi);    // high
    lua_pushinteger(L, *lo);    // low
    result = lua_pcall(L, 3, 3, 0);
    if (result) {
        fprintf(stderr,
                "Failed to run function SerialICE_msr_read_filter: %s\n",
                lua_tostring(L, -1));
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

int serialice_cpuid_filter(uint32_t eax, uint32_t ecx,
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

void serialice_log(int flags, uint32_t data, uint32_t addr, int size)
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

void serialice_wrmsr_log(uint32_t addr, uint32_t hi,
                              uint32_t lo, int filtered)
{
    int result;

    lua_getglobal(L, "SerialICE_msr_write_log");
    lua_pushinteger(L, addr);   // addr/port
    lua_pushinteger(L, hi);     // datasize
    lua_pushinteger(L, lo);     // data
    lua_pushboolean(L, filtered);       // data
    result = lua_pcall(L, 4, 0, 0);
    if (result) {
        fprintf(stderr, "Failed to run function SerialICE_msr_write_log: %s\n",
                lua_tostring(L, -1));
        exit(1);
    }
}

void serialice_rdmsr_log(uint32_t addr, uint32_t hi,
                              uint32_t lo, int filtered)
{
    int result;

    lua_getglobal(L, "SerialICE_msr_read_log");
    lua_pushinteger(L, addr);   // addr/port
    lua_pushinteger(L, hi);     // datasize
    lua_pushinteger(L, lo);     // data
    lua_pushboolean(L, filtered);       // data
    result = lua_pcall(L, 4, 0, 0);
    if (result) {
        fprintf(stderr, "Failed to run function SerialICE_msr_read_log: %s\n",
                lua_tostring(L, -1));
        exit(1);
    }
}

void serialice_cpuid_log(uint32_t eax, uint32_t ecx, cpuid_regs_t res,
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
