# Log file
This page describes the structure of a replayed output. Running qemu is
explained here: <http://www.serialice.com/Getting_Started>

First number pair is a running index that could be used to group and
indent the lines to improve readability.

Second string is combination of flags telling how action got processed:

      I Info         -- informational line only
      R Raw          -- raw IO/MEMORY/CPU operation
      H Hardware     -- action was sent to target
      Q Qemu         -- action was sent to qemu
      U Undefined    -- action hit the fallback filter
      D Dropped      -- filter prevented sending action to target or qemu
      F Faked        -- filter modified the action on-the-fly

Third field is the instruction pointer \[CS:EIP\].

Remaining of the line describes the action.

A few examples:

Memory access, Raw + Qemu. Following is read of vector stored in the
BIOS image file.

      0000.0001    R.Q.    [ffff000:fff0]   MEM,ROM_HI:  readb fffffff0 => ea
      0000.0002    R.Q.    [ffff000:fff0]   MEM,ROM_HI:  readw fffffff1 => e05b
      0000.0003    R.Q.    [ffff000:fff0]   MEM,ROM_HI:  readw fffffff3 => f000

PCI config read, Hardware. This is composed from either IO accesses to
0xcf8-0xcff or memory access to a specific PCI-e MM config region. Thus
it is not Raw but a composed action.

      0044.0046    .H..    [f000:f764]   PCI: 0:1e.0 [004] => 00

CPUID, Raw + Hard + Faked. CPUID was executed on the target, but the
returned value was modified. In this case, it fakes CPU has a single
core.

      000d.000e    RH.F    [f000:e814]   CPUID: eax: 00000001; ecx: 00000000 => 00000f4a.00010800.0000649d.bfebfbff

RDMSR Raw + Hard and WRMSR Raw + Dropped. In this case, requst to do
microcode update in target CPU is dropped as our serialice.rom image
doesn't contain that binary.

      0019.001a    RH..    [f000:e869]   CPU MSR: [00000017] => 00120000.00000000
      001c.001d    R..D    [f000:e88e]   CPU MSR: [00000079] <= 00000000.fffdfc10


(The above is taken from the commit message found at
<http://review.coreboot.org/#/c/2511/> and was slightly edited)
