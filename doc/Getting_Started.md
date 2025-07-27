# First Steps With SerialICE

Connect the target to your computer. Figure out the serial port that you
can use to talk to it. Relative to the qemu directory in the SerialICE
tree, call

    $ cd SerialICE/simba
    $ ln -sf ../../qemu-0.15.x/i386-softmmu/qemu
    $ ./qemu -M serialice -serialice /dev/ttyUSB0 -singlestep -bios /your/bios/image > logfile.txt

This assumes that /dev/ttyUSB0 is your serial port. Replace as
appropriate. You'll get a logfile.txt something like this:

    SerialICE: Open connection to target hardware...
    SerialICE: Waiting for handshake with target... target alive!
    SerialICE: Version.....: SerialICE v1.5 (Feb 19 2013)
    SerialICE: Mainboard...: Commell LV-672
    SerialICE: LUA init...
    SerialICE: LUA init...
    SerialICE: Starting LUA script
    SerialICE: LUA script initialized.
    Registering physical memory at 0xffdf8000 (0x00008000 bytes)
    VNC server running on `127.0.0.1:5900'
    0000.0001    R.Q.    [ffff000:fff0]   MEM:  readb fffffff0 => e9
    0000.0002    R.Q.    [ffff000:fff0]   MEM:  readw fffffff1 => f00d
    0003.0004    RH..    [ffff000:f006]   IO: outb 0080 <= 01
    0003.0005    R.Q.    [ffff000:f018]   MEM:  readl fffff04e => 00000000
    0003.0006    R.Q.    [ffff000:f01c]   MEM:  readw fffff01d => f044
    0003.0007    R.Q.    [ffff000:f021]   MEM:  readl fffff046 => fffff054
    0003.0008    R.Q.    [ffff000:f029]   MEM:  readl fffff02b => 7ffaffd1
    0003.0009    R.Q.    [ffff000:f02f]   MEM:  readl fffff031 => 60000001
    0003.000a    R.Q.    [ffff000:f038]   MEM:  readb fffff038 => 66
    0003.000b    R.Q.    [ffff000:f03b]   MEM:  readl fffff03d => fffff07b
    0003.000c    R.Q.    [ffff000:f03b]   MEM:  readw fffff041 => 0008
    0003.000d    RH..    [0000:fffff07f]   IO: outb 0080 <= 10
    0003.000e    R.Q.    [0000:fffff081]   MEM:  readw fffff083 => 0010
    0003.000f    R.Q.    [0000:fffff096]   MEM:  readl fffff097 => 00000200
    0003.0010    R.Q.    [0000:fffff0a4]   MEM:  readl fffff0a5 => 0000001b
    0011.0012    RH.U    [0000:fffff0a9]   CPU MSR: [0000001b] => 00000000.fee00900
    0011.0013    R.Q.    [0000:fffff0b4]   MEM:  readl fffff0b5 => 8000f8dc
    0011.0014    R.Q.    [0000:fffff0b9]   MEM:  readw fffff0bb => 0cf8
    0016.0017    RH..    [0000:fffff0bd]   IO: outl 0cf8 <= 8000f8dc
    0016.0018    RH..    [0000:fffff0c2]   IO:  inb 0cfc => 00
    0016.0019    R.Q.    [0000:fffff0d0]   MEM:  readl fffff0d1 => 8000f8dc
    0016.001a    R.Q.    [0000:fffff0d5]   MEM:  readw fffff0d7 => 0cf8
    001b.001c    RH..    [0000:fffff0d9]   IO: outl 0cf8 <= 8000f8dc
    001b.001d    RH..    [0000:fffff0e2]   IO: outb 0cfc <= 08
    001b.001e    R.Q.    [0000:fffff0e8]   MEM:  readl fffff0e9 => fffff0ef
    001b.001f    R.Q.    [0000:fffff105]   MEM:  readl fffff106 => fffffffc
    001b.0020    R.Q.    [0000:fffff10a]   MEM:  readl ffffefe8 => 00000800
    001b.0021    R.Q.    [0000:fffff11d]   MEM:  readl fffff11e => 00000000
    001b.0022    R.Q.    [0000:fffff130]   MEM:  readl fff80000 => 4352414c
    001b.0023    R.Q.    [0000:fffff132]   MEM:  readl fffff1b2 => 4352414c
    001b.0024    R.Q.    [0000:fffff13d]   MEM:  readl fffff13f => fffff1b6
    001b.0025    R.Q.    [0000:fffff13d]   MEM:  readl fffff1b6 => 45564948
    001b.0026    R.Q.    [0000:fff8006c]   MEM:  readl fff80056 => fff80054
    001b.0027    R.Q.    [0000:fff80074]   MEM:  readl fff80075 => fff8007b
    001b.0028    R.Q.    [0000:fff80074]   MEM:  readw fff80079 => 0008
    0029.002a    RH..    [0000:fff8007f]   IO: outb 0080 <= 10
    0029.002b    R.Q.    [0000:fff80081]   MEM:  readw fff80083 => 0010
    0029.002c    RH..    [0000:fff800b1]   IO: outb 0080 <= 20
    002d.002e    RH.U    [0000:fff800b8]   CPU MSR: [0000001b] => 00000000.fee00900
    002d.002f    R.Q.    [0000:fff800ba]   MEM:  readl fff800bb => 00000100
    002d.0030    R.Q.    [0000:fff800bf]   MEM:  readl fff800c1 => 00000158
    002d.0031    R.Q.    [0000:fff800c5]   MEM:  readl fff800c6 => fff803fb
    002d.0032    R.Q.    [0000:fff800ca]   MEM:  readl fff800cb => 0000001b
    002d.0033    R.Q.    [0000:fff800d3]   MEM:  readw fff803fb => 0250

This is referred to as the raw logfile from qemu session. It usually
shows more detail than you care to look at, so you will want to further
process this through a replay script.

    $ cat logfile.txt | lua replay.lua

The interesting part of the log is below, where loads from flash memory
are hidden and PCI configuration access is translated. You can control
the verbosity of the replayer with the parameters in user_env.lua file.

    0003.0004    .H..    [ffff000:f006]   POST: *** 01 ***
    0003.000d    .H..    [0000:fffff07f]   POST: *** 10 ***
    0011.0012    RH.U    [0000:fffff0a9]   CPU MSR: [0000001b] => 00000000.fee00900
    0016.0018    .H..    [0000:fffff0c2]   PCI: 0:1f.0 [0dc] => 00
    001b.001d    .H..    [0000:fffff0e2]   PCI: 0:1f.0 [0dc] <= 08
    0029.002a    .H..    [0000:fff8007f]   POST: *** 10 ***
    0029.002c    .H..    [0000:fff800b1]   POST: *** 20 ***
    002d.002e    RH.U    [0000:fff800b8]   CPU MSR: [0000001b] => 00000000.fee00900
