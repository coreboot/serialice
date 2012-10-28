-- **********************************************************
-- CPU MSR filters

function var_mtrr_post(f, action)

	local addr = action.rin.ecx
	local hi = action.rin.edx
	local lo = action.rin.eax

	if addr % 2 == 0 then
		mt = lo % 0x100
		if     mt == 0 then memtype = "Uncacheable"
		elseif mt == 1 then memtype = "Write-Combine"
		elseif mt == 4 then memtype = "Write-Through"
		elseif mt == 5 then memtype = "Write-Protect"
		elseif mt == 6 then memtype = "Write-Back"
		else memtype = "Unknown"
		end
		printk(f, action, "Set MTRR %x base to %08x.%08x (%s)\n", (addr - 0x200) / 2, hi, bit32.band(lo, 0xffffff00), memtype)
	else
		if bit32.band(lo, 0x800) == 0x800 then
			valid = "valid"
		else
			valid = "disabled"
		end
		printk(f, action, "Set MTRR %x mask to %08x.%08x (%s)\n", (addr - 0x200) / 2, hi, bit32.band(lo, 0xfffff000), valid)
	end
end

function cpumsr_pre(f, action)
	return handle_action(f, action)
end

function cpumsr_post(f, action)
	if action.write then
		printk(f, action, "[%08x] <= %08x.%08x\n",
			action.rin.ecx,	action.rin.edx, action.rin.eax)
		if action.addr >= 0x200 and action.addr < 0x210 then
			var_mtrr_post(f, action)
		end
	else
		printk(f, action, "[%08x] => %08x.%08x\n",
			action.rin.ecx,	action.rout.edx, action.rout.eax)
	end
	return true
end


filter_cpumsr_fallback = {
	id = -1,
	name = "CPU MSR",
	pre = cpumsr_pre,
	post = cpumsr_post,
}


-- **********************************************************
-- CPUID filters

function cpuid_pre(f, action)
	return handle_action(f, action)
end

function cpuid_post(f, action)
	printk(f, action, "eax: %08x; ecx: %08x => %08x.%08x.%08x.%08x\n",
			action.rin.eax, action.rin.ecx,
			action.rout.eax, action.rout.ebx, action.rout.ecx, action.rout.edx)
	return true
end

filter_cpuid_fallback = {
	id = -1,
	name = "CPUID",
	pre = cpuid_pre,
	post = cpuid_post,
}

