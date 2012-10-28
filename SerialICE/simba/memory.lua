
-- **********************************************************
-- ROM access

function mem_qemu_rom_pre(f, action)
	action.to_hw = false
	action.to_qemu = true
	-- Reads from ROM space do not count for filter change.
	if not action.write then
		ignore_action(f, action)
	end
	return true
end

function mem_rom_post(f, action)
	if not action.write then
		return true
	end
	-- Writes to ROM space fall-thru to the fallback filter,
	-- so they get logged there.
	return false
end

filter_rom_low = {
	id = -1,
	name = "ROM_LO",
	pre = mem_qemu_rom_pre,
	post = mem_rom_post,
	hide = hide_rom_access,
	base = 0xE0000,
	size = 0x20000
}
filter_rom_high = {
	id = -1,
	name = "ROM_HI",
	pre = mem_qemu_rom_pre,
	post = mem_rom_post,
	hide = hide_rom_access,
	base = rom_base,
	size = rom_size,
}

-- **********************************************************
-- CAR access

function car_qemu_only(f, action)
	action.to_hw = false
	action.to_qemu = true
	return true
end

function car_post(f, action)
	return true
end

function new_car_region(start, size)
	f = {}
	f.id = -1
	f.name = "CAR"
	f.base = start
	f.size = size
	f.pre = car_qemu_only
	f.post = car_post
	f.hide = true
	enable_hook(mem_hooks, f)
	SerialICE_register_physical(start, size)
end


-- **********************************************************
-- RAM access.

-- In the beginning, during RAM initialization, it is essential that
-- all DRAM accesses are handled by the target, or RAM will not work
-- correctly. After RAM initialization, RAM access has no "special"
-- meaning anymore, so we can just use Qemu's memory (and thus get
-- an incredible speed-up)

-- Not catching the end of RAM init is not problematic, except
-- that it makes decompression of the BIOS core to RAM incredibly
-- slow as the decompressor inner loop has to be fetched
-- permanently from the target (several reads/writes per
-- decompressed byte).

ram_is_initialized = false

-- This is handled by SerialICE but *NOT* exclusively.
-- Writes end up in Qemu memory, too
function mem_ram_low(f, action)
	if ram_is_initialized then
		-- RAM init is done. Send all RAM accesses
		-- to Qemu. Using the target as storage would
		-- only slow execution down.
		action.to_hw = false
		action.to_qemu = true
	else
		-- RAM init has not been marked done yet.
		-- so send reads to the target only.
		action.to_hw = true
		action.to_qemu = action.write
	end
	return true
end

-- SMI/VGA writes go to target and qemu
-- SMI/VGA reads come from target
function mem_smi_vga(f, action)
	if action.write then
		action.to_hw = true
		action.to_qemu = true
	else
		action.to_hw = true
		action.to_qemu = false
	end
	return true
end


function mem_post_pre_ram_only(f, action)
	return ram_is_initialized
end

filter_ram_low = {
	id = -1,
	name = "MEM",
	pre = mem_ram_low,
	post = mem_post_pre_ram_only,
	hide = true,
	base = 0x0,
	size = 0xa0000
}

filter_smi_vga = {
	id = -1,
	name = "SMI_VGA",
	pre = mem_smi_vga,
	post = mem_post,
	hide = true,
	base = 0x000a0000,
	size = 0x00010000,
}

filter_ram_low_2 = {
	id = -1,
	name = "MEM",
	pre = mem_ram_low,
	post = mem_post_pre_ram_only,
	hide = true,
	base = 0xc0000,
	size = 0x20000
}


function mem_target_only(f, action)
	action.to_hw = true
	action.to_qemu = false
	return true
end

-- 3.25GB RAM. This is handled by SerialICE.
-- FIXME: use TOLM here

-- We refrain from backing up this memory in Qemu because Qemu would
-- need lots of ram on the host and firmware usually does not intensively
-- use high memory anyways.
filter_ram_high = {
	id = -1,
	name = "MEM",
	pre = mem_target_only,
	post = mem_post_pre_ram_only,
	hide = true,
	base = 0x100000,
	size = 0xd0000000 - 0x100000
}


function ram_enabled()
	return ram_is_initialized
end

function enable_ram()

	enable_hook(mem_hooks, filter_ram_low)
	enable_hook(mem_hooks, filter_smi_vga)
	enable_hook(mem_hooks, filter_ram_low_2)
	enable_hook(mem_hooks, filter_ram_high)

	-- Register low RAM 0x00000000 - 0x000dffff
	SerialICE_register_physical(0x00000000, 0xa0000)
	-- SMI/VGA memory should go to the target...
	SerialICE_register_physical(0x000c0000, 0x20000)
	--printf("Low RAM accesses are now directed to Qemu.\n")
	ram_is_initialized = true
end




