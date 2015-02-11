-- SerialICE
--
-- Copyright (c) 2012 Kyösti Mälkki <kyosti.malkki@gmail.com>
-- Copyright (c) 2013 Alexandru Gagniuc <mr.nuke.me@gmail.com>
--
-- Permission is hereby granted, free of charge, to any person obtaining a copy
-- of this software and associated documentation files (the "Software"), to deal
-- in the Software without restriction, including without limitation the rights
-- to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
-- copies of the Software, and to permit persons to whom the Software is
-- furnished to do so, subject to the following conditions:
--
-- The above copyright notice and this permission notice shall be included in
-- all copies or substantial portions of the Software.
--
-- THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
-- IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
-- FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
-- THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
-- LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
-- OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
-- THE SOFTWARE.
--


-- **********************************************************
--

load_filter("intel_smbus")

dev_nb_traf_ctl = {
	pci_dev = pci_bdf(0,0,5,0),
	name = "trf",
	bar = {},
}

function vx900_pcie_bar(f, action)
	local baseaddr = (action.data << 28)
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
	acpi = { f = nil },
}

function sb_mmio_bar(f, action)
	-- This MMIO space is used for SPI and CEC control
	f.dev.mmio.name = "SB_MMIO"
	f.dev.mmio.val = (action.data & 0xfff0) << 8
	f.dev.mmio.size = 0x10000

	generic_mmio_bar(f.dev.mmio)
end

function pm_io_bar(f, action)
	f.dev.acpi.name = "ACPI"
	f.dev.acpi.val = (action.data & 0xff80)
	f.dev.acpi.size = 0x80
	generic_io_bar(f.dev.acpi)
end

function smbus_bar_hook(f, action)
	local base = (action.data & 0xfff0)
	intel_smbus_setup(base, 0x10)
end

function enable_hooks_vx900()
	pci_cfg8_hook(dev_nb_traf_ctl, 0x61, "PCI", vx900_pcie_bar)
	pci_cfg32_hook(dev_sb, 0xbc, "SB_MMIO", sb_mmio_bar)
	pci_cfg16_hook(dev_sb, 0x88, "PM", pm_io_bar)
	pci_cfg16_hook(dev_sb, 0xd0, "SMBus", smbus_bar_hook)
end
