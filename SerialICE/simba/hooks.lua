

froot = { id = 0, name = "SerialICE" }
fresource = { id = 1, name = "Resource" }

function new_hooks(str, rule)
	return { list = nil, name = str, match_filter = rule }
end

next_filter_id = 2
next_action_id = 1
current_parent_id = 0

function action_id()
	local n = next_action_id
	next_action_id = next_action_id + 1
	return n
end

function new_parent_action()
	current_parent_id = action_id()
end

function filter_in_range(f, action)
	if f.enable and action.addr >= f.base and action.addr < f.base + f.size then
		return true
	end
	return false
end

function filter_enabled(f, action)
	return f.enable
end

io_hooks = new_hooks("IO", filter_in_range)
mem_hooks = new_hooks("MEM", filter_in_range)

cpumsr_hooks = new_hooks("CPU MSR", filter_enabled)
cpuid_hooks = new_hooks("CPUID", filter_enabled)

function enable_hook(list, filter)
	if not filter then
		root_info("Attempted to call enable_hook() with filter==nil\n")
		return
	end

	local l = list.list
	local found = false
	while l and not found do
		found = (l.hook == filter)
		l = l.next
	end
	if not found then
		filter.id = next_filter_id
		filter.list = list
		next_filter_id = next_filter_id + 1
		list.list = { next = list.list, hook = filter }
	end
	update_hook(filter)
	filter.enable = true
end

function disable_hook(f)
	if f.id and f.enable and f.enable == true then
		resource_info(f, "disabled")
	end
	f.enable = false
end

function update_hook(f)
	if f.list == io_hooks or f.list == mem_hooks then
		resource_info(f, "[%04x-%04x] = %s",
				f.base, f.base + f.size - 1, f.name)
	else
		resource_info(f, "%s", f.name)
	end
end

prev_filter = nil

function walk_pre_hooks(list, action)
	if list == nil or list.list == nil then
		return
	end
	local found = false
	local l = list.list
	local f = nil

	while l and not found do
		f = l.hook
		if list.match_filter(f, action) then
			if f.pre and f.pre(f, action) then
				if not f.raw then
					action.logged_pre = f
				end
				found = true
			end
		end
		l = l.next
	end
	if prev_filter ~= f and not action.ignore then
		prev_filter = f
		new_parent_action()
	end
	if action.dropped then
		action.to_hw = false
		action.to_qemu = false
	end
end

function walk_post_hooks(list, action)
	if list == nil or list.list == nil then
		return
	end
	local l = list.list
	local f = nil

	while l do
		f = l.hook
		if list.match_filter(f, action) then
			if f.post and f.post(f, action) then
				if not f.raw then
					action.logged_post = f
				end
			end
		end
		l = l.next
	end
end

function generic_io_bar(bar)
	if not bar.f then
		local f = {}
		f.pre = handle_action
		f.post = io_post
		f.decode = F_RANGE
		f.hide = true
		f.name = bar.name
		f.size = bar.size
		bar.f = f
	end
	bar.f.base = (bar.val & ~(bar.size-1))
	if (bar.f.base ~= 0) then
		enable_hook(io_hooks, bar.f)
	else
		disable_hook(bar.f)
	end
end

function generic_mmio_bar(bar)
	if not bar.f then
		local f = {}
		f.pre = handle_action
		f.post = mem_post
		f.decode = F_RANGE
		f.hide = true
		f.name = bar.name
		f.size = bar.size
		bar.f = f
	end
	bar.f.base = (bar.val & ~(bar.size-1))
	if bar.f.base ~= 0 then
		enable_hook(mem_hooks, bar.f)
	else
		disable_hook(bar.f)
	end
end

PRE_HOOK = 1
POST_HOOK = 2

function handle_action(f, action)
	action.to_hw = true
end

function drop_action(f, action, data)
	if action.stage == POST_HOOK then
		printk(f, action, "ERROR: Cannot drop action in a post-hook.\n")
		return
	end
	action.dropped = true
	action.data = data
end

function ignore_action(f, action)
	action.ignore = true
end

function fake_action(f, action, data)
	if action.stage == POST_HOOK and action.write then
		printk(f, action, "ERROR: Cannot fake write in a post-hook.\n")
		return
	end
	action.faked = true
	action.data = data
end

function pre_action(action, dir_wr, addr, size, data)
	action.stage = PRE_HOOK
	-- CS:IP logging
	action.cs = regs.cs
	action.eip = regs.eip
	action.parent_id = 0
	action.my_id = 0

	-- no filter, not filtered
	action.logged_pre = nil
	action.logged_post = nil
	action.ignore = false
	action.faked = false
	action.dropped = false
	action.to_hw = false
	action.to_qemu = false
	action.info_only = false

	action.write = dir_wr
	action.addr = addr

	action.data = 0
	action.size = size
	if action.write then
		if size == 1 then
			action.data = (0xff & data)
		elseif size == 2 then
			action.data = (0xffff & data)
		elseif size == 4 then
			action.data = (0xffffffff & data)
		elseif size == 8 then
			action.data = (0xffffffffffffffff & data)
		end
	end
end

function post_action(action, data)
	action.stage = POST_HOOK
	action.parent_id = current_parent_id
	action.my_id = action_id()

	if not action.write then
		if not action.faked and not action.dropped then
			action.data = data
		end
	end
end

function load_regs(regs, eax, ebx, ecx, edx)
	regs.eax = eax
	regs.ebx = ebx
	regs.ecx = ecx
	regs.edx = edx
end
