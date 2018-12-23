filter_mainboard = {
	name = "P5QC",
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
	enable_hook_superio(0x2e, 0x07)
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
	new_car_region(0xfffe0000, 0x2000) -- VENDOR settings
	-- new_car_region(0xfeffc000, 0x4000) -- coreboot settings

	-- Apply mainboard hooks last, so they are the first ones to check
	enable_hook(io_hooks, filter_mainboard)
end
