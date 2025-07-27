# Debugging
After you started the SerialICE patched Qemu, you can start a gdbserver
via the Qemu monitor. To enter the Qemu monitor, click into the Qemu
"VGA window" and press CTRL-ALT-2. The virtual machine will continue
executing code while you are in the monitor. Here you can start a
gdbserver on port 1234 by typing:

    gdbserver

Alternatively, you can start QEmu with the arguments `-s -S`, which
automatically puts QEmu in gdbserver mode and stops it at the entry
point. you can then connect gdb as described below.

## Connect with GDB

    $ i386-elf-gdb
    GNU gdb 6.8
    Copyright (C) 2008 Free Software Foundation, Inc.
    License GPLv3+: GNU GPL version 3 or later <`[`http://gnu.org/licenses/gpl.html`](http://gnu.org/licenses/gpl.html)`>
    This is free software: you are free to change and redistribute it.
    There is NO WARRANTY, to the extent permitted by law.  Type "show copying"
    and "show warranty" for details.
    This GDB was configured as "--host=i386-apple-darwin9.6.0 --target=i386-elf".
    (gdb)

Now set the target architecture to i8086 (16bit)

     (gdb) set architecture i8086
     The target architecture is assumed to be i8086 

and connect to the "remote machine"

     (gdb) target remote localhost:1234
     Remote debugging using localhost:1234
     [New Thread 1]
     0x0000084a in ?? ()

Note: The IP is a real mode IP. You need to look at the CS to find out
where your code really lives.

To disassemble code at the current position, you can do this:

     (gdb) x/5i ($cs*16)+$eip
     0xec23c:loop   0xec226
     0xec23e:mov    %bh,%al
     0xec240:out    %al,$0x61
     0xec242:ret    
     0xec243:lcall  $0xe5db,$0x1ac0
     (gdb) 

Now for the fun part:

Look at the registers with

     (gdb) info reg
     eax            0x2e03023015426
     ecx            0x1d2466
     edx            0x302770
     ebx            0x300112289
     esp            0x16310x1631
     ebp            0x54a00030x54a0003
     esi            0xe00357347
     edi            0x11
     eip            0x38ec0x38ec
     eflags         0x12[ AF ]
     cs             0xe89559541
     ss             0xe89559541
     ds             0x18386200
     es             0x00
     fs             0xe5db58843
     gs             0xe89559541
     (gdb) 

Or, with a little bit of context:

     (gdb) x/20i ($cs*16)+$eip - 0x10

prints 20 instructions, starting 0x10 before the current CS:IP.

Of course you can also change registers:

     (gdb) set var $ecx=0x20
     (gdb) print $ecx
     $1 = 32
     (gdb) 

This is an excellent method to skip long loops.

Now you can continue code execution with

     (gdb) c
     Continuing.
     
     Go back to the GDB shell at any time with CTRL-C
     ^C
     Program received signal SIGINT, Interrupt.
     0x000038d6 in ?? ()
     (gdb) 

This will output the current instruction each time you call nexti,
stepi, etc.:

     (gdb) display /i ($cs * 16) + $pc

It's possible to trace execution with

     (gdb) while 1
        >    stepi
        >  end

(newlines are necessary) That snippet is useful to find out the code
flow and pinpoint a crash to an instruction.
