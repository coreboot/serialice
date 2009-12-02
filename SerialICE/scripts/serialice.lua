--
-- SerialICE
--
-- Copyright (c) 2009 coresystems GmbH
--
-- Permission is hereby granted, free of charge, to any person obtaining a copy
-- of this software and associated documentation files (the "Software"), to deal
-- in the Software without restriction, including without limitation the rights
-- to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
-- copies of the Software, and to permit persons to whom the Software is
-- furnished to do so, subject to the following conditions:
--
-- The above copyright notice and this permission notice shall be included in
-- all copies or substantial portions of the Software.
--
-- THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
-- IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
-- FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
-- THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
-- LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
-- OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
-- THE SOFTWARE.
--

io.write("SerialICE: Starting LUA script\n")

-- If you get an error here, install bitlib 
-- (ie. http://luaforge.net/projects/bitlib/)
require("bit")

function printf(s,...)
	return io.write(s:format(...))
end


-- In the beginning, during RAM initialization, it is essential that 
-- all DRAM accesses are handled by the target, or RAM will not work
-- correctly. After RAM initialization, RAM access has no "special"
-- meaning anymore, so we can just use Qemu's memory (and thus get
-- an incredible speed-up)

ram_is_initialized = false

-- Whether to log read access (code fetches) to 0xe0000 to 0xfffff

log_rom_access = false

-- Remember the PCI device selected via IO CF8

SerialICE_pci_device = 0

-- SerialICE_io_read_filter is the filter function for IO reads.
--
-- Parameters:
--   port	IO port to be read
--   data_size	Size of the IO read
-- Return values:
--   caught	True if the filter intercepted the read
--   data	Value returned if the read was intercepted

function SerialICE_io_read_filter(port, data_size)
	local data = 0
	local caught = false

	-- **********************************************************
	--

	-- if port == 0x42 then
	-- 	printf("WARNING: Hijacking timer port 0x42\n")
	-- 	data = 0x80
	-- 	caught = true
	-- end

	-- **********************************************************
	--
	-- Serial Port handling

	if port >= 0x3f8 and port <= 0x3ff then
		printf("Serial I/O read (filtered)\n")
		data = 0xff
		caught = true
	end

	-- **********************************************************
	--

	return caught, data
end

SerialICE_superio_4e_reg = 0
SerialICE_superio_2e_reg = 0
SerialICE_superio_2e_ldn = 0

PCIe_bar  = 0
PCIe_size = 0

-- SerialICE_io_write_filter is the filter function for IO writes.
--
-- Parameters:
--   port	IO port to be written to
--   size	Size of the IO write
--   data	Data to be written to
-- Return values:
--   caught	True if the filter intercepted the write
--   data	Value returned if the write was *not* intercepted

function SerialICE_io_write_filter(port, size, data)
	-- **********************************************************
	--
	-- PCI config space handling

	if port == 0xcf8 then
		SerialICE_pci_device = data
		return false, data
	end

	if port == 0xcfc then
		-- Phoenix BIOS reconfigures 0:1f.0 reg 0x80/0x82.
		-- This effectively wipes out our communication channel
		-- so we mut not allow it.
		if SerialICE_pci_device == 0x8000f880 then
			printf("LPC (filtered)\n")
			return true, data
		end

		-- Catch PCIe base address
		if SerialICE_pci_device == 0x80000048 then
			PCIe_bar  = bit.band(0xfc000000,data)
			PCIe_size = 64 * 1024 -- hard coded for now.
			printf("PCIe BAR set up: 0x%08x\n", PCIe_bar)
		end

		return false, data
	end

	-- **********************************************************
	--
	-- Dell 1850 BMC filter

	if port == 0xe8 then
		-- lua lacks a switch statement
		if	data == 0x44656c6c then printf("BMC: Dell\n")
		elseif	data == 0x50726f74 then printf("BMC: Prot\n")
		elseif	data == 0x496e6974 then
			printf("BMC: Init (filtered)\n")
			return true, data
		else
			printf("BMC: unknown %08x\n", data)
		end
		return false, data
	end

	-- **********************************************************
	--
	-- SuperIO config handling

	if port == 0x4e then
		SerialICE_superio_4e_reg = data
		return false, data
	end

	if port == 0x4f then
		-- Don't allow that our Serial power gets disabled.
		if SerialICE_superio_4e_reg == 0x02 then
			printf("SuperIO (filtered)\n")
			return true, data
		end
		-- XXX what's this?
		if SerialICE_superio_4e_reg == 0x24 then
			printf("SuperIO (filtered)\n")
			return true, data
		end
	end

	if port == 0x2e then
		-- We start requiring a decent state machine
		SerialICE_superio_2e_reg = data
		return false, data
	end

	if port == 0x2f then
		-- Winbond
		if SerialICE_superio_2e_reg == 0x06 then
			SerialICE_superio_2e_ldn = data
			return false, data
		end

		-- Don't allow that our SIO power gets disabled.
		if SerialICE_superio_2e_reg == 0x02 then
			printf("SuperIO (filtered)\n")
			return true, data
		end

		-- XXX what's this?
		if SerialICE_superio_2e_reg == 0x24 then
			printf("SuperIO (filtered)\n")
			return true, data
		end
	end

	-- **********************************************************
	--
	-- Serial Port handling


	if port > 0x3f8 and port <= 0x3ff then
		printf("serial I/O (filtered)\n")
		return true, data
	end

	if port == 0x3f8 then
		printf("COM1: %c\n", data)
		return true, data
	end

	-- **********************************************************
	--
	-- Intel 82945 (reference BIOS) RAM switch
	--

	-- The RAM initialization code for the i945 used by AMI and
	-- Phoenix uses the same POST codes. We use this to determine
	-- when RAM init is done on that chipset.
	-- Not catching the end of RAM init is not problematic, except
	-- that it makes decompression of the BIOS core to RAM incredibly
	-- slow as the decompressor inner loop has to be fetched
	-- permanently from the target (several reads/writes per 
	-- decompressed byte).

	if port == 0x80 and data == 0xff37 then
		ram_is_initialized = true
		-- Register low RAM 0x00000000 - 0x000dffff 
		SerialICE_register_physical(0x00000000, 0xa0000)
		-- SMI/VGA memory should go to the target...
		SerialICE_register_physical(0x000c0000, 0x20000)
		printf("\nLow RAM accesses are now directed to Qemu.\n")
	end

	return false, data
