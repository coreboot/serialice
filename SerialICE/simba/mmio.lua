
-- **********************************************************
--
-- Vendor independent X86 memory mapped IO

-- Local APIC
-- We should avoid that someone wakes up cores
-- on the target system that go wild.
function mem_lapic(f, action)
	if bit32.band(action.addr, f.size-1) == 0x300 then
		-- replace Start-Up IPI with Init IPI
		if action.write and bit32.band(action.data, 0xf0f00) == 0xc0600 then
			return fake_action(f, action, 0xc0500)
		end
	end
	return handle_action(f, action)
end

filter_lapic = {
	id = -1,
	name = "LAPIC",
	pre = mem_lapic,
	post = mem_base_post,
	hide = true,
	base = 0xfee00000,
	size = 0x00010000,
}

-- IOAPIC
filter_ioapic = {
	id = -1,
	name = "IOAPIC",
	pre = mem_target_only,
	post = mem_base_post,
	hide = true,
	base = 0xfec00000,
	size = 0x00010000,
}


