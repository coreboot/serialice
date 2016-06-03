
-- **********************************************************
--
-- PCI IO config access

pci_cfg_action = {}

PCI_CFG_READ = false
PCI_CFG_WRITE = true

pci_cfg_hooks = new_hooks("PCI")

function add_pci_cfg_hook(dev, reg, size, func)
	local bdfr = (dev.pci_dev | reg)
	local name = string.format("%x:%02x.%x [%03x]",
		(0xff & (bdfr >> 20)), (0x1f & (bdfr >> 15)),
		(0x7 & (bdfr >> 12)), (0xfff & bdfr))
	local filter = {
		base = bdfr,
		dev = dev,
		reg = reg,
		size = size,
		pre = func,
		name = name,
		enable = true
	}
	enable_hook(pci_cfg_hooks, filter)
end

function is_pci_cfg_hooked(bdf)
	local l = pci_cfg_hooks.list
	while l do
		if bdf == (l.hook.base & ~0x03)  then
			return true
		end
		l = l.next
	end
	return false
end

function walk_pci_pre_hooks(list, action)
	if list == nil or list.list == nil then
		return false
	end
	local l = list.list
	local f = nil

	while l do
		f = l.hook
		if action.addr == f.base and action.size == f.size then
			if f.enable and f.pre then
				f.pre(f, action)
			end
		end
		l = l.next
	end
end

function call_pci_cfg_hook(action, bdf, size, data)

	if action.stage == PRE_HOOK then
		if action.write then
			pre_action(pci_cfg_action, PCI_CFG_WRITE, bdf, size, data)
		else
			--pre_action(pci_cfg_action, PCI_CFG_READ, bdf, size, 0)
		end
		walk_pci_pre_hooks(pci_cfg_hooks, pci_cfg_action)

		if pci_cfg_action.dropped then
			action.dropped = true
		end
	elseif action.stage == POST_HOOK then

	end
end

function pci_cfg8_hook(dev, reg, name, hook)
	add_bar(dev, reg, name, 0)
	add_pci_cfg_hook(dev, reg, 1, hook)
end

function pci_cfg16_hook(dev, reg, name, hook)
	add_bar(dev, reg, name, 0)
	add_pci_cfg_hook(dev, reg, 2, hook)
end

function pci_cfg32_hook(dev, reg, name, hook)
	add_bar(dev, reg, name, 0)
	add_pci_cfg_hook(dev, reg, 4, hook)
end

function io_bar_hook(f, action)
	f.dev.bar[f.reg].val = action.data
	generic_io_bar(f.dev.bar[f.reg])
end

function mem_bar_hook(f, action)
	f.dev.bar[f.reg].val = action.data
	generic_mmio_bar(f.dev.bar[f.reg])
end

function add_bar(dev, reg, name, size)
	if not dev.bar then
		dev.bar = {}
	end
	if not dev.bar[reg] then
		dev.bar[reg] = {}
		dev.bar[reg].f = nil
	end
	dev.bar[reg].name = name
	dev.bar[reg].val = 0x0
	dev.bar[reg].size = size
end

function add_io_bar(dev, reg, name, size)
	add_bar(dev, reg, name, size)
	add_pci_cfg_hook(dev, reg, 2, io_bar_hook)
end

function add_mem_bar(dev, reg, name, size)
	add_bar(dev, reg, name, size)
	add_pci_cfg_hook(dev, reg, 4, mem_bar_hook)
end

function pci_bdf(bus, dev, func, reg)
	return bus*1048576 + dev*32768 + func*4096 + reg
end

-- Catch partial PCI configuration register writes.
-- This synthesizes 32/16 bit wide access from separate
-- 16/8 bit accesses for pci_cfg_hooks.

bv = {}

function pci_cfg_select(f, bdfr)
	f.reg.bdfr = bdfr

	if f.reg.bdfr_hook ~= bdfr then
		f.reg.bdfr_hook = 0
		if (is_pci_cfg_hooked(bdfr)) then
			f.reg.bdfr_hook = bdfr
		end
		for i = 0, 3, 1 do bv[i] = false end
	end
end

function pci_cfg_print(f, action, bdfr)
	local dir_str = "=>"
	if (action.write) then
		dir_str = "<="
	end

	printk(f, action, "%x:%02x.%x [%03x] %s %s\n",
		(0xff & (bdfr >> 20)), (0x1f & (bdfr >> 15)),
		(0x7 & (bdfr >> 12)), (0xfff & bdfr),
		dir_str, size_data(action.size, action.data))
end

