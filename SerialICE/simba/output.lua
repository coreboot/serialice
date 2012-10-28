

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

function printk(f, action, fmt, ...)
	printf("[%04x:%04x] ", action.cs, action.eip)
	printf("%04x.%04x ", action.parent_id, action.my_id)

	local str = " "
	if action.dropped or action.faked then
		str = "!"
	end
	if action.undefined then
		str = "#"
	end

	if action.f then
		printf("%s %s,%s: ", str, f.name, action.f.name)
		printf(fmt, ...)
	else
		printf("%s %s: ", str, f.name)
		printf(fmt, ...)
	end
end

function printks(f, fmt, ...)
	printf("[%04x:%04x] ", 0, 0)
	printf("%04x.%04x ", 0, 0)
	printf("  %s: ", f.name)
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



