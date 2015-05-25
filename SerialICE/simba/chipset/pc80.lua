-- **********************************************************
--
-- Debug POST port at IO 0x80

function debugport_post(f, action)
	if action.write then
		printk(f, action, "*** %02x ***\n", action.data)
		return true
	end
	return false
end

filter_debugport = {
	name = "POST",
	pre = handle_action,
	post = debugport_post,
	hide = true,
	base = 0x80,
	size = 0x1
}


-- **********************************************************
--
-- i8259 PIC

function i8259_pre(f, action)
	local master = ((0x05 >> action.addr) == 0x1)
	local slave = ((0x05 >> action.addr) == 0x5)
	local reg = (0x03 & action.addr)
	if reg == 0 or reg == 1 then
		return handle_action(f, action)
	end
	return skip_filter(f, action)
end

function i8259_post(f,action)
	local reg = (0x03 & action.addr)
	if reg == 0 or reg == 1 then
		return true
	end
	return false
end

function i8259_edge_pre(f, action)
	return handle_action(f, action)
end

function i8259_edge_post(f,action)
	return true
end

filter_i8259_master = {
	name = "i8259 A",
	pre = i8259_pre,
	post = i8259_post,
	hide = hide_i8259_io,
	base = 0x20,
	size = 0x20
}

filter_i8259_slave = {
	name = "i8259 B",
	pre = i8259_pre,
	post = i8259_post,
	hide = hide_i8259_io,
	base = 0xa0,
	size = 0x20
}

filter_i8259_edge = {
	name = "i8259 C",
	pre = i8259_edge_pre,
	post = i8259_edge_post,
	hide = hide_i8259_io,
	base = 0x4d0,
	size = 0x2
}

-- **********************************************************
--
-- i8237 DMA

function i8237_pre(f, action)
	if action.addr == 0x80 then
		return skip_filter(f, action)
	end
	return handle_action(f, action)
end

function i8237_post(f, action)
	if action.addr == 0x80 then
		return false
	end
	return true
end


filter_i8237_a = {
	name = "i8237 A",
	pre = i8237_pre,
	post = i8237_post,
	hide = hide_i8237_io,
	base = 0x00,
	size = 0x20
}
filter_i8237_b = {
	name = "i8237 B",
	pre = i8237_pre,
	post = i8237_post,
	hide = hide_i8237_io,
	base = 0x80,
	size = 0x20
}
filter_i8237_c = {
	name = "i8237 C",
	pre = i8237_pre,
	post = i8237_post,
	hide = hide_i8237_io,
	base = 0xc0,
	size = 0x20
}


-- **********************************************************
--
-- i8254 IRQ0 and Speaker

function i8254_pre(f, action)

	-- nothing to do on reads
	if not action.write then
		return handle_action(f, action)
	end

	local reg = (0x03 & action.addr)
	if reg >= 0x0 and reg < 0x03 then
		local counter_n = 0
		local counter_p = 0
		if f.counter[reg].lsb then
			f.counter[reg].lsb = f.counter[reg].after_lsb
			counter_n = action.data
			counter_p = (0xff00 & f.counter[reg].init)
		else
			counter_n = (action.data << 8)
			counter_p = (0x00ff & f.counter[reg].init)
		end
		f.counter[reg].init = (counter_n | counter_p)
	elseif reg == 0x03 then
		local reg2 = (action.data >> 6)
		local rwsel = 0x3 & (action.data >> 4)
		if reg2 == 0x3 then
			if (0x10 & action.data) == 0 then
				f.counter[0].readback = ((0x2 & action.data) ~= 0)
				f.counter[1].readback = ((0x4 & action.data) ~= 0)
				f.counter[2].readback = ((0x8 & action.data) ~= 0)
			end
			if (0x20 & action.data) == 0 then
				f.counter[0].latch = ((0x2 & action.data) ~= 0)
				f.counter[1].latch = ((0x4 & action.data) ~= 0)
				f.counter[2].latch = ((0x8 & action.data) ~= 0)
			end
		elseif rwsel == 0x0 then
			f.counter[reg2].latch = true
		else
			f.counter[reg2].mode = (0xf & action.data)
			if rwsel == 0x1 then
				f.counter[reg2].lsb = true
				f.counter[reg2].after_lsb = true
			elseif rwsel == 0x2 then
				f.counter[reg2].lsb = false
				f.counter[reg2].after_lsb = false
			elseif rwsel == 03 then
				f.counter[reg2].lsb = true
				f.counter[reg2].after_lsb = false
			end
		end
	end
	return handle_action(f, action)
