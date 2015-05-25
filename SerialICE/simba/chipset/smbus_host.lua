

local debug_smbus = false
smbus = {}

smbus.host = {
	state = { current = 0, jump_to = 0 },
	proto = 0,
	start_proto = 0,

	passive = true,
	signals = 0,

	slave = 0,
	cmd = 0,
	data0 = 0,
	data1 = 0,
	data0_valid = false,
	data1_valid = false,
	wr_data0 = 0,
	wr_data1 = 0,
	block_register = 0,

	block = {},
	max_index = 0,
}

MAX_BLOCK_SRAM = 32

if true then
	for i = 0, MAX_BLOCK_SRAM-1, 1 do
		smbus.host.block[i] = { hw = 0, tmp = 0, hw_valid = false, tmp_valid = false }
	end
end

-- *******************

SIG_INUSE 	= 0
SIG_RELEASE 	= 1
SIG_START 	= 2
SIG_ABORT 	= 3
SIG_DONE 	= 4
SIG_ACK 	= 5
SIG_TIMEOUT 	= 6
SIG_TIMEOUT_ACK	= 7
SIG_DATA_WRITE 	= 8

local function signal_reset(f)
	f.host.signals = 0
end

local function signal_set(f, flag)
	local mask = (1 << flag)
	f.host.signals = (f.host.signals | mask)
end

local function signal_clr(f, flag)
	local mask = ~(1 << flag)
	f.host.signals = (f.host.signals & mask)

end

local function signal_in(f, flag)
	local mask = (1 << flag)
	return (f.host.signals & mask) ~= 0
end

-- *******************

-- SMBus Protocol Message Types
SMBUS_NOOP = 0
SMBUS_QUICK = 1
SMBUS_BYTE = 2
SMBUS_BYTE_DATA = 3
SMBUS_WORD_DATA = 4
SMBUS_PROC_CALL = 5
SMBUS_BLOCK_DATA = 6
SMBUS_I2C_BLOCK_DATA = 7
SMBUS_BLOCK_PROCESS = 8

local proto_name = {}
proto_name[SMBUS_NOOP] = "(no-op)"
proto_name[SMBUS_QUICK] = "quick"
proto_name[SMBUS_BYTE] = "byte"
proto_name[SMBUS_BYTE_DATA] = "byte_data"
proto_name[SMBUS_WORD_DATA] = "word_data"
proto_name[SMBUS_PROC_CALL] = "proc_call"
proto_name[SMBUS_BLOCK_DATA] = "block"
proto_name[SMBUS_I2C_BLOCK_DATA] = "i2c_block"
proto_name[SMBUS_BLOCK_PROCESS] = "block_process"

-- SMBus Host
SMB_REG_CMD   = 1
SMB_REG_SLAVE = 2
SMB_REG_DATA0 = 3
SMB_REG_DATA1 = 4
SMB_REG_BLOCK = 5
SMB_REG_PROTO = 6


local function dump_block(f, length)
	local block = ""
	local max_length = math.min(length, MAX_BLOCK_SRAM)

	for i=0, max_length-1, 1 do
		if f.host.block[i].hw_valid then
			block = block .. string.format(" %02x", f.host.block[i].hw)
		else
			block = block .. " xx"
		end
	end
	return block
end


-- FIXME: Probably wrong for process calls and hw-dependent.
local function host_proto(f, proto)
	return f.host.proto == proto
end

local function host_reading(f)
	return (f.host.slave & 0x01) == 0x01
end



