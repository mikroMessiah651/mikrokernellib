/* kernel.ld */
   /*OUTPUT_FORMAT(binary)*/
   ENTRY(__init_kernellib)

   /* Bootloader-populated data at fixed physical addresses */
   mmap_entry_count = 0x7000;
   mmap_bios_entries = 0x5000;

   SECTIONS {
       . = 0x13000;

        _kernel_start = .;

       .text : {
           *(.text.entry)
           *(.text)
       }
       . = ALIGN(4096);
        _text_end = .;

        _rodata_start = .;
       .rodata : {
           *(.rodata)
           *(.rodata.*)
       }
       . = ALIGN(4096);
       _rodata_end = .;

        _data_start = .;
       .data : {
           *(.data)
           *(.data.rel.ro*)
           *(.data.rel*)
           *(.got*)
       }

        _bss_start = .;
       .bss : {
           *(.bss)
       }

        . = ALIGN(4096);
       _kernel_end = .;

       /DISCARD/ : {
           *(.eh_frame)
           *(.note*)
           *(.comment)
       }
   }
