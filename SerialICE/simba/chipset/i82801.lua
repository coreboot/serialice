-- **********************************************************
--
-- SMBus controller handling


load_filter("intel_smbus")

function smbus_bar_hook(f, action)
	intel_smbus_setup(action.data, 0x20)
end

dev_sb_lpc = {
	pci_dev = pci_bdf(0x0,0x1f,0x3,0x0),
	name = "Smbus",
	bar = {},
}

function enable_smbus_host_bar()
	pci_cfg16_hook(dev_sb_lpc, 0x20, "SMBus", smbus_bar_hook)
end


-- **********************************************************
--

dev_power = {
	pci_dev = pci_bdf(0x0,0x1f,0x0,0x0),
	name = "SYS",
	bar = {},
	acpi = { f = nil },
	tco = { f = nil },
}

function pm_io_bar(f, action)
	f.dev.acpi.name = "ACPI"
	f.dev.acpi.val = action.data
	f.dev.acpi.size = 0x60
	generic_io_bar(f.dev.acpi)

	f.dev.tco.name = "TCO"
	f.dev.tco.val = action.data + 0x60
	f.dev.tco.size = 0x20
	generic_io_bar(f.dev.tco)
end

function lpc_io_bar(f, action)
	local base = (action.data & 0xffff)
	local mask = (action.data >> 16) | 3
	local size = mask + 1

	base = (base & ~mask)

	add_bar(f.dev, f.reg, "LPC", size)
	f.dev.bar[f.reg].val = base
	generic_io_bar(f.dev.bar[f.reg])
end

function lpc_protect_serial_port(f, action)
	drop_action(f, action, 0)
end

-- **********************************************************
--
-- AC '97 controller handling


dev_audio = {
	pci_dev = pci_bdf(0x0,0x1f,0x5,0x0),
	name = "Audio",
	bar = {}
}

function enable_audio_bars()
	add_io_bar(dev_audio, 0x10, "NAMBAR", 0x100)
	add_io_bar(dev_audio, 0x14, "NABMBAR", 0x40)
	add_mem_bar(dev_audio, 0x18, "MMBAR", 0x200)
	add_mem_bar(dev_audio, 0x1C, "MBBAR", 0x100)
end

dev_modem = {
	pci_dev = pci_bdf(0x0,0x1f,0x6,0x0),
	name = "Modem",
	bar = {}
}

function enable_modem_bars()
	add_io_bar(dev_modem, 0x10, "MMBAR", 0x100)
	add_io_bar(dev_modem, 0x14, "MBAR", 0x80)
end


-- **********************************************************
--
-- i82801dx

function enable_dx_power_bars()
	pci_cfg16_hook(dev_power, 0x40, "PM", pm_io_bar)
	add_io_bar(dev_power, 0x58, "GPIO", 0x40)
end

function enable_dx_lpc_bars()
	pci_cfg8_hook(dev_power, 0xe0, "LPC", lpc_protect_serial_port)
	pci_cfg8_hook(dev_power, 0xe6, "LPC", lpc_protect_serial_port)

	add_io_bar(dev_power, 0xe4, "LPC1", 0x80)
	add_io_bar(dev_power, 0xec, "LPC2", 0x10)
end

function enable_hook_i82801dx()
	enable_smbus_host_bar()
	enable_dx_power_bars()
	enable_dx_lpc_bars()
	enable_audio_bars()
	enable_modem_bars()
end

-- **********************************************************
--
-- i82801fx

function enable_fx_power_bars()
	pci_cfg16_hook(dev_power, 0x40, "PM", pm_io_bar)
	add_io_bar(dev_power, 0x48, "GPIO", 0x40)
end

function enable_fx_lpc_bars()
	add_mem_bar(dev_power, 0xf0, "RCBA", 0x4000)

	pci_cfg8_hook(dev_power, 0x80, "LPC", lpc_protect_serial_port)
	pci_cfg8_hook(dev_power, 0x82, "LPC", lpc_protect_serial_port)

	add_io_bar(dev_power, 0x84, "LPC1", 0x80)
	add_io_bar(dev_power, 0x88, "LPC2", 0x40)
end

function enable_hook_i82801fx()
	enable_smbus_host_bar()
	enable_fx_power_bars()
	enable_fx_lpc_bars()
end

-- **********************************************************
--
-- i82801gx

-- ICH7 TPM
-- Phoenix "Secure" Core bails out if we don't pass the read on ;-)
filter_ich7_tpm = {
	name = "ICH7 TPM",
	pre = mem_target_only,
	post = mem_post,
	decode = F_RANGE,
	base = 0xfed40000,
	size = 0x00001000,
	hide = true
}

function enable_gx_power_bars()
	pci_cfg16_hook(dev_power, 0x40, "PM", pm_io_bar)
	add_io_bar(dev_power, 0x48, "GPIO", 0x40)
end

function enable_gx_lpc_bars()
	pci_cfg8_hook(dev_power, 0x80, "LPC", lpc_protect_serial_port)
	pci_cfg8_hook(dev_power, 0x82, "LPC", lpc_protect_serial_port)

	pci_cfg32_hook(dev_power, 0x84, "LPC", lpc_io_bar)
	pci_cfg32_hook(dev_power, 0x88, "LPC", lpc_io_bar)
	pci_cfg32_hook(dev_power, 0x8c, "LPC", lpc_io_bar)
	pci_cfg32_hook(dev_power, 0x90, "LPC", lpc_io_bar)
end

function enable_hook_i82801gx()
	enable_hook(mem_hooks, filter_ich7_tpm)
	add_mem_bar(dev_power, 0xf0, "RCBA", 0x4000)
	enable_smbus_host_bar()
	enable_gx_power_bars()
	enable_gx_lpc_bars()
end
