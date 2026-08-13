# mikrokernellib

A bare-metal x86-64 kernel with a custom two-stage bootloader, written from scratch in C and x86 assembly.

## Overview

mikrokernellib is a hobby OS kernel targeting x86-64 hardware. It boots via a hand-written BIOS bootloader and implements foundational kernel subsystems including interrupt handling, physical memory management, VESA graphics, virtual memory and paging...

## Architecture

### Boot Sequence

```
BIOS → Stage 1 (512 B) → Stage 2 (1536 B) → Kernel 
```

**Stage 1** (`src/Bootldr/mikroBootldr-stg1-x86.asm`) — 16-bit real mode, loaded at `0x7C00`:

**Stage 2** (`src/Bootldr/mikroBootldr-stg2-x86.asm`) — 16-bit real mode, loaded at `0x7E00`:
- Queries BIOS e820 memory map → stored at `0x5000`, count at `0x7000`
- Enables A20 line (BIOS → port `0x92` fallback)
- Loads a 32-bit GDT, enters protected mode
- Checks CPUID for 64-bit long mode support; halts if unavailable
- Sets up identity-mapped page tables with 2 MB huge pages
- Enables PAE + long mode (`EFER.LME`) + paging, far-jumps to the 64-bit kernel

**Kernel entry** (`src/mikroKernellib/start_kernel.c`) — 64-bit long mode:
1. Says hello
2. Initializes the IDT (CPU exceptions 0–31)
3. Initializes the physical memory allocator (buddy + slab allocators)
4. Initializes kernel paging and VM layout
5. Jumps to kernel virtual entry

**Kernel virtual entry** (`src/mikroKernellib/kmain.c`) - virtual memory on:
1. Reloads virtual IDT
2. Panics, for now

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
- Page fault handler to be implemented by VM subsystem

### Physical Memory Manager (`phys_kmalloc.c`, `mapped_phys_kmalloc.c`)

**Buddy allocator** — page-frame allocator:
- Orders 0–10: `2^order × 4 KB` = 4 KB to 4 MB per allocation
- XOR buddy addressing for O(1) buddy lookup
- Bitmap tracks per-pair allocation state
- Coalesces on free when the buddy is free
- Bootstraps from the largest contiguous e820 usable region, aligned to max order
  Buddy TODO: USE MORE THAN ONE LARGE CONTIGUOUS E820 REGION

**Slab allocator** — small object allocator:
- 8 pre-configured caches: 16, 32, 64, 128, 256, 512, 1024, 2048 bytes
- Per-cache free / partial / full slab lists
- Slab descriptor embedded at the start of each slab (backed by buddy)
- Per-object freelist threading within each slab
- New slabs via kmem_cache_create_sl

All allocator state is protected by a single interrupt-safe spinlock.
Slab TODO: Seperate pmm_lock to slab_lock and buddy_lock

### Spinlocks (`spinlocks.asm`)

- `xchg`-based test-and-set spin loop
- Saves and restores RFLAGS; disables interrupts while held
- Stores the holding CPU's APIC ID (via CPUID) for debugging

### VESA graphics:
- bootloader enables vesa graphical mode and hands off the framebuffer address and vesa-mode info to the kernel
- kernel handles: font rendering and writes to the framebuffer, larger graphics libraries are needed in the future

### Utilities

| Module | Functions |
|--------|-----------|
| `kmath.c` | `align_up`, `klog2`, `round_up_pow2`, `buddy_order` |
| `kstrings.c` | `kstring_length`, `kstring_strcpy`, `kstring_strcmp` |
| `mmu.c` | `map_page/unmap_page`, `map_pages/unmap_pages`, `map_huge_page/unmap_huge_page` (raw mapping/unmapping functions, agnostic to virtual address spaces) |
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
│       ├── start_kernel.c         # Kernel entry point
│       ├── idt.c / idt_common.asm # IDT setup and ISR stubs
│       ├── isr_dispatch.c         # Exception handler dispatch
│       ├── vesa_graphics_lib.c    # VESA output
│       ├── phys_kmalloc.c         # Buddy + slab allocator
│       ├── kmath.c                # Math utilities
│       ├── kstrings.c             # String utilities
│       ├── mmu.c                  # MMU
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
| VESA graphics output | Done |
| IDT / exception handling | Done |
| Buddy allocator | Done |
| Slab allocator | Done |
| Spinlocks | Done |
| MMU / virtual memory | In progress |
| Process/task scheduling | Not started |
| Syscall interface | Not started |

README WAS PARTLY GENERATED BY AI, CODE WAS NOT.
