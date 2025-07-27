# Installation
## Building SerialICE

Take note of the size of the flash chip you will use for SerialICE and
set that correctly while in menuconfig.

     $ cd SerialICE
     $ make menuconfig
     $ make

You can now flash the serialice.rom image. You can execute flashrom on
the target machine like this:

     $ flashrom -p internal -w serialice.rom

You can also use external programmer devices. In either case, keep a
copy of the original mainboard bios image, preferably use a different
chip for SerialICE purposes. After programming the flash, do a cold
reboot of the target machine.

Next check with a terminal program of your choice (eg
[minicom](http://alioth.debian.org/projects/minicom),
[picocom](http://code.google.com/p/picocom/)) that you are seeing a
SerialICE shell prompt. If you do not get a prompt, see
<a href="Make_SerialICE_work_on_new_hardware" class="wikilink"
title="Make_SerialICE_work_on_new_hardware">Make_SerialICE_work_on_new_hardware</a>.

     SerialICE v1.5 (Nov 20 2012)
     > 
     CTRL-A Z for help |115200 8N1 | NOR | Minicom 2.3    | VT102 |      Offline  

## Building QEMU

You need to build a patched QEMU from source, and you will need Lua \>=
5.2. To build Qemu you can run the build script that was added by the
SerialICE patch:

     $ sh build.sh

You are now ready to start using SerialICE. [Getting
Started](Getting_Started) provides an intro to using SerialICE, while
[Log_file](Log_file) explains the output format. Advanced topics like
[Debugging](Debugging) have information about using gdb with SerialICE
targets, and [Scripting](Scripting) describes the basics of writing
filters to match the hardware.
