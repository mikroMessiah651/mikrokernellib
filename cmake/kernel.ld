/* kernel.ld */
   /*OUTPUT_FORMAT(binary)*/
   ENTRY(start_kernel)

   /* Bootloader-populated data at fixed physical addresses */
   mmap_entry_count = 0x7000;
   mmap_bios_entries = 0x5000;

   /* Higher-half link base and the physical address the bootloader
    * actually loads the kernel bytes at. VMA = KERNEL_VMA + KERNEL_PHYS,
    * LMA = KERNEL_PHYS (via AT() below), so symbols resolve high while
    * the raw bytes still land where the bootloader's disk read puts them. */
   KERNEL_VMA = 0xffffffff80000000;
   KERNEL_PHYS = 0x13000;

   SECTIONS {
       . = KERNEL_VMA + KERNEL_PHYS;

        _kernel_start = .;

       .text : AT(ADDR(.text) - KERNEL_VMA) {
           *(.text.entry)
           *(.text)
       }
       . = ALIGN(4096);
        _text_end = .;

        _rodata_start = .;
       .rodata : AT(ADDR(.rodata) - KERNEL_VMA) {
           *(.rodata)
           *(.rodata.*)
       }
       . = ALIGN(4096);
       _rodata_end = .;

        _data_start = .;
       .data : AT(ADDR(.data) - KERNEL_VMA) {
           *(.data)
           *(.data.rel.ro*)
           *(.data.rel*)
           *(.got*)
       }

        _bss_start = .;
       .bss : AT(ADDR(.bss) - KERNEL_VMA) {
           *(.bss)
       }

        . = ALIGN(4096);
       _kernel_end = .;

       /* Physical (low) aliases, for code that needs the kernel image's
        * physical extent before/while the high mapping is being built. */
       _kernel_start_phys = _kernel_start - KERNEL_VMA;
       _kernel_end_phys = _kernel_end - KERNEL_VMA;

       /DISCARD/ : {
           *(.eh_frame)
           *(.note*)
           *(.comment)
       }
   }
