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
#include "hw/pc.h"
#include "serialice.h"
#include "sysemu.h"

#define SERIALICE_DEBUG 3
#define BUFFER_SIZE 1024
typedef struct {
#ifdef WIN32
	HANDLE fd;
#else
	int fd;
#endif
	char *buffer;
	char *command;
} SerialICEState;

static SerialICEState *s;

int serialice_active = 0;
const char *serialice_lua_script="serialice.lua";

#ifndef WIN32
static struct termios options;
#endif

static lua_State *L;

// **************************************************************************
// LUA scripting interface and callbacks

static int serialice_register_physical(lua_State *luastate)
{
	int n = lua_gettop(luastate);
	uint32_t addr, size;
	ram_addr_t phys;

	if (n != 2) {
		fprintf(stderr, "ERROR: Not called as SerialICE_register_physical(<addr> <size>)\n");
		return 0;
	}

	addr = lua_tointeger(luastate, 1);
	size = lua_tointeger(luastate, 2);

	printf("Registering physical memory at 0x%08x (0x%08x bytes)\n", addr, size);
	phys = qemu_ram_alloc(size);
	cpu_register_physical_memory(addr, size, phys);

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

    /* Load the script file */
    status = luaL_loadfile(L, serialice_lua_script);
    if (status) {
        fprintf(stderr, "Couldn't load SerialICE script: %s\n", lua_tostring(L, -1));
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

static int serialice_lua_exit(void)
{
        lua_close(L);
	return 0;
}

static int serialice_io_read_filter(uint32_t *data, uint16_t port, int size)
{
    int ret, result;

    lua_getfield(L, LUA_GLOBALSINDEX, "SerialICE_io_read_filter");
    lua_pushinteger(L, port); // port
    lua_pushinteger(L, size); // datasize
    result = lua_pcall(L, 2, 2, 0);
    if (result) {
        fprintf(stderr, "Failed to run function SerialICE_io_read_filter: %s\n", lua_tostring(L, -1));
        exit(1);
    }
    *data = lua_tointeger(L, -1);
    ret = lua_toboolean(L, -2);
    lua_pop(L, 2);

    return ret;
} 

static int serialice_io_write_filter(uint32_t *data, uint16_t port, int size)
{
    int ret, result;

    lua_getfield(L, LUA_GLOBALSINDEX, "SerialICE_io_write_filter");
    lua_pushinteger(L, port); // port
    lua_pushinteger(L, size); // datasize
    lua_pushinteger(L, *data); // data

    result = lua_pcall(L, 3, 2, 0);
    if (result) {
        fprintf(stderr, "Failed to run function SerialICE_io_write_filter: %s\n", lua_tostring(L, -1));
        exit(1);
    }
    *data = lua_tointeger(L, -1);
    ret = lua_toboolean(L, -2);
    lua_pop(L, 2);

    return ret;
} 


#define READ_FROM_QEMU		(1 << 0)
#define READ_FROM_SERIALICE	(1 << 1)
static int serialice_memory_read_filter(uint32_t addr, uint32_t *data, int size)
{
    int ret = 0, result;

    lua_getfield(L, LUA_GLOBALSINDEX, "SerialICE_memory_read_filter");
    lua_pushinteger(L, addr); // addr
    lua_pushinteger(L, size); // datasize
    result = lua_pcall(L, 2, 3, 0);
    if (result) {
        fprintf(stderr, "Failed to run function SerialICE_memory_read_filter: %s\n", lua_tostring(L, -1));
        exit(1);
    }

    *data = lua_tointeger(L, -1); // result

    ret |= lua_toboolean(L, -2) ? READ_FROM_QEMU : 0; // to_qemu
    ret |= lua_toboolean(L, -3) ? READ_FROM_SERIALICE : 0; // to_hw

    lua_pop(L, 3);

    return ret;
} 

#define WRITE_TO_QEMU		(1 << 0)
#define WRITE_TO_SERIALICE	(1 << 1)

static int serialice_memory_write_filter(uint32_t addr, int size, uint32_t *data)
{
    int ret = 0, result;
    int write_to_qemu, write_to_serialice;

    lua_getfield(L, LUA_GLOBALSINDEX, "SerialICE_memory_write_filter");
    lua_pushinteger(L, addr); // address
    lua_pushinteger(L, size); // datasize
    lua_pushinteger(L, *data); // data
    result = lua_pcall(L, 3, 3, 0);
    if (result) {
        fprintf(stderr, "Failed to run function SerialICE_memory_write_filter: %s\n", lua_tostring(L, -1));
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

static int serialice_msr_filter(int flags, uint32_t addr, uint32_t *hi, uint32_t *lo)
{
    int ret, result;

    if (flags & FILTER_WRITE)
    	lua_getfield(L, LUA_GLOBALSINDEX, "SerialICE_msr_write_filter");
    else
    	lua_getfield(L, LUA_GLOBALSINDEX, "SerialICE_msr_read_filter");

    lua_pushinteger(L, addr); // port
    lua_pushinteger(L, *hi);  // high
    lua_pushinteger(L, *lo);  // low
    result = lua_pcall(L, 3, 3, 0);
    if (result) {
        fprintf(stderr, "Failed to run function SerialICE_msr_read_filter: %s\n", lua_tostring(L, -1));
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

static int serialice_cpuid_filter(cpuid_regs_t *regs)
{
    int ret, result;

    lua_getfield(L, LUA_GLOBALSINDEX, "SerialICE_cpuid_filter");

    lua_pushinteger(L, regs->eax); // eax
    lua_pushinteger(L, regs->ecx); // ecx
    result = lua_pcall(L, 2, 5, 0);
    if (result) {
        fprintf(stderr, "Failed to run function SerialICE_msr_read_filter: %s\n", lua_tostring(L, -1));
        exit(1);
    }

    ret = lua_toboolean(L, -5);
    if (ret) {
    	regs->eax = lua_tointeger(L, -1);
    	regs->ebx = lua_tointeger(L, -2);
    	regs->ecx = lua_tointeger(L, -3);
    	regs->edx = lua_tointeger(L, -4);
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

    if ((flags & LOG_WRITE) && (flags & LOG_MEMORY))
    	lua_getfield(L, LUA_GLOBALSINDEX, "SerialICE_memory_write_log");
    else if (!(flags & LOG_WRITE) && (flags & LOG_MEMORY))
    	lua_getfield(L, LUA_GLOBALSINDEX, "SerialICE_memory_read_log");
    else if ((flags & LOG_WRITE) && !(flags & LOG_MEMORY))
    	lua_getfield(L, LUA_GLOBALSINDEX, "SerialICE_io_write_log");
    else // if (!(flags & LOG_WRITE) && !(flags & LOG_MEMORY))
    	lua_getfield(L, LUA_GLOBALSINDEX, "SerialICE_io_read_log");

    lua_pushinteger(L, addr); // addr/port
    lua_pushinteger(L, size); // datasize
    lua_pushinteger(L, data); // data
    lua_pushboolean(L, ((flags & LOG_TARGET) != 0));

    result = lua_pcall(L, 4, 0, 0);
    if (result) {
        fprintf(stderr, "Failed to run function SerialICE_%s_%s_log: %s\n",
			(flags & LOG_MEMORY)?"memory":"io", 
			(flags & LOG_WRITE)?"write":"read",
			lua_tostring(L, -1));
        exit(1);
    }
} 

static void serialice_msr_log(int flags, uint32_t addr, uint32_t hi, uint32_t lo, int filtered)
{
    int result;

    if (flags & LOG_WRITE)
    	lua_getfield(L, LUA_GLOBALSINDEX, "SerialICE_msr_write_log");
    else // if (!(flags & LOG_WRITE))
    	lua_getfield(L, LUA_GLOBALSINDEX, "SerialICE_msr_read_log");

    lua_pushinteger(L, addr); // addr/port
    lua_pushinteger(L, hi); // datasize
    lua_pushinteger(L, lo); // data
    lua_pushboolean(L, filtered); // data
    result = lua_pcall(L, 4, 0, 0);
    if (result) {
        fprintf(stderr, "Failed to run function SerialICE_msr_%s_log: %s\n",
			(flags & LOG_WRITE)?"write":"read",
			lua_tostring(L, -1));
        exit(1);
    }
}

static void serialice_cpuid_log(uint32_t eax, uint32_t ecx, cpuid_regs_t res, int filtered)
{
    int result;

    lua_getfield(L, LUA_GLOBALSINDEX, "SerialICE_cpuid_log");

    lua_pushinteger(L, eax); // input: eax
    lua_pushinteger(L, ecx); // input: ecx
    lua_pushinteger(L, res.eax); // output: eax
    lua_pushinteger(L, res.ebx); // output: ebx
    lua_pushinteger(L, res.ecx); // output: ecx
    lua_pushinteger(L, res.edx); // output: edx
    lua_pushboolean(L, filtered); // data
    result = lua_pcall(L, 7, 0, 0);
    if (result) {
        fprintf(stderr, "Failed to run function SerialICE_cpuid_log: %s\n",
			lua_tostring(L, -1));
        exit(1);
    }
}


// **************************************************************************
// low level communication with the SerialICE shell (serial communication)

static int serialice_read(SerialICEState *state, void *buf, size_t nbyte)
{
	int bytes_read = 0;

	while (1) {
#ifdef WIN32
		int ret;
		if (!ReadFile(state->fd, buf, nbyte - bytes_read, &ret, NULL))
			break;
#else
		int ret = read(state->fd, buf, nbyte - bytes_read);

		if (ret == -1 && errno == EINTR)
			continue;

		if (ret == -1)
			break;
#endif

		bytes_read += ret;
		buf += ret;

		if (bytes_read >= (int)nbyte)
			break;
	}

	return bytes_read;
}

static int serialice_write(SerialICEState *state, const void *buf, size_t nbyte)
{
	char *buffer = (char *) buf;
	char c;
	int i;

	for (i = 0; i < (int)nbyte; i++) {
#ifdef WIN32
		int ret = 0;
		while (ret == 0) WriteFile(state->fd, buffer + i, 1, &ret, NULL);
		ret = 0;
		while (ret == 0) ReadFile(state->fd, &c, 1, &ret, NULL);
#else
		while (write(state->fd, buffer + i, 1) != 1) ;
		while (read(state->fd, &c, 1) != 1) ;
#endif
		if (c != buffer[i]) {
			printf("Readback error! %x/%x\n", c, buffer[i]);
		}
	}

	return nbyte;
}

static int serialice_wait_prompt(void)
{
	char buf[3];
	int l;

	l = serialice_read(s, buf, 3);

	if (l == -1) {
		perror("SerialICE: Could not read from target");
		exit(1);
	}

	while (buf[0] != '\n' || buf[1] != '>' || buf[2] != ' ') {
		buf[0] = buf[1];
		buf[1] = buf[2];
		l = serialice_read(s, buf + 2, 1);
		if (l == -1) {
			perror("SerialICE: Could not read from target");
			exit(1);
		}
	}

	return 0;
}

static void serialice_command(const char *command, int reply_len)
{
#if SERIALICE_DEBUG > 5
	int i;
#endif
	int l;

	serialice_wait_prompt();

	serialice_write(s, command, strlen(command));
	
	memset(s->buffer, 0, reply_len + 1); // clear enough of the buffer

	l = serialice_read(s, s->buffer, reply_len);

	if (l == -1) {
		perror("SerialICE: Could not read from target");
		exit(1);
	}

	if (l != reply_len) {
		printf("SerialICE: command was not answered sufficiently: "
				"(%d/%d bytes)\n'%s'\n", l, reply_len, s->buffer);
		exit(1);
	}

#if SERIALICE_DEBUG > 5
	for (i=0; i < reply_len; i++)
		printf("%02x ", s->buffer[i]);
	printf("\n");
#endif
}


// **************************************************************************
// high level communication with the SerialICE shell

uint8_t serialice_inb(uint16_t port)
{
	uint8_t ret;
	uint32_t data;

	if (serialice_io_read_filter(&data, port, 1))
		return data & 0xff;

	sprintf(s->command, "*ri%04x.b", port);
	// command read back: "\n00" (3 characters)
	serialice_command(s->command, 3);
	ret = (uint8_t)strtoul(s->buffer + 1, (char **)NULL, 16);

	serialice_log(LOG_READ|LOG_IO, ret, port, 1);

	return ret;
}

uint16_t serialice_inw(uint16_t port)
{
	uint16_t ret;
	uint32_t data;

	if (serialice_io_read_filter(&data, port, 1))
		return data & 0xffff;

	sprintf(s->command, "*ri%04x.w", port);
	// command read back: "\n0000" (5 characters)
	serialice_command(s->command, 5);
	ret = (uint16_t)strtoul(s->buffer + 1, (char **)NULL, 16);

	serialice_log(LOG_READ|LOG_IO, ret, port, 2);

	return ret;
}

uint32_t serialice_inl(uint16_t port)
{
	uint32_t ret;
	uint32_t data;

	if (serialice_io_read_filter(&data, port, 1))
		return data;

	sprintf(s->command, "*ri%04x.l", port);
	// command read back: "\n00000000" (9 characters)
	serialice_command(s->command, 9);
	ret = (uint32_t)strtoul(s->buffer + 1, (char **)NULL, 16);

	serialice_log(LOG_READ|LOG_IO, ret, port, 4);

	return ret;
}

void serialice_outb(uint8_t data, uint16_t port)
{
	uint32_t filtered_data = (uint32_t)data;

	serialice_log(LOG_WRITE|LOG_IO, data, port, 1);

	if (serialice_io_write_filter(&filtered_data, port, 1)) {
		return;
	}

	data = (uint8_t)filtered_data;
	sprintf(s->command, "*wi%04x.b=%02x", port, data);
	serialice_command(s->command, 0);
}

void serialice_outw(uint16_t data, uint16_t port)
{
	uint32_t filtered_data = (uint32_t)data;

	serialice_log(LOG_WRITE|LOG_IO, data, port, 2);

	if (serialice_io_write_filter(&filtered_data, port, 2)) {
		return;
	}

	data = (uint16_t)filtered_data;
	sprintf(s->command, "*wi%04x.w=%04x", port, data);
	serialice_command(s->command, 0);
}

void serialice_outl(uint32_t data, uint16_t port)
{
	uint32_t filtered_data = data;

	serialice_log(LOG_WRITE|LOG_IO, data, port, 4);

	if (serialice_io_write_filter(&filtered_data, port, 4)) {
		return;
	}

	data = filtered_data;
	sprintf(s->command, "*wi%04x.l=%08x", port, data);
	serialice_command(s->command, 0);
}

uint8_t serialice_readb(uint32_t addr)
{
	uint8_t ret;
	sprintf(s->command, "*rm%08x.b", addr);
	// command read back: "\n00" (3 characters)
	serialice_command(s->command, 3);
	ret = (uint8_t)strtoul(s->buffer + 1, (char **)NULL, 16);
	return ret;
}

uint16_t serialice_readw(uint32_t addr)
{
	uint16_t ret;
	sprintf(s->command, "*rm%08x.w", addr);
	// command read back: "\n0000" (5 characters)
	serialice_command(s->command, 5);
	ret = (uint16_t)strtoul(s->buffer + 1, (char **)NULL, 16);
	return ret;
}

uint32_t serialice_readl(uint32_t addr)
{
	uint32_t ret;
	sprintf(s->command, "*rm%08x.l", addr);
	// command read back: "\n00000000" (9 characters)
	serialice_command(s->command, 9);
	ret = (uint32_t)strtoul(s->buffer + 1, (char **)NULL, 16);
	return ret;
}

void serialice_writeb(uint8_t data, uint32_t addr)
{
	sprintf(s->command, "*wm%08x.b=%02x", addr, data);
	serialice_command(s->command, 0);
}

void serialice_writew(uint16_t data, uint32_t addr)
{
	sprintf(s->command, "*wm%08x.w=%04x", addr, data);
	serialice_command(s->command, 0);
}

void serialice_writel(uint32_t data, uint32_t addr)
{
	sprintf(s->command, "*wm%08x.l=%08x", addr, data);
	serialice_command(s->command, 0);
}

uint64_t serialice_rdmsr(uint32_t addr, uint32_t key)
{
	uint32_t hi, lo;
	uint64_t ret;
	int filtered;

	filtered = serialice_msr_filter(FILTER_READ, addr, &hi, &lo);
	if (!filtered) {
		sprintf(s->command, "*rc%08x.%08x", addr, key);

		// command read back: "\n00000000.00000000" (18 characters)
		serialice_command(s->command, 18);

		s->buffer[9] = 0; // . -> \0
		hi = (uint32_t)strtoul(s->buffer + 1, (char **)NULL, 16);
		lo = (uint32_t)strtoul(s->buffer + 10, (char **)NULL, 16);
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
		sprintf(s->command, "*wc%08x.%08x=%08x.%08x", addr, key, hi, lo);
		serialice_command(s->command, 0);
	}

	serialice_msr_log(LOG_WRITE, addr, hi, lo, filtered);
}

cpuid_regs_t serialice_cpuid(uint32_t eax, uint32_t ecx)
{
	cpuid_regs_t ret;
	int filtered;

	ret.eax = eax;
	ret.ebx = 0; // either set by filter or by target
	ret.ecx = ecx;
	ret.edx = 0; // either set by filter or by target

	filtered = serialice_cpuid_filter(&ret);
	if (!filtered) {
		sprintf(s->command, "*ci%08x.%08x", eax, ecx);

		// command read back: "\n000006f2.00000000.00001234.12340324"
		// (36 characters)
		serialice_command(s->command, 36);

		s->buffer[9] = 0; // . -> \0
		s->buffer[18] = 0; // . -> \0
		s->buffer[27] = 0; // . -> \0
		ret.eax = (uint32_t)strtoul(s->buffer +  1, (char **)NULL, 16);
		ret.ebx = (uint32_t)strtoul(s->buffer + 10, (char **)NULL, 16);
		ret.ecx = (uint32_t)strtoul(s->buffer + 19, (char **)NULL, 16);
		ret.edx = (uint32_t)strtoul(s->buffer + 28, (char **)NULL, 16);
	}

	serialice_cpuid_log(eax, ecx, ret, filtered);

	return ret;
}

// **************************************************************************
// memory load handling

static uint32_t serialice_load_wrapper(uint32_t addr, unsigned int size)
{
	switch (size) {
	case 1: return (uint32_t)serialice_readb(addr);
	case 2: return (uint32_t)serialice_readw(addr);
	case 4: return (uint32_t)serialice_readl(addr);
	default: printf("WARNING: unknown read access size %d @%08x\n", size, addr);
	}
	return 0;
}

/**
 * This function is called by the softmmu engine to update the status
 * of a load cycle
 */
void serialice_log_load(int caught, uint32_t addr, uint32_t result, unsigned int data_size)
{
	if (caught)
		serialice_log(LOG_READ|LOG_MEMORY|LOG_TARGET, result, addr, data_size);
	else
		serialice_log(LOG_READ|LOG_MEMORY, result, addr, data_size);
}

/* This function can grab Qemu load ops and forward them to the SerialICE
 * target. 
 *
 * @return 0: pass on to Qemu; 1: handled locally.
 */
int serialice_handle_load(uint32_t addr, uint32_t *result, unsigned int data_size)
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

static void serialice_store_wrapper(uint32_t addr, unsigned int size, uint32_t data)
{
	switch (size) {
	case 1: serialice_writeb((uint8_t)data, addr); break;
	case 2: serialice_writew((uint16_t)data, addr); break;
	case 4: serialice_writel((uint32_t)data, addr); break;
	default: printf("WARNING: unknown write access size %d @%08x\n", size, addr);
	}
}

static void serialice_log_store(int caught, uint32_t addr, uint32_t val, unsigned int data_size)
{
	if (caught)
		serialice_log(LOG_WRITE|LOG_MEMORY|LOG_TARGET, val, addr, data_size);
	else
		serialice_log(LOG_WRITE|LOG_MEMORY, val, addr, data_size);
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

	if (write_to_target)
		serialice_store_wrapper(addr, data_size, filtered_data);

	return (write_to_qemu == 0);
}

// **************************************************************************
// external initialization and exit

void serialice_init(void)
{
	printf("SerialICE: Open connection to target hardware...\n");

	if (serialice_device == NULL) {
		printf("You need to specify a serial device to use SerialICE.\n");
		exit(1);
	}

	s =  qemu_mallocz(sizeof(SerialICEState));
#ifdef WIN32
	s->fd = CreateFile(serialice_device, GENERIC_READ | GENERIC_WRITE,
		0, NULL, OPEN_EXISTING, 0, NULL);

	if (s->fd == INVALID_HANDLE_VALUE) {
		perror("SerialICE: Could not connect to target TTY");
		exit(1);
	}

	DCB dcb;
	if (!GetCommState(s->fd, &dcb)) {
		perror("SerialICE: Could not load config for target TTY");
		exit(1);
	}

	dcb.BaudRate = 115200;
	dcb.ByteSize = 8;
	dcb.Parity = NOPARITY;
	dcb.StopBits = ONESTOPBIT;
	dcb.fDtrControl = DTR_CONTROL_DISABLE;
	dcb.fInX = FALSE;

	if (!SetCommState(s->fd, &dcb)) {
		perror("SerialICE: Could not store config for target TTY");
		exit(1);
	}

	COMMTIMEOUTS to;
	if (!GetCommTimeouts(s->fd, &to)) {
		perror("SerialICE: Could not load timeouts for target TTY");
		exit(1);
	}

	to.ReadIntervalTimeout = 1000;
	to.ReadTotalTimeoutMultiplier = 0;
	to.ReadTotalTimeoutConstant = 0;

	if (!SetCommTimeouts(s->fd, &to)) {
		perror("SerialICE: Could not store timeouts for target TTY");
		exit(1);
	}
	
#else
	s->fd = open(serialice_device, O_RDWR | O_NOCTTY | O_NONBLOCK);

	if (s->fd == -1) {
		perror("SerialICE: Could not connect to target TTY");
		exit(1);
	}

	if (ioctl(s->fd, TIOCEXCL) == -1) {
		perror("SerialICE: TTY not exclusively available");
		exit(1);
	}

	if (fcntl(s->fd, F_SETFL, 0) == -1) {
		perror("SerialICE: Could not switch to blocking I/O");
		exit(1);
	}

	if (tcgetattr(s->fd, &options) == -1) {
		perror("SerialICE: Could not get TTY attributes");
		exit(1);
	}

	cfsetispeed(&options, B115200);
	cfsetospeed(&options, B115200);

	/* set raw input, 1 second timeout */
	options.c_cflag     |= (CLOCAL | CREAD);
	options.c_lflag     &= ~(ICANON | ECHO | ECHOE | ISIG);
	options.c_oflag     &= ~OPOST;
	options.c_iflag     |= IGNCR;
	options.c_cc[VMIN]  = 0;
	options.c_cc[VTIME] = 100;

	tcsetattr(s->fd, TCSANOW, &options);

	tcflush(s->fd, TCIOFLUSH);
#endif

	s->buffer = qemu_mallocz(BUFFER_SIZE);
	s->command = qemu_mallocz(BUFFER_SIZE);

	printf("SerialICE: Waiting for handshake with target... ");

	/* Trigger a prompt */
	serialice_write(s, "\n", 1);

	/* ... and wait for it to appear */
	if (serialice_wait_prompt() == 0) {
		printf("target alife!\n");
	} else {
		printf("target not ok!\n" );
		exit(1);
	}

	/* Each serialice_command() waits for a prompt, so trigger one for the
	 * first command, as we consumed the last one for the handshake
	 */
	serialice_write(s, "\n", 1);

	printf("SerialICE: LUA init...\n");
	serialice_lua_init();
}

void serialice_exit(void)
{
	serialice_lua_exit();
	qemu_free(s->command);
	qemu_free(s->buffer);
	qemu_free(s);
}

