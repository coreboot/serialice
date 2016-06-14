-- **********************************************************
-- Intel i845

dev_i845_mch2 = {
	pci_dev = pci_bdf(0x0,0x6,0x0,0x0),
	name = "MCH",
	bar = {}
}

function northbridge_i845()
	add_mem_bar(dev_i845_mch2, 0x10, "RCOMP", 0x1000)
end

-- **********************************************************
-- Intel e7505

dev_e7505_mch = {
	pci_dev = pci_bdf(0x0,0x0,0x0,0x0),
	name = "MCH",
	bar = {}
}

function northbridge_e7505()
	add_mem_bar(dev_e7505_mch, 0x14, "RCOMP", 0x1000)
end


-- **********************************************************
-- Intel 82915 PCIe BAR

dev_i915 = {
	pci_dev = pci_bdf(0,0,0,0),
	name = "i915",
	bar = {},
}

function i915_pcie_bar(f, action)
	local baseaddr = (action.data & 0xf0000000)
	local size = 256*1024*1024

	-- enable is 0:00.0 [054] .31
	if true then
		pcie_mm_enable(f.dev, f.reg, baseaddr, size)
	else
		pcie_mm_disable(f.dev, f.reg, baseaddr, size)
	end
end

function northbridge_i915()
	add_mem_bar(dev_i915, 0x40, "EPBAR", 4*1024)
	add_mem_bar(dev_i915, 0x44, "MCHBAR", 16*1024)
	add_mem_bar(dev_i915, 0x4c, "DMIBAR", 4*1024)
	pci_cfg32_hook(dev_i915, 0x48, "PCI", i915_pcie_bar)
end

-- **********************************************************
-- Intel 82945 PCIe BAR

function i945_pcie_bar(f, action)
	local base = action.data
	local sizebits = (base >> 1) & 0x3
	local baseaddr = 0
	local size = 0

	if sizebits == 0 then
		size = 256*1024*1024
		baseaddr = (base & 0xf0000000)
	elseif sizebits == 1 then
		size = 128*1024*1024
		baseaddr = (base & 0xf8000000)
	elseif sizebits == 2 then
		size = 64*1024*1024
		baseaddr = (base & 0xfc000000)
	else
		-- undefined, really
		baseaddr = (base & 0xfe000000)
		size = 32*1024*1024
	end

	if (base & 1) ~= 0 then
		pcie_mm_enable(f.dev, f.reg, baseaddr, size)
	else
		pcie_mm_disable(f.dev, f.reg, baseaddr, size)
	end
end

dev_i945 = {
	pci_dev = pci_bdf(0,0,0,0),
	name = "i945",
	bar = {},
}

function northbridge_i945()
	add_mem_bar(dev_i945, 0x40, "EPBAR", 4*1024)
	add_mem_bar(dev_i945, 0x44, "MCHBAR", 16*1024)
	add_mem_bar(dev_i945, 0x4c, "DMIBAR", 4*1024)
	add_mem_bar(dev_i945, 0x60, "(unknown)", 4*1024)

	pci_cfg32_hook(dev_i945, 0x48, "PCI", i945_pcie_bar)
end

function northbridge_i946()
	add_mem_bar(dev_i945, 0x40, "PXPEPBAR", 4*1024)
	add_mem_bar(dev_i945, 0x48, "MCHBAR", 16*1024)
	add_mem_bar(dev_i945, 0x68, "DMIBAR", 4*1024)

	pci_cfg32_hook(dev_i945, 0x60, "PCI", i945_pcie_bar)
end
