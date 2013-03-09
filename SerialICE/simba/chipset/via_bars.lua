
function sb_pcie_bar(dev, reg, base)
	local baseaddr = bit32.lshift(base, 16)
	local size = 64*1024

	pcie_mm_cfg_bar(baseaddr, size)
end

dev_sb = {
	pci_dev = pci_bdf(0,0x11,0,0),
	name = "sb",
	bar = {},
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
	pci_cfg16_hook(dev_sb, 0xbd, "SB_PCI", sb_pcie_bar)
	pci_cfg32_hook(dev_nb, 0x0, "NB_PCI", nb_pcie_bar)
end