end

function i8254_post(f, action)
	local reg = (0x03 & action.addr)
	if reg >= 0x0 and reg < 0x03 then
		if action.write then
			local mode = (0x0f & f.counter[reg].mode);
			local modestr = "Mode" .. mode
			if mode == 0x4 then
				modestr = "Square Wave"
			elseif mode == 0x6 then
				modestr = "Rate Generator"
			end
			if (0x01 & mode) ~= 0 then
				modestr = modestr .. " (BCD)"
			end

			local period = 838 * f.counter[reg].init
			if reg == 0 then
				if period == 0 then
					printk(f, action, "IRQ0 disabled\n")
				else
					printk(f, action, "IRQ0 (%s): %d ns\n", modestr, period)
				end
			elseif reg == 1 then
				if period == 0 then
					printk(f, action, "Refresh disabled\n")
				else
					printk(f, action, "Refresh (%s): %d ns\n", modestr, period)
				end
			elseif reg == 2 then
				if period ~= 0 then
					local spktone = 1193000 / f.counter[reg].init
					printk(f, action, "Speaker Tone (%s): %f kHz\n", modestr, spktone)
				end
			end
		else
			if f.counter[reg].readback then
				f.counter[reg].readback = false
				f.counter[reg].status = action.data
				printk(f, action, "[%d] status = %02x\n", reg, f.counter[reg].status)
			end
			if f.counter[reg].latch then
				f.counter[reg].latch = false
				f.counter[reg].current = action.data
				printk(f, action, "[%d] current = %d\n", reg, f.counter[reg].current)
			end
		end
	elseif reg == 0x03 then
	end
	return true
end

i8254_counters = {}
i8254_counters[0x0] = { init=0, current=0, latch, readback, status=0 }
i8254_counters[0x1] = { init=0, current=0, latch, readback, status=0 }
i8254_counters[0x2] = { init=0, current=0, latch, readback, status=0 }

filter_i8254_a = {
	name = "i8254 A",
	pre = i8254_pre,
	post = i8254_post,
	base = 0x40,
	hide = hide_i8254_io,
	size = 4,
	counter = i8254_counters,
}
filter_i8254_b = {
	name = "i8254 B",
	pre = i8254_pre,
	post = i8254_post,
	base = 0x50,
	hide = hide_i8254_io,
	size = 4,
	counter = i8254_counters,
}


-- **********************************************************
--
-- i8042 KBD, A20, Reset(?)

function i8042_write(f, action)
	if action.addr == 0x60 then
		f.reg.data = action.data
		f.reg.sts = (f.reg.sts & 0xf7)
		if (f.reg.cmd == 0xd1) then
			f.reg.A20 = ((0x02 & action.data) == 0x02)
		end
		return handle_action(f, action)
	end
	if action.addr == 0x64 then
		f.reg.cmd = action.data
		f.reg.sts = (f.reg.sts | 0x0a)
		return handle_action(f, action)
	end
	return skip_filter(f, action)
end

function i8042_read(f, action)
	if action.addr == 0x60 then
		f.reg.sts = (f.reg.sts & 0xfe)
		return handle_action(f, action)
	end
	if action.addr == 0x64 then
		return handle_action(f, action)
	end
	return skip_filter(f, action)
end

function i8042_pre(f, action)
	if (action.write) then
		return i8042_write(f, action)
	else
		return i8042_read(f, action)
	end
end

function i8042_post(f, action)
	if action.addr == 0x60 then
		if action.write and f.reg.cmd == 0xd1 then
			if f.reg.A20 then
				printk(f, action, "A20 enabled\n")
			else
				printk(f, action, "A20 disabled\n")
			end
		end
		return true
	end
	if action.addr == 0x64 then
		return true
	end
	return false
end

filter_i8042 = {
	decode = F_FIXED,
	name = "i8042",
	pre = i8042_pre,
	post = i8042_post,
	hide = hide_i8042_io,
	base = 0x60,
	size = 0x5,
	reg = { data = 0, sts = 0, cmd = 0, A20 = 0 }
}


-- **********************************************************
--
-- CMOS nvram


function nvram_bank(addr)
	if (0xfe & addr) == 0x70 then
		return 1
	elseif (0xfe & addr) == 0x72 then
		return 2
	elseif (0xfe & addr) == 0x74 then
		return 2
	else
		return 0
	end
end

