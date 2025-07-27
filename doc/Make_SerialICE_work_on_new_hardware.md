# Make SerialICE work on new hardware
Though it is designed to be as generic as possible the SerialICE shell
needs a little bit of hardware specific initialization code for each
supported mainboard. There is one file per mainboard with this
initialization code in the mainboard/ directory.

The purpose of the initialization code is to get the machine far enough
to be able to talk via serial console. This usually requires setup of
the Super I/O chip and some southbridge registers. In some cases extra
work may have to be done, like disabling a power-on watchdog timer,
enabling a fan(!), or enabling some devices "on the path to the Super
I/O".

Here is some simple example:

    
    /* Hardware specific functions */
    
    #define PMBASE 0x100
    #define TCOBASE 0x60
    #define TCO1_CNT 0x08
    
    static void southbridge_init(void)
    {
            /* Prevent the TCO timer from rebooting */
            /* Set ACPI base address (I/O space). */
            pci_write_config32(PCI_ADDR(0, 0x1f, 0, 0x40), (PMBASE | 1));
            /* Enable ACPI I/O. */
            pci_write_config8(PCI_ADDR(0, 0x1f, 0, 0x44), 0x10);
            /* Halt the TCO timer, preventing SMI and automatic reboot */
            outw(inw(PMBASE + TCOBASE + TCO1_CNT) | (1 << 11),
                 PMBASE + TCOBASE + TCO1_CNT);
    }
    
    static void superio_init(void)
    {
            /* Enter the configuration state. */
            pnp_enter_ext_func_mode_alt(0x2e);
    
            /* COM A */
            pnp_set_logical_device(0x2e, 4);
            pnp_set_enable(0x2e, 0);
            pnp_set_iobase0(0x2e, 0x3f8);
            pnp_set_irq0(0x2e, 4);
            pnp_set_enable(0x2e, 1);
    
            /* Exit the configuration state. */
            pnp_exit_ext_func_mode(0x2e);
    }
    
    static void chipset_init(void)
    {
            southbridge_init();
            superio_init();
    }
