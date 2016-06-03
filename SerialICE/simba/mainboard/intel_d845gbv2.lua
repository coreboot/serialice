

function mainboard_io_read(f, action)
	-- Some timer loop
	if ( action.addr == 0x61 ) then
	end

	-- IO slowdown
	if action.addr == 0xed then
		ignore_action(f, action)
		drop_action(f, action, 0)
		return true
	end

	if action.addr == 0xcfb then
		ignore_action(f, action)
		drop_action(f, action, 0)
		return true
	end

	return false
end


function mainboard_io_write(f, action)

	-- Catch RAM controller ready.
	if action.addr == 0x80 and action.data == 0xd5 and not ram_enabled() then
	--	enable_ram()
	end

	if action.addr == 0xcfb then
		ignore_action(f, action)
		drop_action(f, action, 0)
		return true
	end

	if action.addr == 0xe1 then
		ignore_action(f, action)
		drop_action(f, action, action.data)
		return true
	end

	return false
end

function mainboard_io_pre(f, action)
	if action.write then
		return mainboard_io_write(f, action)
	else
		return mainboard_io_read(f, action)
	end
end

function mainboard_io_post(f, action)
	if action.addr == 0xe1 or action.addr == 0xed or action.addr == 0xcfb then
		return true
	end
	if action.addr == 0x80 and not action.write then
		return true
	end
end

filter_mainboard = {
	name = "GEBV2",
	pre = mainboard_io_pre,
	post = mainboard_io_post,
	hide = hide_mainboard_io,
	base = 0x0,
	size = 0x10000
}

load_filter("i82801")

function do_mainboard_setup()
	new_car_region(0xfef00000, 0x800)

	enable_hook(io_hooks, filter_pci_io_cfg)
	enable_hook(mem_hooks, filter_lapic)
	enable_hook(mem_hooks, filter_ioapic)

	enable_hook(cpumsr_hooks, filter_intel_microcode)
	enable_hook(cpuid_hooks, filter_multiprocessor)

	-- I have a hook to detect RAM initialisation from
	-- a POST code I can skip this here
	enable_ram()

	enable_hook_pc80()
	enable_hook_superio(0x2e, 0x07)
	--enable_hook(io_hooks, filter_com1)
	enable_hook_i82801dx()

	-- Apply mainboard hooks last, so they are the first ones to check
	enable_hook(io_hooks, filter_mainboard)
end