function nvram_write(f, action)
	local val = action.data
	local rtc = false
	local is_index = ((0x01 & action.addr) == 0x0)
	local bank = nvram_bank(action.addr)

	if bank == 1 then
		if is_index then
			f.reg.p70 = (0x7f & val)
			if f.reg.p70 < 0x0E then
				rtc = true
			end
		else
			f.nvram_data[f.reg.p70] = val
			f.nvram_set[f.reg.p70] = true
			if f.reg.p70 < 0x0E then
				rtc = true
			end
		end
	elseif bank == 2 then
		if is_index then
			f.reg.p72 = (0x7f & val)
		else
			local index = 0x80 + f.reg.p72
			f.nvram_data[index] = val
			f.nvram_set[index] = true
		end
	end
	if cache_nvram and not rtc then
		return fake_action(f, action, val)
	else
		return handle_action(f, action)
	end
end

function nvram_read(f, action)
	local val = 0
	local rtc = false
	local is_index = ((0x01 & action.addr) == 0x0)
	local bank = nvram_bank(action.addr)

	if bank == 1 then
		if is_index then
			-- NMI returned as 0
			val = f.reg.p70
			if f.reg.p70 < 0x0E then
				rtc = true
			end
		else
			if f.reg.p70 < 0x0E then
				rtc = true
			elseif f.nvram_set[f.reg.p70] then
				val = f.nvram_data[f.reg.p70]
			end
		end
	else -- bank
		if is_index then
			-- NMI returned as 0
			val = f.reg.p72
		else
			local index = 0x80 + f.reg.p72
			if f.nvram_set[index] then
				val = f.nvram_data[index]
			end
		end
	end
	if cache_nvram and not rtc then
		return fake_action(f, action, val)
	else
		return handle_action(f, action)
	end
end

function nvram_pre(f, action)
	if (action.write) then
		return nvram_write(f, action)
	else
		return nvram_read(f, action)
	end
end

function nvram_post(f, action)
	if (0x01 & action.addr) == 0x0 then
		return true
	end

	local bank = nvram_bank(action.addr)
	if (action.write) then
		if bank == 1 then
			printk(f, action, "[%02x] <= %02x\n", f.reg.p70, action.data)
		elseif bank == 2 then
			printk(f, action, "[%02x] <= %02x\n", 0x80 + f.reg.p72, action.data)
		end
	else
		if bank == 1 then
			printk(f, action, "[%02x] => %02x\n", f.reg.p70, action.data)
		elseif bank == 2 then
			printk(f, action, "[%02x] => %02x\n", 0x80 + f.reg.p72, action.data)
		end
	end
	return true
end

filter_nvram = {
	name = "NVram",
	pre = nvram_pre,
	post = nvram_post,
	base = 0x70,
	size = 8,
	hide = hide_nvram_io,
	reg = { p70 = 0, p72 = 0 },
	nvram_data = {},
	nvram_set = {},
}


-- **********************************************************
--
-- Reset at 0xcf9

function sys_rst_pre(f, action)
	if action.size == 1 then
		if action.write and (action.data & 0x04) == 0x04 then
			SerialICE_system_reset()
		end
		return handle_action(f, action)
	end
	return skip_filter(f, action)
end

function sys_rst_post(f, action)
	if action.size == 1 then
		if action.write then
			printk(f, action, "Control = %02x\n", action.data)
			return true
		end
	end
	return false
end

filter_reset = {
	name = "Reset",
	pre = sys_rst_pre,
	post = sys_rst_post,
	hide = false,
	base = 0xcf9,
	size = 1
}

-- **********************************************************
--
-- VGA io

function vga_io_pre(f, action)
	return skip_filter(f, action)
end

function vga_io_post(f, action)
	return true
end

filter_vga_io = {
	name = "VGA",
	pre = vga_io_pre,
	post = vga_io_post,
	hide = false,
	base = 0x3c0,
	size = 0x20,
}


-- **********************************************************
--
-- Enable all PC80 stuff

function enable_hook_pc80()
	enable_hook(io_hooks, filter_i8237_a)
	enable_hook(io_hooks, filter_i8237_b)
	enable_hook(io_hooks, filter_i8237_c)
	enable_hook(io_hooks, filter_i8259_master)
	enable_hook(io_hooks, filter_i8259_slave)
	enable_hook(io_hooks, filter_i8259_edge)
	enable_hook(io_hooks, filter_i8042)
	enable_hook(io_hooks, filter_i8254_a)
	enable_hook(io_hooks, filter_i8254_b)
	enable_hook(io_hooks, filter_reset)
	enable_hook(io_hooks, filter_nvram)
	enable_hook(io_hooks, filter_vga_io)
	enable_hook(io_hooks, filter_debugport)
end

