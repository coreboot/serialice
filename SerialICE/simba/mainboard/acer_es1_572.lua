-- SPDX-License-Identifier: GPL-2.0-or-later

function uart_pre(f, action)
	if action.write then
		-- block writes to UART BAR to protect UART
		if (action.addr & ~0xff) == 0xfe034000 then
			drop_action(f, action, 0)
			return true
		end
		-- block SERIAL_IO_GPPRVRW7 to protect UART
		if action.addr == 0xfdcb0618 then
			drop_action(f, action, 0)
			return true
		end
		-- protect the UART PCI device
		if (action.addr & 0xffffff00) == 0xe00c8000 then
			drop_action(f, action, 0)
			return true
		end
		-- protect UART PSF3 registers
		if (action.addr & 0xffffff00) == 0xfdbc0f00 then
			drop_action(f, action, 0)
			return true
		end
	end
	return false
end

filter_uart = {
	name = "Acer ES1-572 UART",
	pre =  uart_pre,
	hide = hide_mainboard_mem,
	base = 0,
	size = 0x100000000,
}

function mainboard_io_pre(f, action)
	-- Check if the last 16-bit POST code tells us the RAM controller is ready
	if action.write and action.addr == 0x80 and action.data == 0xdd5d then
		if not ram_enabled() then
			enable_ram()
		end
	end
	return false
end

filter_mainboard_io = {
	name = "Acer ES1-572 IO",
	pre  = mainboard_io_pre,
	hide = hide_mainboard_io,
	base = 0x0,
	size = 0x10000
}

fake_readback = false
fake_addr = -1
fake_data = -1

function ec_mem_pre(f, action)
	-- The ec clears bit 2 of the written value to d82.
	-- Since SerialICE is too slow, the firmware fails comparing
	-- the written value and the current one. That leads to a
	-- infinite loop. Save the written value and fake it on the
	-- subsequent read to circumvent that.
	if action.write and action.addr == 0xff000d82 then
		-- save written data
		fake_readback = true
		fake_addr = action.addr
		fake_data = action.data
	end
	-- send both reads and writes to hw
	handle_action(f, action)
	return true
end

function ec_mem_post(f, action)
	if not action.write and fake_readback and fake_addr == action.addr then
		-- fake read data
		fake_action(f, action, fake_data)
		fake_readback = false
		return true
	end
	return false
end

filter_ec_mem = {
	name = "Acer ES1-572 EC MEM",
	pre =  ec_mem_pre,
	post = ec_mem_post,
	hide = hide_mainboard_mem,
	base = 0xff000000,
	size = 0x1000,
}

function do_mainboard_setup()
	do_default_setup()

	-- Disable SIO hooks because they will fail.
	disable_hook(filter_superio_2e)
	disable_hook(filter_superio_4e)

	enable_hook(cpumsr_hooks, filter_mtrr)
	enable_hook(cpumsr_hooks, filter_intel_microcode)
	enable_hook(cpuid_hooks, filter_multiprocessor)
	enable_hook(cpuid_hooks, filter_feature_smx)

	-- Acer ES1-572 firmware
	new_car_region(0xfef00000, 0x40000)
	-- Acer EX2540 firmware (same board)
	--new_car_region(0xfef00000, 0x80000)

	-- Apply mainboard hooks last, so they are the first ones to check
	enable_hook(mem_hooks, filter_uart)
	enable_hook(mem_hooks, filter_ec_mem)
	enable_hook(io_hooks, filter_mainboard_io)
end