function pci_cfg_access(f, action)

	if f.reg.bdfr_hook == 0 then
		return false
	end

	if not action.write then
		return false
	end

	local bdfr = f.reg.bdfr_hook
	local addr = action.addr
	local size = action.size
	local data = action.data

	if not f.reg.reset and ((f.reg.prev_addr ~= addr) or (f.reg.prev_size ~= size)) then
		new_parent_action()
	end
	f.reg.reset = false
	f.reg.prev_addr = addr
	f.reg.prev_size = size

	local av = {}

	for i = 0, 3, 1 do av[i] = false end

	ll = 8 * (addr%4)
	if (size == 1) then
		av[addr%4] = true
		bv[addr%4] = true
		amask = (0xff << ll)
		omask = (data << ll)
		f.reg.data = (f.reg.data & ~amask)
		f.reg.data = (f.reg.data | omask)
	elseif (size == 2) then
		av[addr%4] = true
		bv[addr%4] = true
		av[addr%4+1] = true
		bv[addr%4+1] = true
		amask = (0xffff << ll)
		omask = (data << ll)
		f.reg.data = (f.reg.data & ~amask)
		f.reg.data = (f.reg.data | omask)
	elseif (size == 4) then
		f.reg.data = data
		for i = 0, 3, 1 do av[i] = true end
		for i = 0, 3, 1 do bv[i] = true end
	end

	local val = 0
	val = f.reg.data
	for i = 0, 3, 1 do
		if (bv[i] and av[i]) then
			call_pci_cfg_hook(action, bdfr + i, 1, val)
		end
		val = (val >> 8)
	end
	val = f.reg.data
	for i = 0, 2, 1 do
		if ((bv[i] and bv[i+1]) and (av[i] or av[i+1])) then
			call_pci_cfg_hook(action, bdfr + i, 2, val)
		end
		val = (val >> 8)
	end
	val = f.reg.data
	if (bv[0] and bv[1] and bv[2] and bv[3]) then
		call_pci_cfg_hook(action, bdfr, 4, val)
	end
end

-- **********************************************************
--
-- PCI IO config access

function pci_io_cfg_pre(f, action)
	if action.addr == 0xcf8 and action.size == 4 then
		if action.write then
			if (0x80000000 & action.data) ~= 0 then
				f.reg.reset = true
				new_parent_action()
			end
			local bdfr = 0
			-- BDFR is like normal BDF but reg has 12 bits to cover all extended space
			-- Copy bus/device/function
			bdfr = (action.data << 4)
			bdfr = (bdfr & 0x0ffff000)
			-- Some chipsets allows (on request) performing extended register space access
			-- Usually using bits 27:24, copy that to right place
			bdfr = bdfr | (0xf00 & (action.data >> (24 - 8)))
			-- Add the classic PCI register
			bdfr = bdfr | (action.data & 0xfc)
			pci_cfg_select(f, bdfr)
		end
		handle_action(f, action)
		return true
	end
	if action.addr >= 0xcfc and action.addr <= 0xcff then
		pci_cfg_access(f, action)
		handle_action(f, action)
		return true
	end
	return false
end

function pci_io_cfg_post(f, action)
	if action.addr == 0xcf8 and action.size == 4 then
		return true
	end
	if action.addr >= 0xcfc and action.addr <= 0xcff then
		local byte_offset = action.addr - 0xcfc
		pci_cfg_print(f, action, f.reg.bdfr + byte_offset)
		return true
	end
	return false
end

filter_pci_io_cfg = {
	name = "PCI",
	pre = pci_io_cfg_pre,
	post = pci_io_cfg_post,
	hide = hide_pci_io_cfg,
	base = 0xcf8,
	size = 0x08,
	reg = { bdfr = 0, bdfr_hook = 0, data = 0,
		reset = true, prev_addr = 0, prev_size = 0 }
}

-- **********************************************************
--
-- PCIe MM config access

function pci_mm_cfg_pre(f, action)
	local bdfr = 0
	bdfr = (action.addr & ~f.base)
	bdfr = (bdfr & ~0x3)

	pci_cfg_select(f, bdfr)
	pci_cfg_access(f, action)

	handle_action(f, action)
	return true
end

function pci_mm_cfg_post(f, action)
	local bdfr = (action.addr & ~f.base)
	pci_cfg_print(f, action, bdfr)
	return true
end

filter_pci_mm_cfg = {
	pre = pci_mm_cfg_pre,
	post = pci_mm_cfg_post,
	hide = hide_pci_mm_cfg,
	reg = { bdfr = 0, bdfr_hook = 0, data = 0,
		reset = true, prev_addr = 0, prev_size = 0 }
}

function pcie_mm_enable(dev, reg, base, size)
	local bar = dev.bar[reg]
	bar.val = base
	bar.size = size

	if not bar.f then
		bar.f = filter_pci_mm_cfg
	end

	bar.f.name = bar.name
	bar.f.base = bar.val
	bar.f.size = bar.size
	enable_hook(mem_hooks, bar.f)
end

function pcie_mm_disable(dev, reg, base, size)
	if dev.bar and dev.bar[reg] and dev.bar[reg].f then
		disable_hook(dev.bar[reg].f)
	end
end
