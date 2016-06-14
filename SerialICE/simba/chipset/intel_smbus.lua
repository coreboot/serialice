

load_filter("smbus_host")

-- ********************************************
-- Intel 82801 SMBus Host controller

I801_SMB_HSTSTS = 0
I801_SMB_HSTCNT = 2
I801_SMB_HSTCMD = 3
I801_SMB_HSTADD = 4
I801_SMB_HSTDAT0 = 5
I801_SMB_HSTDAT1 = 6
I801_SMB_BLKDAT = 7
I801_SMB_PEC = 8
I801_SMB_AUXSTS = 12
I801_SMB_AUXCTL = 13

I801_QUICK		= 0x00
I801_BYTE		= 0x04
I801_BYTE_DATA		= 0x08
I801_WORD_DATA		= 0x0C
I801_PROC_CALL		= 0x10
I801_BLOCK_DATA		= 0x14
I801_I2C_BLOCK_DATA	= 0x18
I801_BLOCK_PROCESS	= 0x1C
I801_BLOCK_LAST		= 0x34  -- 0x14 | 0x20
I801_I2C_BLOCK_LAST	= 0x38  -- 0x18 | 0x20
I801_START		= 0x40
I801_PEC_EN		= 0x80


local function intel_smbus_get_protocol(f)
	local proto = (f.reg.control & 0x1c)

	if proto == I801_QUICK then
		return SMBUS_QUICK

	elseif proto == I801_BYTE then
		return SMBUS_BYTE

	elseif proto == I801_BYTE_DATA then
		return SMBUS_BYTE_DATA

	elseif proto == I801_WORD_DATA then
		return SMBUS_WORD_DATA

	elseif proto == I801_PROC_CALL then
		return SMBUS_PROC_CALL

	elseif proto == I801_BLOCK_DATA then
		return SMBUS_BLOCK_DATA

	elseif proto == I801_I2C_BLOCK_DATA then
		return SMBUS_I2C_BLOCK_DATA

	elseif proto == I801_BLOCK_PROCESS then
		return SMBUS_BLOCK_PROCESS

	else
		printk(f, action, "Unknown protocol\n")
		return SMBUS_NOOP
	end
end

local function intel_smbus_host_status(f, action)
	if not action.write then
		if smbus.passive(f) then
			f.reg.status = action.data;
		end

		if smbus.state(f, HOST_IDLE) then
			if not smbus.passive(f) then
				f.reg.status = 0x0
			end
			if (f.reg.status & 0x40) ~= 0 then
			       printk(f, action, "Host may be busy, ignoring.\n")
			end
			smbus.get_resource(f)

		elseif smbus.state(f, HOST_ACTIVE) then
			f.reg.busy_count = 3

		elseif smbus.state(f, HOST_STARTED) then
			if not smbus.passive(f) then
				f.reg.status = (f.reg.status & 0xFE)
				if f.reg.busy_count > 0 then
					f.reg.busy_count = f.reg.busy_count - 1
					f.reg.status = (f.reg.status | 0x01)
				end
				if (f.reg.status & 0x02) == 0 then
					smbus_transaction(host)
				end
			end

			local irq = (f.reg.status & 0x02) ~= 0
			local failures = (f.reg.status & 0x1c) ~= 0
			local host_busy = (f.reg.status & 0x01) ~= 0

			if irq and not host_busy then
				smbus.done(f)
			end
			if failures then
				smbus.timeout(f)
			end
		end

		if not smbus.passive(f) then
			action.data = f.reg.status;
			f.reg.status = (f.reg.status | 0x40)
		end
	else

		if not smbus.passive(f) then
			f.reg.status = (f.reg.status & ~action.data)
		end

		local ack_irq = (action.data & 0x02) ~= 0
		local release_host = (action.data & 0x40) ~= 0
		local failures = (action.data & 0x1c) ~= 0
		if release_host then
			smbus.put_resource(f)
		end
		if failures then
			smbus.timeout_ack(f)
		end
		if ack_irq then
			smbus.ack(f)
		end
	end

end


local function intel_smbus_host_control(f, action)

	if not action.write then
		f.reg.block_ptr=0;
		if not smbus.passive(f) then
			action.data = (f.reg.control & ~0x40)
		end

	else

		f.reg.control = action.data;
		if (f.reg.control & 0x80) ~= 0 then
			printk(f, action, "No PEC simulation\n")
		end

		local abort = (f.reg.control & 0x02) ~= 0
		local start = (f.reg.control & 0x40) ~= 0
		if abort then
			smbus.abort(f)
		end
		if start then
			f.reg.block_ptr=0;
			smbus.update_register(f, action, SMB_REG_PROTO)
			smbus.start(f, intel_smbus_get_protocol(f))
		end
	end
end



local function intel_smbus_block_data(f, action)
	if f.reg.block_ptr < MAX_BLOCK_SRAM then
		smbus.block_data(f, action, f.reg.block_ptr)
	end
	f.reg.block_ptr = f.reg.block_ptr + 1
	smbus.update_register(f, action, SMB_REG_BLOCK)
end

local function intel_smbus_host_access(f, action)
	local reg = action.addr & (f.size-1)

	-- mirror hw register both ways
	local data_write = 0

	-- Store this to display CS:IP etc.
	f.host.action = action

	if reg == I801_SMB_HSTSTS then
		intel_smbus_host_status(f, action);

	elseif reg == I801_SMB_HSTCNT then
		intel_smbus_host_control(f, action);

	elseif reg == I801_SMB_HSTCMD then
		smbus.update_register(f, action, SMB_REG_CMD)

	elseif reg == I801_SMB_HSTADD then
		smbus.update_register(f, action, SMB_REG_SLAVE)

	elseif reg == I801_SMB_HSTDAT0 then
		smbus.update_register(f, action, SMB_REG_DATA0)

	elseif reg == I801_SMB_HSTDAT1 then
		smbus.update_register(f, action, SMB_REG_DATA1)

	elseif reg == I801_SMB_BLKDAT then
		intel_smbus_block_data(f, action);

	elseif reg == I801_SMB_AUXSTS then
		if data_write then
			f.reg.aux_sts = action.data;
		else
			action.data = f.reg.aux_sts;
		end

	elseif reg == I801_SMB_AUXCTL then
		if data_write then
			f.reg.aux_ctl = action.data;
		else
			action.data = f.reg.aux_ctl;
		end

	else
		printk(f, action, "Unknown register 0x%02x\n", reg);
	end
end


function intel_smbus_host_pre(f, action)
	if action.write then
		intel_smbus_host_access(f, action)
	end
	handle_action(f, action)
	return true
end

function intel_smbus_host_post(f, action)
	if not action.write then
		intel_smbus_host_access(f, action)
	end
	return true
end


local intel_smbus_host = {
	name = "i801-smbus",
	pre = intel_smbus_host_pre,
	post = intel_smbus_host_post,
	hide = hide_smbus_io,
	base = 0x0,
	size = 0x0,
}

function intel_smbus_setup(base, size)
	local f = intel_smbus_host
	f.base = (base & ~(size-1))
	f.size = size
	if not f.reg then
		f.reg = { control = 0, status = 0, busy_count = 0, block_ptr = 0, aux_ctl = 0, aux_sts = 0 }
	end
	smbus.init(f)

	enable_hook(io_hooks, f)
end
