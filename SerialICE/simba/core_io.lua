
F_FIXED = 1
F_RANGE = 2

-- **********************************************************
--

function io_undefined(f, action)
	action.to_hw = true
	action.to_qemu = false
	return true
end

function io_post(f, action)
	local size = size_data(action.size, action.data)
	if not f.decode or f.decode == F_FIXED then
		if (action.write) then
			printk(f, action, "out%s %04x <= %s\n", size_suffix(action.size), action.addr, size)
		else
			printk(f, action, " in%s %04x => %s\n", size_suffix(action.size), action.addr, size)
		end
	elseif f.decode == F_RANGE then
		if (action.write) then
			printk(f, action, "[%04x] <= %s\n", action.addr - f.base, size)
		else
			printk(f, action, "[%04x] => %s\n", action.addr - f.base, size)
		end
	end
	return true
end

filter_io_fallback = {
	name = "IO",
	raw = true,
	pre = io_undefined,
	post = io_post,
	decode = F_FIXED,
	base = 0x0,
	size = 0x10000,
}

-- **********************************************************
--

function mem_undefined(f, action)
	action.to_hw = true
	action.to_qemu = false
	return true
end

function mem_post(f, action)
	local size = size_data(action.size, action.data)
	if not f.decode or f.decode == F_FIXED then
		if (action.write) then
			printk(f, action, "write%s %08x <= %s\n", size_suffix(action.size), action.addr, size)
		else
			printk(f, action, " read%s %08x => %s\n", size_suffix(action.size), action.addr, size)
		end
	elseif f.decode == F_RANGE then
		if (action.write) then
			printk(f, action, "[%08x] <= %s\n", action.addr & (f.size - 1), size)
		else
			printk(f, action, "[%08x] => %s\n", action.addr & (f.size - 1), size)
		end
	end
	return true
end

filter_mem_fallback = {
	name = "MEM",
	raw = true,
	pre = mem_undefined,
	post = mem_post,
	decode = F_FIXED,
	base = 0x0,
	size = 0x100000000
}
