
-- IO, MMIO, RAM and ROM access
io_action = {}
mem_action = {}

IO_READ = false
IO_WRITE = true
MEM_READ = false
MEM_WRITE = true

-- CPUID and CPU MSR
MSR_READ = false
MSR_WRITE = true
CPUID = false

cpu_action = {}
cpu_action.rin = {}
cpu_action.rout = {}



-- SerialICE_io_read_filter is the filter function for IO reads.
--
-- Parameters:
--   addr	IO port to be read
--   size	Size of the IO read
-- Return values:
--   to_hw	True if the read should be directed to the target
--   to_qemu	True if the read should be directed to Qemu

function SerialICE_io_read_filter(addr, size)
	pre_action(io_action, IO_READ, addr, size, 0)
	walk_pre_hooks(io_hooks, io_action)
	return io_action.to_hw, io_action.to_qemu
end

-- Parameters:
--   data	Data from hw or Qemu
-- Return values:
--   result	Data to give back to Qemu

function SerialICE_io_read_log(data)
	post_action(io_action, data)
	walk_post_hooks(io_hooks, io_action)
	return io_action.data
end

-- SerialICE_io_write_filter is the filter function for IO writes.
--
-- Parameters:
--   addr	IO port to be written to
--   size	Size of the IO write
--   data	Data to be written to
-- Return values:
--   to_hw	True if the write should be directed to the target
--   to_qemu	True if the write should be directed to Qemu
--   data	Data to be written (possible changed in filter)

function SerialICE_io_write_filter(addr, size, data)
	pre_action(io_action, IO_WRITE, addr, size, data)
	walk_pre_hooks(io_hooks, io_action)
	return io_action.to_hw, io_action.to_qemu, io_action.data
end

function SerialICE_io_write_log()
	post_action(io_action)
	walk_post_hooks(io_hooks, io_action)
end




-- SerialICE_memory_read_filter is the filter function for memory reads
--
-- Parameters:
--   addr	memory address to be read
--   size	Size of the memory read
-- Return values:
--   to_hw	True if the read should be directed to the target
--   to_qemu	True if the read should be directed to Qemu

function SerialICE_memory_read_filter(addr, size)
	pre_action(mem_action, MEM_READ, addr, size, 0)
	walk_pre_hooks(mem_hooks, mem_action)
	return mem_action.to_hw, mem_action.to_qemu
end

function SerialICE_memory_read_log(data)
	post_action(mem_action, data)
	walk_post_hooks(mem_hooks, mem_action)
	return mem_action.data
end


-- SerialICE_memory_write_filter is the filter function for memory writes
--
-- Parameters:
--   addr	memory address to write to
--   size	Size of the memory write
--   data	Data to be written
-- Return values:
--   to_hw	True if the write should be directed to the target
--   to_qemu	True if the write should be directed to Qemu
--   result	Data to be written (may be changed in filter)

function SerialICE_memory_write_filter(addr, size, data)
	pre_action(mem_action, MEM_WRITE, addr, size, data)
	walk_pre_hooks(mem_hooks, mem_action)
	return mem_action.to_hw, mem_action.to_qemu, mem_action.data
end

function SerialICE_memory_write_log()
	post_action(mem_action, 0)
	walk_post_hooks(mem_hooks, mem_action)
end

function SerialICE_msr_read_filter(addr)
	pre_action(cpu_action, MSR_READ, 0, 0, 0)
	load_regs(cpu_action.rin, 0, 0, addr, 0)

	walk_pre_hooks(cpumsr_hooks, cpu_action)
	return cpu_action.to_hw, cpu_action.to_qemu
end

function SerialICE_msr_read_log(hi, lo)
	local rout = cpu_action.rout
	post_action(cpu_action, 0)
	load_regs(cpu_action.rout, lo, 0, 0, hi)
	walk_post_hooks(cpumsr_hooks, cpu_action)
	return rout.edx, rout.eax
end

function SerialICE_msr_write_filter(addr, hi, lo)
	local rin = cpu_action.rin
	pre_action(cpu_action, MSR_WRITE, 0, 0, 0)
	load_regs(cpu_action.rin, lo, 0, addr, hi)
	walk_pre_hooks(cpumsr_hooks, cpu_action)
	return cpu_action.to_hw, cpu_action.to_qemu, rin.edx, rin.eax
end

function SerialICE_msr_write_log()
	post_action(cpu_action, 0)
	load_regs(cpu_action.rout, 0, 0, 0, 0)
	walk_post_hooks(cpumsr_hooks, cpu_action)
end

function SerialICE_cpuid_filter(eax, ecx)
	pre_action(cpu_action, CPUID, 0, 0, 0)
	load_regs(cpu_action.rin, eax, 0, ecx, 0)
	walk_pre_hooks(cpuid_hooks, cpu_action)
	return cpu_action.to_hw, cpu_action.to_qemu
end

function SerialICE_cpuid_log(eax, ebx, ecx, edx)
	local rout = cpu_action.rout
	post_action(cpu_action, 0)
	load_regs(cpu_action.rout, eax, ebx, ecx, edx)
	walk_post_hooks(cpuid_hooks, cpu_action)
	return rout.eax, rout.ebx, rout.ecx, rout.edx
end
