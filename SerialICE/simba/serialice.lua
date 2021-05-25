-- SerialICE
--
-- Copyright (c) 2009 coresystems GmbH
-- Copyright (c) 2012 Kyösti Mälkki <kyosti.malkki@gmail.com>
--
-- Permission is hereby granted, free of charge, to any person obtaining a copy
-- of this software and associated documentation files (the "Software"), to deal
-- in the Software without restriction, including without limitation the rights
-- to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
-- copies of the Software, and to permit persons to whom the Software is
-- furnished to do so, subject to the following conditions:
--
-- The above copyright notice and this permission notice shall be included in
-- all copies or substantial portions of the Software.
--
-- THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
-- IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
-- FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
-- THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
-- LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
-- OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
-- THE SOFTWARE.
--

io.write("SerialICE: Starting LUA script\n")

-- --------------------------------------------------------------------
-- This initialization is executed right after target communication
-- has been established

dofile("user_env.lua")
dofile("interface.lua")
dofile("output.lua")
dofile("hooks.lua")
dofile("core_io.lua")
dofile("memory.lua")
dofile("cpu.lua")
dofile("pci_cfg.lua")
dofile("mmio.lua")

io.write("SerialICE: LUA script initialized.\n")

function load_filter(str)
	local filename = "chipset/" .. str .. ".lua"
	dofile(filename)
end

load_filter("pc80")
load_filter("superio")

function do_minimal_setup()
	enable_hook(io_hooks, filter_io_fallback)
	enable_hook(mem_hooks, filter_mem_fallback)
	enable_hook(cpumsr_hooks, filter_cpumsr_fallback)
	enable_hook(cpuid_hooks, filter_cpuid_fallback)
	enable_hook(mem_hooks, filter_rom_low)
	enable_hook(mem_hooks, filter_rom_high)
end

function do_default_setup()
	enable_hook(mem_hooks, filter_lapic)
	enable_hook(mem_hooks, filter_ioapic)
	enable_hook(io_hooks, filter_pci_io_cfg)
	enable_hook_pc80()
	enable_hook_superio(0x2e, DEFAULT_SUPERIO_LDN_REGISTER)
	enable_hook_superio(0x4e, DEFAULT_SUPERIO_LDN_REGISTER)
	enable_hook(io_hooks, filter_com1)
	enable_hook(io_hooks, filter_com2)
	enable_hook(io_hooks, filter_com3)
	enable_hook(io_hooks, filter_com4)
	if superio_initialization then
		superio_initialization()
	end
end

root_info("ROM size: 0x%x\n", SerialICE_rom_size)
root_info("Mainboard %s connected.\n", SerialICE_mainboard)
local mainboard_file = string.format("mainboard/%s.lua", string.lower(string.gsub(SerialICE_mainboard, "[ -]", "_")))
local mainboard_lua, ferr = loadfile(mainboard_file)
local mainboard_script = io.open(mainboard_file)
if mainboard_script then
	io.close(mainboard_script)
	assert(mainboard_lua, ferr)
	mainboard_lua()
	root_info("Using script %s.\n", mainboard_file)
	do_minimal_setup()
	do_mainboard_setup()
else
	root_info("Script %s not found.\n", mainboard_file)
	do_minimal_setup()
	do_default_setup()
end

return true
