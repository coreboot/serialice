-- Set to "true" to show all memory and IO access in output
log_everything = false

-- Set to "false" to show undecoded access for the specified class
hide_rom_access = true
hide_ram_low = true
hide_ram_high = true
hide_smi_vga = true
hide_car = true
hide_pci_io_cfg = true
hide_pci_mm_cfg = true
hide_nvram_io = true
hide_i8042_io = false
hide_i8237_io = true
hide_i8254_io = true
hide_i8259_io = true
hide_superio_cfg = true
hide_smbus_io = true
hide_com = false
hide_mainboard_io = true
hide_mainboard_mem = true

-- Use lua table for NVram
-- RTC registers 0x0-0xd go to HW
cache_nvram = false

-- SMSC 0x07, Winbond 0x06 ?
DEFAULT_SUPERIO_LDN_REGISTER = 0x07

-- We refrain from backing up most of memory in Qemu because Qemu would
-- need lots of ram on the host and firmware usually does not intensively
-- use high memory anyways.
top_of_qemu_ram = 0x800000
tolm = 0xd0000000