end


-- SerialICE_memory_read_filter is the filter function for memory reads
--
-- Parameters:
--   addr	memory address to be read
--   size	Size of the memory read
-- Return values:
--   to_hw	True if the read should be directed to the target
--   to_qemu	True if the read should be directed to Qemu
--   result	Read result if both to_hw and to_qemu are false

function SerialICE_memory_read_filter(addr, size)

	-- Example: catch memory read and return a value
	-- defined in this script:
	--
	-- if addr == 0xfec14004 and size == 4 then
	-- 	return false, false, 0x23232323
	-- end

	if	addr >= 0xfff00000 and addr <= 0xffffffff then
		-- ROM accesses go to Qemu only
		return false, true, 0
	elseif	addr >= 0xf0000000 and addr <= 0xf3ffffff then
		-- PCIe MMIO config space accesses are
		-- exclusively handled by the SerialICE
		-- target
		return true, false, 0
	elseif	addr >= 0xfed10000 and addr <= 0xfed1ffff then
		-- Intel chipset BARs are exclusively 
		-- handled by the SerialICE target
		return true, false, 0
	elseif	addr >= 0xffd80000 and addr <= 0xffdfffff then
		-- coreboot Cache-As-RAM is exclusively
		-- handled by Qemu (RAM backed)
		return false, true, 0
	elseif	addr >= 0xffbc0000 and addr <= 0xffbfffff then
		-- AMI Cache-As-RAM is exclusively
		-- handled by Qemu (RAM backed)
		return false, true, 0
	elseif	addr >= 0xfee00000 and addr <= 0xfeefffff then
		-- Local APIC.. Hm, not sure what to do here.
		-- We should avoid that someone wakes up cores
		-- on the target system that go wild.
		return true, false, 0 -- XXX Handled by target
	elseif	addr >= 0xfec00000 and addr <= 0xfecfffff then
		-- IO APIC.. Hm, not sure what to do here.
		return true, false, 0 -- XXX Handled by target
	elseif	addr >= 0xfed40000 and addr <= 0xfed45000 then
		-- ICH7 TPM
		-- Phoenix "Secure" Core bails out if we don't pass this on ;-)
		return true, false, 0
	elseif	addr >= 0x000e0000 and addr <= 0x000fffff then
		-- Low ROM accesses go to Qemu memory
		return false, true, 0
	elseif	addr >= 0x000a0000 and addr <= 0x000affff then
		-- SMI/VGA go to target
		return true, false, 0
	elseif	addr >= 0x00000000 and addr <= 0x000dffff then
		-- RAM access. This is handled by SerialICE
		-- but *NOT* exclusively. Writes should end
		-- up in Qemu memory, too
		if not ram_is_initialized then
			-- RAM init has not not been marked done yet.
			-- so send reads to the target only.
			return true, false, 0
		end
		-- RAM init is done. Send all RAM accesses
		-- to Qemu. Using the target as storage would
		-- only slow execution down.
		-- TODO handle VGA / SMI memory correctly
		return false, true, 0
	elseif	addr >= 0x00100000 and addr <= 0xcfffffff then
		-- 3.25GB RAM. This is handled by SerialICE.
		-- We refrain from backing up this memory in Qemu
		-- because Qemu would need 3.25G RAM on the host
		-- and firmware usually does not intensively use
		-- high memory anyways.
		return true, false, 0
	else
		printf("\nWARNING: undefined load operation @%08x\n", addr)
		-- Fall through and handle by Qemu
	end
	return false, true, 0
