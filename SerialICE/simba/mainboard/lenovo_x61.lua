-- For X61, chop off the bottom 2M: dd if=x61 bs=2048k skip=1 of=x61.2m
-- Dies shortly after raminit

function mainboard_io_pre(f, action)
	-- src/mainboard/lenovo/x60/dock.c dock_connect():
	-- Firmware attempts toggling the D_PLTRST GPIO pin,
	-- disconnecting the dock we're connecting through
	if action.write and action.addr == 0x1680 then
		ignore_action(f, action)
		drop_action(f, action, 0)
		return true
	end
end

function mainboard_io_post(f, action)
	if action.write and action.addr == 0x1680 then
		return true
	end
end

filter_mainboard = {
	name = "X61",
	pre = mainboard_io_pre,
	post = mainboard_io_post,
	hide = hide_mainboard_io,
	base = 0x0,
	size = 0x10000
}

load_filter("i82801")
load_filter("intel_bars")

function do_mainboard_setup()
	do_default_setup()

	enable_hook_pc80()
	-- Reasonably similar to ICH8/i82801hx
	enable_hook_i82801gx()

        enable_hook(io_hooks, filter_pci_io_cfg)
        enable_hook(mem_hooks, filter_lapic)
        enable_hook(mem_hooks, filter_ioapic)

	enable_hook(cpumsr_hooks, filter_intel_microcode)
	enable_hook(cpuid_hooks, filter_multiprocessor)

	-- i965 happens to have 64-bit BARs too, on the same addresses
	northbridge_i946()

	enable_ram()
	new_car_region(0xffde0000, 0x2000)

        -- Apply mainboard hooks last, so they are the first ones to check
	enable_hook(io_hooks, filter_mainboard)
end
