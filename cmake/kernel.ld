/* kernel.ld */
   OUTPUT_FORMAT(binary)
   ENTRY(Init_Kernellib)

   /* Bootloader-populated data at fixed physical addresses */
   mmap_entry_count = 0x7000;
   mmap_bios_entries = 0x5000;

   SECTIONS {
       . = 0x13000;

       .text : {
           *(.text.entry)
           *(.text)
       }

       .rodata : {
           *(.rodata)
           *(.rodata.*)
       }

       .data : {
           *(.data)
           *(.data.rel.ro*)
           *(.data.rel*)
           *(.got*)
       }

       .bss : {
           *(.bss)
       }

       _kernel_end = .;

       /DISCARD/ : {
           *(.eh_frame)
           *(.note*)
           *(.comment)
       }
   }
