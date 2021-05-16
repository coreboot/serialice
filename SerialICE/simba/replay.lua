
replaying = true

function SerialICE_register_physical()
end

function SerialICE_system_reset()
end

SerialICE_mainboard = "undetected"

-- keep the previous default for backward compatibility of old dumps
SerialICE_rom_size  = 4 * 1024 * 1024

regs = { eax, ebc, ecx, edx, cs=0, eip=0, ds, es, ss, gs, fs, }
ids = { parent, this, flags}

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

function replay_msr(dir_wr, addr, hi, lo)
	pre_action(cpu_action, dir_wr, 0, 0, 0)
	load_regs(cpu_action.rin, lo, 0, addr, hi)
	walk_pre_hooks(cpumsr_hooks, cpu_action)
	post_action(cpu_action, 0)
	load_regs(cpu_action.rout, lo, 0, addr, hi)
	walk_post_hooks(cpumsr_hooks, cpu_action)
end

function replay_cpuid(idx0, idx1, eax, ebx, ecx, edx)
	pre_action(cpu_action, CPUID, 0, 0, 0)
	load_regs(cpu_action.rin, idx0, 0, idx1, 0)
	walk_pre_hooks(cpuid_hooks, cpu_action)
	post_action(cpu_action, 0)
	load_regs(cpu_action.rout, eax, ebx, ecx, edx)
	walk_post_hooks(cpuid_hooks, cpu_action)
end


function replay_unknown(str)
	local dummy = {}
	pre_action(dummy, false, 0, 0, 0)
	post_action(dummy, 0)
	print_address(ids.parent, ids.this, ids.flags, regs.cs, regs.eip)
	printf(str)
	printf("\n")
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

function parse_cpumsr(line)
	local msr_op = "CPU MSR%:%s+%[(%x+)%]%s+(<?=>?)%s+(%x+).(%x+)"
	local msr_op_old = "CPU%:%s+[:rdw:]*msr%s+(%x+)%s+(<?=>?)%s+(%x+).(%x+)"
	local found, ecx, dir, edx, eax
	found, _, ecx, dir, edx, eax = string.find(line, msr_op)
	if not found then
		found, _, ecx, dir, edx, eax = string.find(line, msr_op_old)
	end
	if not found then
		return false
	end
	local necx = tonumber(ecx, 16)
	local nedx = tonumber(edx, 16)
	local neax = tonumber(eax, 16)
	if string.find("<=", dir) then
		replay_msr(true, necx, nedx, neax)
	else
		replay_msr(false, necx, nedx, neax)
	end
	return true
end

function parse_cpuid(line)
	local id_op = "CPUID%: eax%: (%x+)%; ecx%: (%x+) => (%x+).(%x+).(%x+).(%x+)"
	local id_op_old = "CPU%: CPUID eax%: (%x+)%; ecx%: (%x+) => (%x+).(%x+).(%x+).(%x+)"
	local found, idx0, idx1, eax, ebx, ecx, edx
	found, _, idx0, idx1, eax, ebx, ecx, edx = string.find(line, id_op)
	if not found then
		found, _, idx0, idx1, eax, ebx, ecx, edx = string.find(line, id_op_old)
	end
	if not found then
		return false
	end
	local nidx0 = tonumber(idx0, 16)
	local nidx1 = tonumber(idx1, 16)
	local neax = tonumber(eax, 16)
	local nebx = tonumber(ebx, 16)
	local necx = tonumber(ecx, 16)
	local nedx = tonumber(edx, 16)
	replay_cpuid(nidx0, nidx1, neax, nebx, necx, nedx)
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

	local nreg = 0xfc & tonumber(reg,16)
	local noff = 0x03 & tonumber(reg,16)
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
	local found_mb   = false
	local found_size = false
	while true do
		line = io.read("*line")
		if not found_mb then
			local board
			found_mb, _, board = string.find(line, "SerialICE: Mainboard...:%s+(.+)")
			if found_mb then
				SerialICE_mainboard = board
			end
		end
		if not found_size then
			local board
			found_size, _, size = string.find(line, "SerialICE: ROM size....:%s+0x(.+)")
			if found_size then
				SerialICE_rom_size = tonumber(size, 16)
			end
		end
		if string.find(line, "LUA script initialized.") then
			return
		end
	end
end

function parse_file()
	while true do
		local iplog = false
		local found = false
		local line, str, cs, eip, a, b, f

		line = io.read("*line")
		if not line then
			return
		end

		if not iplog then
			iplog, _, a, b, f, cs, eip, str =
				string.find(line, "(%x+)[%.:](%x+)%s+([^ ]*)%s+%[(%x+):(%x+)%]...(.*)")
			if iplog and not f then
				f = "R###"
			end
		end

		-- Output format of 1st modular scripts
		if not iplog then
			iplog, _, cs, eip, a, b, str =
				string.find(line, "%[(%x+):(%x+)%]%s+(%x+)[%.:](%x+)...(.*)")
			if iplog and str then
				if string.find(str,"IO[,:]") or string.find(str,"MEM[,:]") or string.find(str,"CPU") then
					f = "R..."
				else
					f = "...."
				end
			end
		end

		-- Parse logfiles from before modular scripts
		if not iplog then
			regs.cs = 0
			regs.eip = 0
			ids.parent = 0
			ids.this = 0
			str = line
			if string.find(str,"IO:") or string.find(str,"MEM:") or string.find(str,"CPU") then
				f = "R..."
			else
				f = "...."
			end
			ids.flags = f
			found = parse_pci(str)
		end

		if iplog then
			regs.cs = tonumber(cs, 16)
			regs.eip = tonumber(eip, 16)
			ids.parent = tonumber(a, 16)
			ids.this = tonumber(b, 16)
			ids.flags = f
		end

		-- Drop rows that are not raw input
		if not string.find(ids.flags,"R.*") then
			found = true
			--printf("%s --- deleted\n", line)
		end

		if not found then
			found = parse_io(str)
		end
		if not found then
			found = parse_mem(str)
		end
		if not found then
			found = parse_cpuid(str)
		end
		if not found then
			found = parse_cpumsr(str)
		end
		if not found then
			found = replay_unknown(str)
		end
	end
end

parse_headers()
dofile("serialice.lua")
parse_file()
