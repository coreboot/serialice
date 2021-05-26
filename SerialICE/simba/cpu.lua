-- **********************************************************
-- CPU MSR filters

function var_mtrr_post(f, action)
	local addr = action.rin.ecx
	local hi = action.rin.edx
	local lo = action.rin.eax

	if not action.write then
		return true
	end

	if addr % 2 == 0 then
		mt = lo % 0x100
		if     mt == 0 then memtype = "Uncacheable"
		elseif mt == 1 then memtype = "Write-Combine"
		elseif mt == 4 then memtype = "Write-Through"
		elseif mt == 5 then memtype = "Write-Protect"
		elseif mt == 6 then memtype = "Write-Back"
		else memtype = "Unknown"
		end
		printk(f, action, "Set MTRR %x base to %08x.%08x (%s)\n", (addr - 0x200) / 2, hi, (lo & 0xffffff00), memtype)
	else
		if (lo & 0x800) == 0x800 then
			valid = "valid"
		else
			valid = "disabled"
		end
		printk(f, action, "Set MTRR %x mask to %08x.%08x (%s)\n", (addr - 0x200) / 2, hi, (lo & 0xfffff000), valid)
	end
	return true
end

function mtrr_pre(f, action)
	local addr = action.rin.ecx
	if addr >= 0x200 and addr < 0x210 then
		handle_action(f, action)
		return true
	end
	return false
end

function mtrr_post(f, action)
	local addr = action.rin.ecx
	if addr >= 0x200 and addr < 0x210 then
		return var_mtrr_post(f, action)
	end
	return false
end

filter_mtrr = {
	name = "MTRR",
	pre = mtrr_pre,
	post = mtrr_post,
	hide = true,
}

function cpumsr_pre(f, action)
	handle_action(f, action)
	return true
end

function cpumsr_post(f, action)
	if action.write then
		printk(f, action, "[%08x] <= %08x.%08x\n",
			action.rin.ecx,	action.rin.edx, action.rin.eax)
	else
		printk(f, action, "[%08x] => %08x.%08x\n",
			action.rin.ecx,	action.rout.edx, action.rout.eax)
	end
	return true
end


filter_cpumsr_fallback = {
	name = "CPU MSR",
	raw = true,
	pre = cpumsr_pre,
	post = cpumsr_post,
}


-- **********************************************************
-- CPUID filters

microcode_patchlevel_eax = 0
microcode_patchlevel_edx = 0

function cpuid_pre(f, action)
	handle_action(f, action)
	return true
end

function cpuid_post(f, action)
	printk(f, action, "eax: %08x; ecx: %08x => %08x.%08x.%08x.%08x\n",
			action.rin.eax, action.rin.ecx,
			action.rout.eax, action.rout.ebx, action.rout.ecx, action.rout.edx)
	return true
end

filter_cpuid_fallback = {
	name = "CPUID",
	raw = true,
	pre = cpuid_pre,
	post = cpuid_post,
}

function feature_smx_pre(f, action)
	return false
end

function feature_smx_post(f, action)
	local rout = action.rout
	local rin = action.rin
	if not action.write and rin.eax == 0x01 then
		rout.ecx = rout.ecx & ~(1<<6)  -- SMX
		fake_action(f, action, 0)
		return true
	end
	return false
end

filter_feature_smx = {
	name = "CPU feature SMX",
	pre = feature_smx_pre,
	post = feature_smx_post,
}

function multicore_pre(f, action)
	return false
end

function multicore_post(f, action)
	local rout = action.rout
	local rin = action.rin
	-- Set number of cores to 1 on Core Duo and Atom to trick the
	-- firmware into not trying to wake up non-BSP nodes.
	if not action.write and rin.eax == 0x01 then
		rout.ebx = (0xff00ffff & rout.ebx);
		rout.ebx = (0x00010000 | rout.ebx);
		fake_action(f, action, 0)
		return true
	end
	return false
end

filter_multiprocessor = {
	name = "Multiprocessor Count",
	pre = multicore_pre,
	post = multicore_post,
}

-- Intel CPU microcode update
function intel_microcode_pre(f, action)
	if action.rin.ecx == 0x79 then
		--action.dropped = true
		--action.rout.edx = 0
		--action.rout.eax = 0xffff0000
		drop_action(f, action)
		return true
	end
	return false
end

-- Intel CPU microcode revision check
-- Fakes microcode revision of my 0x6f6 Core 2 Duo Mobile
function intel_microcode_post(f, action)
	if action.rin.ecx == 0x8b then
		action.rout.edx = microcode_patchlevel_edx
		action.rout.eax = microcode_patchlevel_eax
		fake_action(f, action, 0)
		return true
	end
	return false
end

filter_intel_microcode = {
	name = "Microcode Update",
	pre = intel_microcode_pre,
	post = intel_microcode_post,
}

-- AMD CPU microcode update
function amd_microcode_pre(f, action)
	if action.rin.ecx == 0xc0010020 then
		--action.dropped = true
		--action.rout.edx = 0
		--action.rout.eax = 0xffff0000
		drop_action(f, action)
		return true
	end
	return false
end

-- AMD CPU microcode revision check
-- Fakes microcode revision.
function amd_microcode_post(f, action)
	if action.rin.ecx == 0x8b then
		action.rout.eax = microcode_patchlevel_eax
		action.rout.edx = microcode_patchlevel_edx
		fake_action(f, action, 0)
		return true
	end
	return false
end

filter_amd_microcode = {
	name = "Microcode Update",
	pre = amd_microcode_pre,
	post = amd_microcode_post,
}
