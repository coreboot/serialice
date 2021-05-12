-- SPDX-License-Identifier: GPL-2.0-or-later

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
	name = "Supermicro X11SSM-F IO",
	pre  = mainboard_io_pre,
	hide = hide_mainboard_io,
	base = 0x0,
	size = 0x10000
}

function do_mainboard_setup()
	do_default_setup()

	enable_hook(cpumsr_hooks, filter_mtrr)
	enable_hook(cpumsr_hooks, filter_intel_microcode)
	enable_hook(cpuid_hooks, filter_multiprocessor)
	enable_hook(cpuid_hooks, filter_feature_smx)

	new_car_region(0xfef00000, 0x40000)
end
