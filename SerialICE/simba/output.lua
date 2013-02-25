

froot = {
	id = 0,
	name = "SerialICE",
}

fresource = {
	id = -1,
	name = "Resource",
}

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
	if action.undefined or action.f or action == cpu_action then
		str = str .. "R"
	else
		str = str .. "."
	end
	if action.to_hw then
		str = str .. "H"
	elseif action.to_qemu then
		str = str .. "Q"
	else
		str = str .. "."
	end
	if action.undefined then
		str = str .. "U"
	else
		str = str .. "."
	end
	if action.dropped then
		str = str .. "D"
	elseif action.faked then
		str = str .. "F"
	else
		str = str .. "."
	end

	print_address(action.parent_id, action.my_id, str, action.cs, action.eip)

	if action.f then
		printf("%s,%s: ", f.name, action.f.name)
		printf(fmt, ...)
	else
		printf("%s: ", f.name)
		printf(fmt, ...)
	end
end

function printks(f, fmt, ...)
	print_address(0,0,"I...",0,0)
	printf("%s: ", f.name)
	printf(fmt, ...)
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
	elseif size == 8 then return string.format("%16x", data)
	else return string.format("Error: size=%x", size)
	end
end