local function dump_transaction(f, action)

	local data0, data1, length, dir
	local invalid_data = "xx"
	local iodir = {}
	iodir[0] = "<="
	iodir[1] = "=>"

	if host_reading(f) then
		dir = iodir[1]
		data0 = invalid_data
		length = f.host.max_index
		if f.host.data0_valid then
			length = f.host.data0
			data0 = string.format("%02x", f.host.data0)
		end
		data1 = invalid_data
		if f.host.data1_valid then
			data1 = string.format("%02x", f.host.data1)
		end
	else
		dir = iodir[0]
		length = f.host.wr_data0
		data0 = string.format("%02x", f.host.wr_data0)
		data1 = string.format("%02x", f.host.wr_data1)
	end


	local dump = string.format("%02x %s ", f.host.slave >> 1, proto_name[f.host.proto])

	if host_proto(f, SMBUS_QUICK) then

	elseif host_proto(f, SMBUS_BYTE) then
		dump = dump .. string.format("%02x %s %s", f.host.cmd, dir, data0)

	elseif host_proto(f, SMBUS_BYTE_DATA) then
		dump = dump .. string.format("%02x %s %s", f.host.cmd, dir, data0)

	elseif host_proto(f, SMBUS_WORD_DATA) then
		dump = dump .. string.format("%02x %s %s%s", f.host.cmd, dir, data0, data1)

	elseif host_proto(f, SMBUS_PROC_CALL) then
		dump = dump .. string.format("%02x %02x %02x %s %s %s", f.host.cmd,
			f.host.wr_data0, f.host.wr_data1, iodir[1], data0, data1)

	elseif host_proto(f, SMBUS_BLOCK_DATA) then
		dump = dump .. string.format("%02x len=%02d %s", f.host.cmd, length, dir)
		dump = dump .. dump_block(f, length)  .. ""

	elseif host_proto(f, SMBUS_I2C_BLOCK_DATA) then
		dump = dump .. string.format("%02x %02x %02x len=%02d %s",
			f.host.cmd, f.host.data0, f.host.data1, f.host.max_index, dir)
		dump = dump .. dump_block(f, length) .. ""

	elseif host_proto(f, SMBUS_BLOCK_PROCESS) then
		dump = dump .. string.format("%02x len=%02d %s", f.host.cmd, length, iodir[1])
		dump = dump .. dump_block(f, length) .. ""
	else
		dump = dump .. "Cannot parse command"
	end

	if signal_in(f, SIG_TIMEOUT) then
		dump = dump .. " (TIMEOUT) "
	end

	printk(f, action, "%s\n", dump)
end


-- *******************

HOST_NOOP = 0
HOST_IDLE = 1
HOST_ACTIVE = 2
HOST_STARTED = 3
HOST_WAIT = 4
HOST_COMPLETE = 5
HOST_FAIL = 6

local ctrl_state = {}
ctrl_state[HOST_NOOP] = "noop"
ctrl_state[HOST_IDLE] = "idle"
ctrl_state[HOST_ACTIVE] = "active"
ctrl_state[HOST_STARTED] = "started"
ctrl_state[HOST_WAIT] = "wait"
ctrl_state[HOST_COMPLETE] = "complete"
ctrl_state[HOST_FAIL] = "failed"

function dprintk(...)
	if debug_smbus then
		printk(...)
	end
end

local function host_jump(f, state)
	f.host.state.jump_to = state
end

local function host_change_state(f, prev_state, new_state)

	if new_state == HOST_NOOP then
		--printk(f, f.host.action, "state switch to HOST_NOOP\n")
		new_state = HOST_IDLE
	end

	dprintk(f, f.host.action, "%s -> %s\n",
		ctrl_state[prev_state], ctrl_state[new_state])

	-- Jumping to current is no jump.
	f.host.state.current = new_state
	f.host.state.jump_to = new_state

	if smbus.state(f, HOST_IDLE) then
		signal_reset(f)

	elseif smbus.state(f, HOST_ACTIVE) then
		signal_reset(f)
		signal_set(f, SIG_INUSE)

	elseif smbus.state(f, HOST_STARTED) then
		local i
		f.host.proto = f.host.start_proto
		f.host.wr_data0 = f.host.data0
		f.host.wr_data1 = f.host.data1

		-- Invalidation used with reads
		f.host.data0_valid = false
		f.host.data1_valid = false

		for i = 0, MAX_BLOCK_SRAM-1, 1 do
			f.host.max_index = 0
			-- On block writes, previously read data in buffer is also valid.
			if f.host.block[i].tmp_valid then
				f.host.block[i].hw = f.host.block[i].tmp
				f.host.block[i].tmp_valid = false
				f.host.block[i].hw_valid = true
			end
			-- On block reads, no data in buffer is yet valid.
			if host_reading(f) and host_proto(f, SMBUS_BLOCK_DATA) then
				f.host.block[i].hw_valid = false;
			end
		end

	elseif smbus.state(f, HOST_COMPLETE) then
		dump_transaction(f, f.host.action)
		if signal_in(f, SIG_RELEASE) then
			host_jump(f, HOST_IDLE)
		else
			host_jump(f, HOST_ACTIVE)
		end

	elseif smbus.state(f, HOST_FAIL) then
		dump_transaction(f, f.host.action)
		host_jump(f, HOST_ACTIVE)
	end

