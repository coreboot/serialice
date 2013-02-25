
dofile("i82801.lua")
dofile("intel_bars.lua")

function do_mainboard_setup()
	do_default_setup()

	enable_hook_i82801dx()
	northbridge_i845()

	-- Apply mainboard hooks last, so they are the first ones to check
	--enable_hook(io_hooks, filter_mainboard)
end
