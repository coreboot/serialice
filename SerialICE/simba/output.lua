
-- -------------------------------------------------------------------
-- logging functions

function printf(s,...)
	return io.write(s:format(...))
end

function print_address(pid, id, flags, cs, eip)
	printf("%04x.%04x    %s    [%04x:%04x]   ",pid,id,flags,cs,eip)
end

function printk(f, action, fmt, ...)
	local str = ""

	-- For Qemu runs only Raw actions are logged
	if not replaying and not f.raw then
		return
	end

	-- For replays, a post filter prevents further lines for the
	-- same action from printing if it has property hide set.
	if replaying and not log_everything then
		if action.logged_post and action.logged_post.hide then
			return
		end
	end

	if f.raw then
		str = str .. "R"
	elseif action.info_only then
		str = str .. "I"
	else
		str = str .. "."
	end
	if action.to_hw then
		str = str .. "H"
	else
		str = str .. "."
	end
	if action.to_qemu then
		str = str .. "Q"
	else
		str = str .. "."
	end
	if f.raw and not (action.logged_pre or action.logged_post) then
		str = str .. "U"
	elseif action.dropped then
		str = str .. "D"
	elseif action.faked then
		str = str .. "F"
	else
		str = str .. "."
	end

	print_address(action.parent_id, action.my_id, str, action.cs, action.eip)

	if not replaying then
		printf("%s: ", f.name)
		printf(fmt, ...)
	else
		if not f.raw then
			printf("%s: ", f.name)
			printf(fmt, ...)
		elseif action.logged_post then
			printf("%s: ", action.logged_post.name)
			printf(fmt, ...)
		elseif action.logged_pre then
			printf("%s: ", action.logged_pre.name)
			printf(fmt, ...)
		else
			printf("%s: ", f.name)
			printf(fmt, ...)
		end
	end
end

info_action = {}

function print_info(f, str)
	pre_action(info_action)
	info_action.info_only = true
	info_action.parent_id = current_parent_id
	info_action.my_id = next_action_id
	printk(f, info_action, "%s", str)
end

function root_info(fmt, ...)
	print_info(froot, string.format(fmt, ...))
end

function resource_info(f, fmt, ...)
	print_info(fresource,
		string.format("[%04x] %s ", f.id, f.list.name) ..
		string.format(fmt, ...) .. "\n")
end

function trim (s)
	return (string.gsub(s, "^%s*(.-)%s*$", "%1"))
end

function size_suffix(size)
	if     size == 1 then return "b"
	elseif size == 2 then return "w"
	elseif size == 4 then return "l"
	elseif size == 8 then return "ll"
	else return string.format("invalid size: %d", size)
	end
end

function size_data(size, data)
	if     size == 1 then return string.format("%02x", data)
	elseif size == 2 then return string.format("%04x", data)
	elseif size == 4 then return string.format("%08x", data)
	elseif size == 8 then return string.format("%016x", data)
	else return string.format("Error: size=%d", size)
	end
end