end

local function host_switch(f, new_state)
	local prev_state = f.host.state.current
	while prev_state ~= new_state do
		host_change_state(f, prev_state, new_state)
		prev_state = f.host.state.current
		new_state = f.host.state.jump_to
	end
end

local function host_read_completed(f)

	if not host_reading(f) then
		return true
	end

	local complete = false

	if host_proto(f, SMBUS_QUICK) then
		complete = true

	elseif host_proto(f, SMBUS_BYTE) then
		complete = f.host.data0_valid

	elseif host_proto(f, SMBUS_BYTE_DATA) then
		complete = f.host.data0_valid

	elseif host_proto(f, SMBUS_WORD_DATA) then
		complete = f.host.data0_valid and f.host.data1_valid

	elseif host_proto(f, SMBUS_BLOCK_DATA) then
		complete = f.host.data0_valid and f.host.max_index >= f.host.data0

	elseif host_proto(f, SMBUS_PROC_CALL) or host_proto(f, SMBUS_I2C_BLOCK_DATA)
					or host_proto(f, SMBUS_BLOCK_PROCESS) then
	       printk(f, f.host.action, "Unimplemented completion (proto %d)\n", f.host.proto)
	end
	return complete
end


-- Syncronize state machine according to input signals.
local function host_sync(f)

--	if release and not smbus.state(f, HOST_ACTIVE) then
--		printk(f, f.host.action, "Premature reset of bit INUSE_STS\n")
--	end
	if signal_in(f, SIG_ABORT) then
		-- FIXME Killing on-going transaction.
		host_switch(f, HOST_COMPLETE)
	end

	if smbus.state(f, HOST_IDLE) then
		if signal_in(f, SIG_INUSE) then
			host_switch(f, HOST_ACTIVE)
		end
		if signal_in(f, SIG_START) then
			host_switch(f, HOST_STARTED)
		end

	elseif smbus.state(f, HOST_ACTIVE) then
		if signal_in(f, SIG_START) then
			host_switch(f, HOST_STARTED)
		end

	elseif smbus.state(f, HOST_STARTED) then
		if signal_in(f, SIG_TIMEOUT) then
			host_switch(f, HOST_FAIL)
		elseif signal_in(f, SIG_DONE) then
			host_switch(f, HOST_WAIT)
		end

	elseif smbus.state(f, HOST_WAIT) then
		if signal_in(f, SIG_START) then
			-- Restarting previous transaction.
			host_switch(f, HOST_COMPLETE)
			host_switch(f, HOST_STARTED)
		elseif signal_in(f, SIG_ACK) and not host_reading(f) then
			-- Complete after sw ack.
			host_switch(f, HOST_COMPLETE)
		elseif signal_in(f, SIG_DATA_WRITE) or host_read_completed(f) then
			-- Complete after all data read or new data written
			-- remain active
			signal_clr(f, SIG_RELEASE)
			host_switch(f, HOST_COMPLETE)
		end
	elseif smbus.state(f, HOST_FAIL) then
		if signal_in(f, SIG_TIMEOUT_ACK) then
			host_switch(f, HOST_COMPLETE)
		end
	end

	if signal_in(f, SIG_START) and not smbus.state(f, HOST_STARTED) then
		printk(f, f.host.action, "Starting from illegal state\n");
	end

	signal_clr(f, SIG_DONE)
	signal_clr(f, SIG_START)
	signal_clr(f, SIG_DATA_WRITE)
