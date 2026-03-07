mikroBoot is a x86-64 bootloader that:

1. enables vga graphics mode 3
2. checks for LBA
3. loads stage 2 with either CHS or LBA
4. jumps to stage 2
stage 2 does:
1. loads kernel using CHS or LBA
2. loads memory map from BIOS using int 0x15 eax = 0xe820
3. enables a20, loads a gdt, enables protected mode and far jumps to protected mode
4. in protected mode, it checks for CPUID and queries long mode capability
5. if long mode is unsupported, it prints a long mode unsupported message and halts, jmp $
6. if long mode is supported, it sets up identity paging, loads a 64 bit gdt, enables long mode, and far jumps to kernel

