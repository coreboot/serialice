
function sb_mmio_bar(f, action)
	-- This MMIO space is used for SPI and CEC control
	f.dev.mmio.name = "SB_MMIO"
	f.dev.mmio.val = bit32.lshift(bit32.band(action.data, 0xfff0), 8)
	f.dev.mmio.size = 0x10000

	generic_mmio_bar(f.dev.mmio)
end

dev_sb = {
	pci_dev = pci_bdf(0,0x11,0,0),
	name = "sb",
	bar = {},
	mmio = { f = nil },
}

function nb_pcie_bar(dev, reg, base)
	local size = 64*1024

	pcie_mm_cfg_bar(base, size)
end

dev_nb = {
	pci_dev = pci_bdf(0,0,0,0),
	name = "nb",
	bar = {},
}

function northbridge_vx900()
	pci_cfg32_hook(dev_sb, 0xbc, "SB_MMIO", sb_mmio_bar)
	pci_cfg32_hook(dev_nb, 0x0, "NB_PCI", nb_pcie_bar)
end
