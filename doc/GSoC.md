# Google Summer of Code

The SerialICE project will hopefully participate in GSoC 2016 under the
patronage of [coreboot's GSoC administration](http://coreboot.org/GSoC).
Depending on the number and quality of applications, and available
mentors, there might be room for a separate SerialICE project. If you're
interested, please send your thoughts and ideas on the [mailing
list](http://www.serialice.com/mailman/listinfo/serialice) and come
discuss them on IRC <irc://irc.freenode.net/#coreboot>.

It is not likely that the project organisation would supply any hardware
required to complete or even start a project on SerialICE. So if you
want to apply, you should first try building and running SerialICE
yourself to understand its current capabilities and weaknesses. You
really don't need a high-end mainboard or CPUs on these projects. While
it's a powerful tool for low-level debugging and understanding even part
of it requires a fair amount of knowledge on x86 architecture, there are
also challenges in the user interface development.

The list below is a collection of improvement ideas and capabilities
that would be nice to have. Some of these could be merged with [coreboot
GSoC](http://www.coreboot.org/GSoC) projects or support [flashrom
GSoC](http://www.flashrom.org/GSoC) projects.

## SerialICE on target

- Build target ROM image with super-IO and PnP from coreboot tree.
- Support EHCI debug port, extend the protocol for memory block moves.
- Investigate possibilities to catch SMI and run System Management Mode.

## SIMBA, the filtering subsystem

- Query PCI IDs to detect chipsets and load filters automatically.
- Enable modifying SMBus traffic on-the-fly to forge SPD data for
  testing purposes.
- Create log output conditionally of CS:EIP or accessed PCI device.
- Decode PCI/PCI-e standard configuration registers.
- Enable injection of IO and memory transactions.

## User interface

- Well, provide one. Now it is just a logfile and a script.
- Display SerialICE log in parallel with disassembly. Could be
  integrated with [radare2](http://rada.re/y/?p=features)
- Visualize PCI devicetree configuration sequence and allocations.
- An editor that knows the log format and supports user-assisted folding
  of loops that wait for bit flips, naming of registers, ...
- Integration with GDB and DDD.

## coreboot

- Collect IO and PCI transactions on boot and store them in cbmem.
  Replay them to see what devicetree really did during ramstage.
- Complete and merge [coreboot panic
  room](http://www.coreboot.org/Project_Ideas#coreboot_panic_room)
  results upstream.

## QEMU

- Update to a more current QEMU.
- Support other architectures.
- Create hybrid platform, where some devices are emulated and some run
  on real hardware.

## Port to Unicorn

[Unicorn](http://www.unicorn-engine.org/) may be a more suitable (and
maybe even stable) base for our purposes than QEmu because it shares
some of SerialICE's goals. Needs investigation if that's actually a good
idea _before_ GSoC starts (although such an investigation is already a
great way to show that you're capable of finishing this project)
