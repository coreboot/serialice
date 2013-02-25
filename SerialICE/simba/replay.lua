

function SerialICE_register_physical()
end

function SerialICE_system_reset()
end

SerialICE_mainboard = "undetected"

regs = { eax, ebc, ecx, edx, cs=0, eip=0, ds, es, ss, gs, fs, }
ids = { parent, this, }

function pci_bdf_noext(bus, dev, func, reg)
	return bus*65536 + dev*2048 + func*256 + reg
end

function replay_io(dir_wr, addr, size, data)
	pre_action(io_action, dir_wr, addr, size, data)
	walk_pre_hooks(io_hooks, io_action)
	io_action.data = data
	post_action(io_action, data)
	walk_post_hooks(io_hooks, io_action)
end

function replay_mem(dir_wr, addr, size, data)
	pre_action(mem_action, dir_wr, addr, size, data)
	walk_pre_hooks(mem_hooks, mem_action)
	mem_action.data = data
	post_action(mem_action, data)
	walk_post_hooks(mem_hooks, mem_action)
end

function replay_unknown(str)
	local dummy = {}
	pre_action(dummy, false, 0, 0, 0)
	post_action(dummy, 0)
	print_address(ids.parent, ids.this, "...", regs.cs, regs.eip)
	printf(str)
	printf("\n")
end

function parse_cpu(line)
	if string.find(line, "CPUID") then
		replay_unknown(line)
		return true
	end
	if string.find(line, "CPU MSR") then
		replay_unknown(line)
		return true
	end
	return false
end


function parse_io(line)
	local io_op = "IO[^:]*:?%s+%a+%s+(%x+)%s+(<?=>?)%s+(%x+)"
	local found, addr, dir, data
	found, _, addr, dir, data = string.find(line, io_op)
	if not found then
		return false
	end
	local naddr = tonumber(addr, 16)
	local ndata = tonumber(data, 16)
	local nsize = string.len(data)/2
	if string.find("<=", dir) then
		replay_io(true, naddr, nsize, ndata)
	else
		replay_io(false, naddr, nsize, ndata)
	end
	return true
end

function parse_mem(line)
	local mem_op = "MEM[^:]*:?%s+%a+%s+(%x+)%s+(<?=>?)%s+(%x+)"
	local found, addr, dir, data
	found, _, addr, dir, data = string.find(line, mem_op)
	if not found then
		return false
	end
	local naddr = tonumber(addr, 16)
	local ndata = tonumber(data, 16)
	local nsize = string.len(data)/2
	if string.find("<=", dir) then
		replay_mem(true, naddr, nsize, ndata)
	else
		replay_mem(false, naddr, nsize, ndata)
	end
	return true
end

-- Old script already parsed PCI config, synthesize those IOs back.
function parse_pci(line)
	local found, bus, dev, fn, reg, dir, data
	local pci_cfg = "PCI:?%s+(%x):(%x+).(%x+)%s+R.(%x+)%s+(<?=>?)%s+(%x+)"
	found, _, bus, dev, fn, reg, dir, data = string.find(line, pci_cfg)
	if not found then
		local pci_cfg_empty = "PCI:?%s+(%x):(%x+).(%x+)%s+R.(%x+)"
		if string.find(line, pci_cfg_empty) then
			return true
		end
		return false
	end

	local nreg = bit32.band(0xfc, tonumber(reg,16))
	local noff = bit32.band(0x03, tonumber(reg,16))
	local ndata = tonumber(data,16)
	local nsize = string.len(data)/2

	replay_io(true, 0xcf8, 4, pci_bdf_noext(tonumber(bus,16), tonumber(dev,16), tonumber(fn,16), nreg))
	if string.find("<=", dir) then
		replay_io(true, 0xcfc + noff, nsize, ndata)
	else
		replay_io(false, 0xcfc + noff, nsize, ndata)
	end
	return true
end

function parse_headers()
	while true do
		local found = false
		line = io.read("*line")
		if not found then
			local board
			found, _, board = string.find(line, "SerialICE: Mainboard...:%s+(.+)")
			if found then
				SerialICE_mainboard = board
			end
		end
--		io.write(line)
--		io.write("\n")
		if string.find(line, "LUA script initialized.") then
			return
		end
	end
end

function parse_file()
	while true do
		local iplog = false
		local found = false
		local line, str, cs, eip, a, b

		line = io.read("*line")
		if not line then
			return
		end

		regs.cs = 0
		regs.eip = 0
		ids.parent = 0
		ids.this = 0
		iplog, _, cs, eip, a, b, str = string.find(line, "%[(%x+):(%x+)%]%s+(%x+)[%.:](%x+)...(.*)")
		if iplog then
			regs.cs = tonumber(cs, 16)
			regs.eip = tonumber(eip, 16)
			ids.parent = tonumber(a, 16)
			ids.this = tonumber(b, 16)
		end

		if not iplog then
			str = line
		end

		if not found then
			found = parse_io(str)
		end
		if not found then
			found = parse_pci(str)
		end
		if not found then
			found = parse_mem(str)
		end
		if not found then
			found = parse_cpu(str)
		end
		if not found then
			--replay_unknown(str)
		end
	end
end

parse_headers()

dofile("serialice.lua")

parse_file()




