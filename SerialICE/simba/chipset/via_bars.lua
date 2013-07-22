
dev_nb_traf_ctl = {
	pci_dev = pci_bdf(0,0,5,0),
	name = "trf",
	bar = {},
}

function vx900_pcie_bar(f, action)
	local baseaddr = bit32.lshift(action.data, 28)
	local size = 256*1024*1024

	-- enable is 0:00.5 [060] .10
	if baseaddr then
		pcie_mm_enable(f.dev, f.reg, baseaddr, size)
	else
		pcie_mm_disable(f.dev, f.reg, baseaddr, size)
	end
end

dev_sb = {
	pci_dev = pci_bdf(0,0x11,0,0),
	name = "sb",
	bar = {},
	mmio = { f = nil },
}

function sb_mmio_bar(f, action)
	-- This MMIO space is used for SPI and CEC control
	f.dev.mmio.name = "SB_MMIO"
	f.dev.mmio.val = bit32.lshift(bit32.band(action.data, 0xfff0), 8)
	f.dev.mmio.size = 0x10000

	generic_mmio_bar(f.dev.mmio)
end

function enable_hooks_vx900()
	pci_cfg8_hook(dev_nb_traf_ctl, 0x61, "PCI", vx900_pcie_bar)
	pci_cfg32_hook(dev_sb, 0xbc, "SB_MMIO", sb_mmio_bar)
end