end

-- SerialICE_memory_write_filter is the filter function for memory writes
--
-- Parameters:
--   addr	memory address to write to
--   size	Size of the memory write
--   data	Data to be written
-- Return values:
--   to_hw	True if the write should be directed to the target
--   to_qemu	True if the write should be directed to Qemu
--   result	Data to be written (may be changed in filter)

function SerialICE_memory_write_filter(addr, size, data)
	if	addr >= 0xfff00000 and addr <= 0xffffffff then
		printf("\nWARNING: write access to ROM?\n")
		-- ROM accesses go to Qemu only
		return false, true, data
	elseif	addr >= 0xf0000000 and addr <= 0xf3ffffff then
		-- PCIe MMIO config space accesses are
		-- exclusively handled by the SerialICE
		-- target
		return true, false, data
	elseif	addr >= 0xfed10000 and addr <= 0xfed1ffff then
		-- Intel chipset BARs are exclusively 
		-- handled by the SerialICE target
		return true, false, data
	elseif	addr >= 0xffd80000 and addr <= 0xffdfffff then
		-- coreboot Cache-As-RAM is exclusively
		-- handled by Qemu (RAM backed)
		return false, true, data
	elseif	addr >= 0xffbc0000 and addr <= 0xffbfffff then
		-- AMI Cache-As-RAM is exclusively
		-- handled by Qemu (RAM backed)
		return false, true, data
	elseif	addr >= 0xfee00000 and addr <= 0xfeefffff then
		-- Local APIC.. Hm, not sure what to do here.
		-- We should avoid that someone wakes up cores
		-- on the target system that go wild.
		return true, false, data
	elseif	addr >= 0xfec00000 and addr <= 0xfecfffff then
		-- IO APIC.. Hm, not sure what to do here.
		return true, false, data
	elseif	addr >= 0xfed40000 and addr <= 0xfed45000 then
		-- ICH7 TPM
		return true, false, data
	elseif	addr >= 0x000e0000 and addr <= 0x000fffff then
		-- Low ROM accesses go to Qemu memory
		return false, true, data
	elseif	addr >= 0x000a0000 and addr <= 0x000affff then
		-- SMI/VGA go to target
		return true, true, data
	elseif	addr >= 0x00000000 and addr <= 0x000dffff then
		-- RAM access. This is handled by SerialICE during 
		-- RAM initialization and by Qemu later on.
		if not ram_is_initialized then
			return true, true, data
		end
		-- Don't send writes to the target for speed reasons.
		return false, true, data
	elseif	addr >= 0x00100000 and addr <= 0xcfffffff then
		-- 3.25 GB RAM ... This is handled by SerialICE
		return true, false, data
	else
		printf("\nWARNING: undefined store operation @%08x\n", addr)
		-- Fall through, send to SerialICE
	end

	return true, false, data
end

function SerialICE_msr_read_filter(addr, hi, lo)
	return false, hi, lo
end

function SerialICE_msr_write_filter(addr, hi, lo)
	return false, hi, lo
end

function SerialICE_cpuid_filter(eax, ecx)
	-- set all to 0 so they're defined but return false, so the 
	-- result is not filtered.
	-- NOTE: If the result is filtered, all four registers are 
	-- overwritten.
	eax = 0
	ebx = 0
	ecx = 0
	edx = 0
	return false, eax, ebx, ecx, edx
end


-- --------------------------------------------------------------------
-- logging functions

function SerialICE_memory_write_log(addr, size, data, target)
	if addr >= 0x00000000 and addr <= 0x0009ffff and ram_is_initialized then
		return
	end
	if addr >= 0x000c0000 and addr <= 0x000dffff and ram_is_initialized then
		return
	end

	if size == 1 then	printf("MEM: writeb %08x <= %02x", addr, data)
	elseif size == 2 then	printf("MEM: writew %08x <= %04x", addr, data)
	elseif size == 4 then	printf("MEM: writel %08x <= %08x", addr, data)
	end
	if target then
		printf(" *")
	end
	printf("\n")

	-- **********************************************************
	--

	if (PCIe_bar ~= 0) and addr >= PCIe_bar and addr <= (PCIe_bar + PCIe_size) then
		printf("PCIe %x:%02x.%x R.%02x\n",
			bit.band(0xff,bit.rshift(addr, 20)),
			bit.band(0x1f,bit.rshift(addr, 15)),
			bit.band(0x7,bit.rshift(addr, 12)),
			bit.band(0xfff,addr))
	end
