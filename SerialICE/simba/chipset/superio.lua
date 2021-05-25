

function pnp_switch_ldn(f, data)
	if not f.ldn[data] then
		f.ldn[data] = { data = {}, set = {}, bar0 = {}, bar1 = {} }
	end
	f.pnp.active_ldn = data
end

function pnp_select_cfg(f, data)
	f.pnp.reg = data
end

function pnp_store_cfg(f, data)
	local reg = f.pnp.reg
	if reg < 0x30 then
		f.chip.data[reg] = data;
		f.chip.set[reg] = true;
	else
		local ldn = f.pnp.active_ldn
		f.ldn[ldn].data[reg] = data;
		f.ldn[ldn].set[reg] = true;
	end
end


-- **********************************************************
--
-- SuperIO device handling

function superio_pnpdev(f)
	return string.format("%s %02x:%02x", f.name, f.base, f.pnp.active_ldn)
end

function superio_string(f)
	if f.pnp.reg < 0x30 then
		return string.format("%02x: ", f.base)
	else
		return string.format("%02x:%02x ", f.base, f.pnp.active_ldn)
	end
end

function superio_register_post(f, action)
	local str = superio_string(f)
	if action.write then
		printk(f, action, "%s [%02x] <= %02x\n", str, f.pnp.reg, action.data)
	else
		printk(f, action, "%s [%02x] => %02x\n", str, f.pnp.reg, action.data)
	end
end

function superio_string_post(f, action, str2)
	printk(f, action, "%s %s\n", superio_string(f), str2)
end

function superio_try_enable_io(f, idx)

	local ldn = f.ldn[f.pnp.active_ldn]

	if ldn.set[0x30] and ldn.data[0x30] ~= 0x0 then
		if idx == 0 and ldn.set[0x60] and ldn.set[0x61] then
			local iobase = (ldn.data[0x60] << 8) | ldn.data[0x61]
			if not ldn.bar0.size then
				ldn.bar0.size = 1
			end
			if not ldn.bar0.name then
				ldn.bar0.name = superio_pnpdev(f)
			end
			if iobase ~= 0x60 then
				ldn.bar0.val = iobase
				generic_io_bar(ldn.bar0)
				ldn.bar0.f.decode = F_FIXED
			end
		end
		if idx == 1 and ldn.set[0x62] and ldn.set[0x63] then
			local iobase = (ldn.data[0x62] << 8) | ldn.data[0x63]
			if not ldn.bar1.size then
				ldn.bar1.size = 1
			end
			if not ldn.bar1.name then
				ldn.bar1.name = superio_pnpdev(f)
			end
			if iobase ~= 0x64 then
				ldn.bar1.val = iobase
				generic_io_bar(ldn.bar1)
				ldn.bar1.f.decode = F_FIXED
			end
		end
	end
end

function superio_try_enable_ldn(f, action)
	local ldn = f.ldn[f.pnp.active_ldn]

	if ldn.set[0x30] and ldn.data[0x30] == 0x0 then
		superio_string_post(f, action, "disabled")
	else
		superio_string_post(f, action, "enabled")
	end
end

function superio_pre(f, action)
	if not action.write then
		handle_action(f, action)
		return true
	end

	if action.addr == f.base then
		pnp_select_cfg(f, action.data)
		handle_action(f, action)
		return true
	end

	if action.addr == f.base + 0x01 then
		-- Also creates new LDN instance, if necessary.
		if f.pnp.reg == f.pnp.ldn_register then
			pnp_switch_ldn(f, action.data)
		end

		pnp_store_cfg(f, action.data)

		-- Don't allow that our SIO power gets disabled.
		if f.pnp.reg == 0x02 then
			drop_action(f, action, 0)
			return true
		end

		-- Don't mess with oscillator setup.
		if f.pnp.reg == 0x24 then
			drop_action(f, action, 0)
			return true
		end
		handle_action(f, action)
		return true
	end

	-- should not reach here
	return false
end

function superio_post(f, action)

	-- Do not log change of register or LDN.
	if action.addr == f.base or f.pnp.reg == f.pnp.ldn_register then
		return true
	end

	if not action.write then
		superio_register_post(f, action)
		return true
	end

	local ldn = f.ldn[f.pnp.active_ldn]

	-- Log base address once both bytes are set.

	if ( f.pnp.reg == 0x60 or f.pnp.reg == 0x61 ) then
		superio_try_enable_io(f, 0)
		return true
	end

	if (  f.pnp.reg == 0x62 or f.pnp.reg == 0x63 ) then
		superio_try_enable_io(f, 1)
		return true
	end

	if f.pnp.reg == 0x30 then
		superio_try_enable_io(f, 0)
		superio_try_enable_io(f, 1)
		superio_try_enable_ldn(f, action)
		return true
	end

	if f.pnp.reg == 0x70 then
		superio_string_post(f, action, string.format("irq = %d", ldn.data[0x70]))
		return true
	end
	if f.pnp.reg == 0x72 then
		superio_string_post(f, action, string.format("irq2 = %d", ldn.data[0x72]))
		return true
	end

	superio_register_post(f, action)
	return true
end


filter_superio_2e = {
	name = "PnP",
	pre = superio_pre,
	post = superio_post,
	base = 0x2e,
	size = 0x02,
	hide = hide_superio_cfg,
	chip = { data = {}, set = {} },
	pnp = { reg = 0, active_ldn = -1, ldn_register = 0 },
	ldn = {},
}
filter_superio_4e = {
	name = "PnP",
	pre = superio_pre,
	post = superio_post,
	base = 0x4e,
	size = 0x02,
	hide = hide_superio_cfg,
	chip = { data = {}, set = {} },
	pnp = { reg = 0, active_ldn = -1, ldn_register = 0 },
	ldn = {},
}


function superio_get_filter(cfg_base)
	if cfg_base == 0x2e then
		return filter_superio_2e
	elseif cfg_base == 0x4e then
		return filter_superio_4e
	else
		return nil
	end
end

function superio_set_ldn_register(f, ldn_register)
	f.pnp.ldn_register = ldn_register
end

function superio_new_ldn(f, idx)
	if not f.ldn[idx] then
		f.ldn[idx] = { data = {}, set = {}, bar0 = {}, bar1 = {} }
	end
end

function superio_ldn_iobase0(f, idx, name, size)
	f.ldn[idx].bar0.name = name
	f.ldn[idx].bar0.size = size
end

function superio_ldn_iobase1(f, idx, name, size)
	f.ldn[idx].bar1.name = name
	f.ldn[idx].bar1.size = size
end

function enable_hook_superio(base, ldn_register)
	local sio = superio_get_filter(base)
	superio_set_ldn_register(sio, ldn_register)
	enable_hook(io_hooks, sio)
end

-- **********************************************************
--
-- Serial Port handling

function com_pre(f, action)
	if (action.write) then
		drop_action(f, action, action.data)
		return true
	else
		drop_action(f, action, 0xff)
		return true
	end
end

filter_com1 = {
	name = "COM1",
	pre = com_pre,
	post = io_post,
	base = 0x3f8,
	size = 8,
	hide = hide_com,
}

filter_com2 = {
	name = "COM2",
	pre = com_pre,
	post = io_post,
	base = 0x2f8,
	size = 8,
	hide = hide_com,
}

filter_com3 = {
	name = "COM3",
	pre = com_pre,
	post = io_post,
	base = 0x3e8,
	size = 8,
	hide = hide_com,
}

filter_com4 = {
	name = "COM4",
	pre = com_pre,
	post = io_post,
	base = 0x2e8,
	size = 8,
	hide = hide_com,
}
