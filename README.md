A bare-metal x86-64 kernel with a custom two-stage bootloader, written from scratch in C and x86 assembly.

## Overview

mikrokernellib is a hobby OS kernel targeting x86-64 hardware. It boots entirely from scratch via a hand-written BIOS bootloader and implements foundational kernel subsystems including interrupt handling, physical memory management, and VESA text output.

## Architecture

### Boot Sequence

```
BIOS → Stage 1 (512 B, LBA 0) → Stage 2 (1536 B, LBA 1–3) → Kernel (LBA 5+)
```

**Stage 1** (`src/Bootldr/mikroBootldr-stg1-x86.asm`) — 16-bit real mode, loaded at `0x7C00`:
- Enables VGA mode 3
- Detects LBA support via BIOS int `0x13`
- Loads 3 sectors of stage 2 to `0x7E00` (LBA or CHS fallback)

**Stage 2** (`src/Bootldr/mikroBootldr-stg2-x86.asm`) — 16-bit real mode, loaded at `0x7E00`:
- Loads 64 kernel sectors (LBA 5) to physical address `0x13000`
- Queries BIOS e820 memory map → stored at `0x5000`, count at `0x7000`
- Enables A20 line (BIOS → port `0x92` fallback)
- Loads a 32-bit GDT, enters protected mode
- Checks CPUID for 64-bit long mode support; halts if unavailable
- Sets up identity-mapped page tables with 2 MB huge pages (PML4T @ `0x9000`, PDPT @ `0xA000`, PDTs @ `0xB000`+), supporting up to 8 GB RAM
- Enables PAE + long mode (`EFER.LME`) + paging, far-jumps to the 64-bit kernel

**Kernel entry** (`src/mikroKernellib/ekInitKernellib.c`) — 64-bit long mode, at `0x13000`:
1. Clears VGA screen
2. Initializes the IDT (CPU exceptions 0–31)
3. Initializes the physical memory allocator (buddy + slab)
4. Halts in an `hlt` loop

### Disk Image Layout

| LBA | Contents |
|-----|----------|
| 0 | Stage 1 bootloader (512 B) |
| 1–3 | Stage 2 bootloader (1536 B) |
| 4 | Gap sector |
| 5+ | Kernel binary |

## Kernel Subsystems

### Interrupt Handling (`idt.c`, `idt_common.asm`, `isr_dispatch.c`)

- 64-bit IDT with 256 descriptors; ISRs 0–31 handle all CPU exceptions
- Assembly stubs save the full register file (15 GPRs + RIP/CS/RFLAGS/RSP/SS + vector + error code) before calling the C dispatcher
- PIC remapped: master IRQs → vector `0x20`, slave → `0x28`; all IRQs masked
- Page fault handler (`#PF`, vector 14) reads CR2 and prints the faulting virtual address

### Physical Memory Manager (`phys_kmalloc.c`)

**Buddy allocator** — page-frame allocator:
- Orders 0–10: `2^order × 4 KB` = 4 KB to 4 MB per allocation
- XOR buddy addressing for O(1) buddy lookup
- Bitmap tracks per-pair allocation state
- Coalesces on free when the buddy is free
- Bootstraps from the largest contiguous e820 usable region, aligned to max order

**Slab allocator** — small object allocator:
- 8 pre-configured caches: 16, 32, 64, 128, 256, 512, 1024, 2048 bytes
- Per-cache free / partial / full slab lists
- Slab descriptor embedded at the start of each slab (backed by buddy)
- Per-object freelist threading within each slab

All allocator state is protected by a single interrupt-safe spinlock.

### Spinlocks (`spinlocks.asm`)

- `xchg`-based test-and-set spin loop
- Saves and restores RFLAGS; disables interrupts while held
- Stores the holding CPU's APIC ID (via CPUID) for debugging

### VGA Text Output (`vga_graphics.c`)

- 80×25 text mode, buffer at `0xB8000`
- Functions: `vga_nt_println`, `vga_nt_printch`, `vga_clear_screen`, `vga_clear_lower_half`
- Helpers: hex and decimal formatting, e820 map display, 64-bit virtual address display

### Utilities

| Module | Functions |
|--------|-----------|
| `kmath.c` | `align_up`, `klog2`, `round_up_pow2`, `buddy_order` |
| `kstrings.c` | `kstring_length`, `kstring_strcpy`, `kstring_strcmp` |
| `mmu_page_tables.c` | `__init_mmu_paging` (stub — in progress) |
| `include/kasm.h` | `hlt`, `cli`, `sti`, `nop`, `outb` inline macros |
| `include/kasm-expanded.h` | Full inline-assembly library: CR/DR/MSR/segment registers, CPUID, RDTSC, atomics, TLB, XSAVE, and more |

## Memory Map

| Address | Contents |
|---------|----------|
| `0x5000` | e820 memory map entries |
| `0x7000` | e820 entry count |
| `0x7C00` | Stage 1 bootloader |
| `0x7E00` | Stage 2 bootloader |
| `0x9000` | PML4T (page table root) |
| `0xA000` | PDPT |
| `0xB000`+ | PDTs (up to 8 × 4 KB) |
| `0x13000` | Kernel binary (entry: `init_kernellib`) |
| `0xB8000` | VGA text buffer |

## Building

### Prerequisites

- `nasm` — assembler for bootloader and kernel assembly sources
- `x86_64-elf-gcc` or `x86_64-linux-gnu-gcc` — cross-compiler (freestanding)
- `cmake` >= 3.20
- `qemu-system-x86_64` — for running

### Build

```sh
cmake -B build -DCMAKE_TOOLCHAIN_FILE=cmake/toolchain-x86_64-elf.cmake
cmake --build build
```

This produces `build/disk.img`.

### Run

```sh
# Normal boot
cmake --build build --target run

# Debug (exposes GDB stub on port 1234)
cmake --build build --target run-debug
```

To attach GDB:
```sh
gdb -ex "target remote :1234" build/kernel.bin
```

## Project Structure

```
.
├── cmake/
│   ├── kernel.ld                  # Linker script (kernel @ 0x13000)
│   └── toolchain-x86_64-elf.cmake # Cross-compilation toolchain
├── docs/
│   └── mikroBoot.txt              # Bootloader design notes
├── src/
│   ├── Bootldr/
│   │   ├── mikroBootldr-stg1-x86.asm
│   │   └── mikroBootldr-stg2-x86.asm
│   └── mikroKernellib/
│       ├── ekInitKernellib.c      # Kernel entry point
│       ├── idt.c / idt_common.asm # IDT setup and ISR stubs
│       ├── isr_dispatch.c         # Exception handler dispatch
│       ├── vga_graphics.c         # VGA text mode output
│       ├── phys_kmalloc.c         # Buddy + slab allocator
│       ├── kmath.c                # Math utilities
│       ├── kstrings.c             # String utilities
│       ├── mmu_page_tables.c      # MMU init (stub)
│       ├── spinlocks.asm          # Spinlock primitives
│       └── include/               # All header files
└── CMakeLists.txt
```

## Status

| Subsystem | Status |
|-----------|--------|
| Stage 1 bootloader | Done |
| Stage 2 bootloader | Done |
| Long mode transition | Done |
| VGA text output | Done |
| IDT / exception handling | Done |
| Buddy allocator | Done |
| Slab allocator | Done |
| Spinlocks | Done |
| MMU / virtual memory | In progress |
| Process/task scheduling | Not started |
| Syscall interface | Not started |

README WAS MOSTLY GENERATED BY AI, CODE WAS NOT.
