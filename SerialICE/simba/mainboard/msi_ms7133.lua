
function mainboard_io_read(f, action)
	-- IO slowdown
	if action.addr == 0xed then
		ignore_action(f, action)
		drop_action(f, action, 0)
		return true
	end

	if action.addr == 0xcfb then
		ignore_action(f, action)
		drop_action(f, action, 0)
		return true
	end

	return false
end


function mainboard_io_write(f, action)
	if action.addr == 0xcfb then
		ignore_action(f, action)
		drop_action(f, action, 0)
		return true
	end

	if action.addr == 0xe1 then
		ignore_action(f, action)
		drop_action(f, action, action.data)
		return true
	end

	return false
end

function mainboard_io_pre(f, action)
	if action.write then
		return mainboard_io_write(f, action)
	else
		return mainboard_io_read(f, action)
	end
end

function mainboard_io_post(f, action)
	if action.addr == 0xe1 or action.addr == 0xed or action.addr == 0xcfb then
		return true
	end
	if action.addr == 0x80 and not action.write then
		return true
	end
end

filter_mainboard = {
	name = "test",
	pre = mainboard_io_pre,
	post = mainboard_io_post,
	hide = hide_mainboard_io,
	base = 0x0,
	size = 0x10000
}

load_filter("i82801")
load_filter("intel_bars")

function do_mainboard_setup()
	do_default_setup()

	enable_hook_i82801fx()
	northbridge_i915()

	-- Apply mainboard hooks last, so they are the first ones to check
	enable_hook(io_hooks, filter_mainboard)
end
