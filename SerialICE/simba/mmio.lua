
-- **********************************************************
--
-- Vendor independent X86 memory mapped IO

-- Local APIC
-- We should avoid that someone wakes up cores
-- on the target system that go wild.
function mem_lapic(f, action)
	if (action.addr & f.size-1) == 0x300 then
		-- replace Start-Up IPI with Init IPI
		if action.write and (action.data & 0xf0f00) == 0xc0600 then
			fake_action(f, action, 0xc0500)
			return true
		end
	end
	handle_action(f, action)
	return true
end

filter_lapic = {
	name = "LAPIC",
	pre = mem_lapic,
	post = mem_post,
	decode = F_RANGE,
	hide = true,
	base = 0xfee00000,
	size = 0x00010000,
}

-- IOAPIC
filter_ioapic = {
	name = "IOAPIC",
	pre = mem_target_only,
	post = mem_post,
	decode = F_RANGE,
	hide = true,
	base = 0xfec00000,
	size = 0x00010000,
}
