-- **********************************************************
--

function io_undefined(f, action)
	action.to_hw = true
	action.to_qemu = false
	action.undefined = true
	return true
end

function io_post(f, action)
	if (action.write) then
		printk(f, action, "out%s %04x <= %s\n", size_suffix(action.size), action.addr, size_data(action.size, action.data))
	else
		printk(f, action, " in%s %04x => %s\n", size_suffix(action.size), action.addr, size_data(action.size, action.data))
	end
	return true
end

function io_base_post(f, action)
	if (action.write) then
		printk(f, action, "[%04x] <= %s\n", action.addr - f.base, size_data(action.size, action.data))
	else
		printk(f, action, "[%04x] => %s\n", action.addr - f.base, size_data(action.size, action.data))
	end
	return true
end

filter_io_fallback = {
	id = -1,
	name = "IO",
	pre = io_undefined,
	post = io_post,
	base = 0x0,
	size = 0x10000,
}

-- **********************************************************
--

function mem_undefined(f, action)
	action.to_hw = true
	action.to_qemu = false
	action.undefined = true
	return true
end

function mem_post(f, action)
	if (action.write) then
		printk(f, action, "write%s %08x <= %s", size_suffix(action.size), action.addr, size_data(action.size, action.data))
	else
		printk(f, action, " read%s %08x => %s", size_suffix(action.size), action.addr, size_data(action.size, action.data))
	end
	if action.to_hw then
		printf(" *")
	end
	printf("\n")
	return true
end

function mem_base_post(f, action)
	if (action.write) then
		printk(f, action, "[%08x] <= %s\n", bit32.band(action.addr, (f.size - 1)), size_data(action.size, action.data))
	else
		printk(f, action, "[%08x] => %s\n", bit32.band(action.addr, (f.size - 1)), size_data(action.size, action.data))
	end
	return true
end


filter_mem_fallback = {
	id = -1,
	name = "MEM",
	pre = mem_undefined,
	post = mem_post,
	base = 0x0,
	size = 0x100000000
}