end

function SerialICE_memory_read_log(addr, size, data, target)
	if addr >= 0x00000000 and addr <= 0x0009ffff and ram_is_initialized then
		return
	end
	if addr >= 0x000c0000 and addr <= 0x000dffff and ram_is_initialized then
		return
	end
	if addr >= 0xe0000 and addr <= 0xfffff and not log_rom_access then
		return
	end
	if addr >= 0xfff00000 and addr <= 0xffffffff and not log_rom_access then
		return
	end

	if size == 1 then	printf("MEM:  readb %08x => %02x", addr, data)
	elseif size == 2 then	printf("MEM:  readw %08x => %04x", addr, data)
	elseif size == 4 then	printf("MEM:  readl %08x => %08x", addr, data)
	end
	if target then
		printf(" *")
	end
	printf("\n")

	-- **********************************************************
	--

	if (PCIe_bar ~= 0) and addr >= PCIe_bar and addr <= (PCIe_bar + PCIe_size) then
		printf("PCIe %x:%02x.%x R.%02x\n",
			bit.band(0xff,bit.rshift(addr, 20)),
			bit.band(0x1f,bit.rshift(addr, 15)),
			bit.band(0x7,bit.rshift(addr, 12)),
			bit.band(0xfff,addr))
	end
end

function SerialICE_io_write_log(port, size, data, target)
	if size == 1 then	printf("IO: outb %04x <= %02x\n", port, data)
	elseif size == 2 then	printf("IO: outw %04x <= %04x\n", port, data)
	elseif size == 4 then	printf("IO: outl %04x <= %08x\n", port, data)
	end

	-- **********************************************************
	--

	if port >= 0xcfc and port <= 0xcff then
		printf("PCI %x:%02x.%x R.%02x\n",
			bit.band(0xff,bit.rshift(SerialICE_pci_device, 16)),
			bit.band(0x1f,bit.rshift(SerialICE_pci_device, 11)),
			bit.band(0x7,bit.rshift(SerialICE_pci_device, 8)),
			bit.band(0xff,SerialICE_pci_device + (port - 0xcfc) ))
	end
end

function SerialICE_io_read_log(port, size, data, target)
	if size == 1 then	printf("IO:  inb %04x => %02x\n", port, data)
	elseif size == 2 then	printf("IO:  inw %04x => %04x\n", port, data)
	elseif size == 4 then	printf("IO:  inl %04x => %08x\n", port, data)
	end

	-- **********************************************************
	--

	if port >= 0xcfc and port <= 0xcff then
		printf("PCI %x:%02x.%x R.%02x\n",
			bit.band(0xff,bit.rshift(SerialICE_pci_device, 16)),
			bit.band(0x1f,bit.rshift(SerialICE_pci_device, 11)),
			bit.band(0x7,bit.rshift(SerialICE_pci_device, 8)),
			bit.band(0xff,SerialICE_pci_device + (port - 0xcfc) ))
	end
end

function SerialICE_msr_write_log(addr, hi, lo, filtered)
	printf("CPU: wrmsr %08x <= %08x.%08x\n", addr, hi, lo)
end

function SerialICE_msr_read_log(addr, hi, lo, filtered)
	printf("CPU: rdmsr %08x => %08x.%08x\n", addr, hi, lo)
end

function SerialICE_cpuid_log(in_eax, in_ecx, out_eax, out_ebx, out_ecx, out_edx, filtered)
	printf("CPU: CPUID eax: %08x; ecx: %08x => %08x.%08x.%08x.%08x\n", 
			in_eax, in_ecx, out_eax, out_ebx, out_ecx, out_edx)
end

-- --------------------------------------------------------------------
-- This initialization is executed right after target communication
-- has been established

printf("SerialICE: Registering physical memory areas for Cache-As-Ram:\n")

-- Register Phoenix BIOS Cache as RAM area as normal RAM 
-- 0xffd80000 - 0xffdfffff
SerialICE_register_physical(0xffd80000, 0x80000)

-- Register AMI BIOS Cache as RAM area as normal RAM
-- 0xffbc0000 - 0xffbfffff
SerialICE_register_physical(0xffbc0000, 0x40000)

printf("SerialICE: LUA script initialized.\n")

return true