end



-- *******************************

-- Mutual exclusion.
function smbus.get_resource(f)
	signal_set(f, SIG_INUSE)
	host_sync(f)
end

function smbus.put_resource(f)
	signal_set(f, SIG_RELEASE)
	host_sync(f)
end

-- status
function smbus.state(f, state)
	return f.host.state.current == state
end

function smbus.passive(f)
	return f.host.passive
end

-- control
function smbus.start(f, proto)
	signal_set(f, SIG_START)
	f.host.start_proto = proto
	host_sync(f)
end

function smbus.timeout(f)
	signal_set(f, SIG_TIMEOUT)
	host_sync(f)
end

function smbus.timeout_ack(f)
	signal_set(f, SIG_TIMEOUT_ACK)
	host_sync(f)
end

function smbus.done(f)
	signal_set(f, SIG_DONE)
	host_sync(f)
end

function smbus.ack(f)
	signal_set(f, SIG_ACK)
	host_sync(f)
end

function smbus.abort(f)
	signal_set(f, SIG_ABORT)
	host_sync(f)
end

-- A data read may complete and close an active transaction.
function smbus.data_read(f, action)
	if not action.write then
		signal_clr(f, SIG_DATA_WRITE)
		host_sync(f)
	end
end

-- A data write will close active transaction.
function smbus.data_write(f, action)
	if action.write then
		signal_set(f, SIG_DATA_WRITE)
		host_sync(f)
	end
end



function smbus.update_register(f, action, smb_reg)

	local data_write = action.write or smbus.passive(f)

	-- A write to data registers completes previous transaction.
	smbus.data_write(f, action)

	if smb_reg == SMB_REG_SLAVE then
		if data_write then
			f.host.slave = action.data
		else
			action.data = f.host.slave
		end

	elseif smb_reg == SMB_REG_CMD then
		if data_write then
			f.host.cmd = action.data
		else
			action.data = f.host.cmd
		end

	elseif smb_reg == SMB_REG_DATA0 then
		if data_write then
			f.host.data0 = action.data
		else
			action.data = f.host.data0
		end
		f.host.data0_valid = true

	elseif smb_reg == SMB_REG_DATA1 then
		if data_write then
			f.host.data1 = action.data
		else
			action.data = f.host.data1
		end
		f.host.data1_valid = true

	elseif smb_reg == SMB_REG_BLOCK then
		if data_write then
			f.host.block_register = action.data
		else
			action.data = f.host.block_register
		end
		-- Nothing here, smbus.host_block_data updates datas.
		-- This exist to check completion below for blocks.
	elseif smb_reg == SMB_REG_PROTO then
		-- Nothing here. Protocol updates when signalling start.
	else
	       printk(f, f.host.action, "Undefined host register\n")
	end

	-- New read data may complete a waiting transaction.
	smbus.data_read(f, action)
end

function smbus.block_data(f, action, index)

	if smbus.passive(f) then
		if not action.write then
			f.host.block[index].hw = action.data
			f.host.block[index].hw_valid = true
		end
		f.host.block[index].tmp = action.data
		f.host.block[index].tmp_valid = true
	else
		if action.write then
			f.host.block[index].tmp = action.data
			f.host.block[index].tmp_valid = true
		else
			action.data = 0xff
			if f.host.block[index].tmp_valid then
				action.data = f.host.block[index].tmp
			elseif f.host.block[index].hw_valid then
				action.data = f.host.block[index].hw
			end
		end
	end

	-- Detect for block read completion via maximum indexed item.
	if not action.write then
		f.host.max_index = math.max(f.host.max_index, index+1)
	end
end

local init_action = {
	name = "init",
	cs = 0,
	eip = 0,
	my_id = 0,
	parent_id = 0,
}
function smbus.init(f)
	if not f.host then
		f.host = smbus.host
		f.host.action = init_action
	end
	host_switch(f, HOST_IDLE)
end


