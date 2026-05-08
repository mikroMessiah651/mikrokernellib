cmake_minimum_required(VERSION 3.20)

# ─────────────────────────────────────────────────────────────────────────────
# mikrokernellib — CMake build system
#
# Disk image layout (each unit = one 512-byte sector):
#
#   LBA 0      Stage 1 bootloader  (512 bytes, ends with 0xAA55 boot signature)
#   LBA 1–3   Stage 2 bootloader  (≤ 1536 bytes / 3 sectors, loaded to 0x7E00)
#   LBA 4      Gap sector          (zeroed)
#   LBA 5+     Kernel binary       (loaded to phys 0x9000 by stage 2)
#
# Build targets
#   disk        (default) — assemble everything into disk.img
#   stage1      — build Bootldr/mikroBootldr-stg1-x86.asm → stage1.bin
#   stage2      — build Bootldr/mikroBootldr-stg2-x86.asm → stage2.bin
#   kernel      — compile + link mikroKernellib/ → kernel.bin
#   run         — launch disk.img in QEMU (requires qemu-system-x86_64)
#   run-debug   — launch with GDB stub paused on :1234
#
# Cross-compiler toolchain (optional)
#   cmake -DCMAKE_TOOLCHAIN_FILE=cmake/toolchain-x86_64-elf.cmake -B build
# ─────────────────────────────────────────────────────────────────────────────

project(mikrokernellib LANGUAGES NONE)

# Generate compile_commands.json for clangd / CLion code intelligence
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# ─── Tool detection ───────────────────────────────────────────────────────────

find_program(NASM    nasm    REQUIRED DOC "NASM assembler")
find_program(DD_TOOL dd      REQUIRED DOC "dd utility for disk image creation")

# Cross-compiler: prefer x86_64-elf-* bare-metal toolchain, fall back to host
find_program(CROSS_CC   NAMES x86_64-elf-gcc  gcc    REQUIRED DOC "C compiler")
find_program(CROSS_CXX  NAMES x86_64-elf-g++  g++    REQUIRED DOC "C++ compiler")
find_program(CROSS_LD   NAMES x86_64-elf-ld   ld     REQUIRED DOC "Linker")

find_program(CROSS_OBJCOPY NAMES x86_64-elf-objcopy objcopy REQUIRED DOC "objcopy for ELF→binary strip")
find_program(QEMU qemu-system-x86_64 DOC "QEMU x86_64 emulator")

message(STATUS "NASM  : ${NASM}")
message(STATUS "CC    : ${CROSS_CC}")
message(STATUS "CXX   : ${CROSS_CXX}")
message(STATUS "LD    : ${CROSS_LD}")
if(QEMU)
    message(STATUS "QEMU  : ${QEMU}")
else()
    message(STATUS "QEMU  : not found — run / run-debug targets disabled")
endif()

# ─── Kernel compiler / linker flags ──────────────────────────────────────────

# With -nostdinc we must explicitly point at GCC's own freestanding headers
# (stdint.h, stddef.h, …).  Ask the compiler where they live.
execute_process(
    COMMAND ${CROSS_CC} -print-file-name=include
    OUTPUT_VARIABLE _gcc_inc
    OUTPUT_STRIP_TRAILING_WHITESPACE
)

# Flags shared by C and C++
set(KERNEL_CFLAGS
    -m64
    -ffreestanding
    -fno-builtin
    -fno-stack-protector
    -fno-omit-frame-pointer
    -mno-red-zone
    -mno-sse
    -mno-sse2
    -mno-mmx
    -fno-pic
    -fno-pie
    -nostdlib
    -nostdinc
    "-isystem${_gcc_inc}"
    -g
    -O0
    -Wall
    -Wextra
)

# Extra flags added for C++ sources (no RTTI, no exceptions in kernel code)
set(KERNEL_CXXFLAGS
    ${KERNEL_CFLAGS}
    -fno-rtti
    -fno-exceptions
)

# ─── Stage 1 bootloader ───────────────────────────────────────────────────────

set(STAGE1_SRC ${CMAKE_SOURCE_DIR}/src/Bootldr/mikroBootldr-stg1-x86.asm)
set(STAGE1_BIN ${CMAKE_BINARY_DIR}/stage1.bin)

add_custom_command(
    OUTPUT  ${STAGE1_BIN}
    COMMAND ${NASM} -f bin -o ${STAGE1_BIN} ${STAGE1_SRC}
    DEPENDS ${STAGE1_SRC}
    COMMENT "[NASM] mikroBootldr-stg1-x86.asm → stage1.bin"
    VERBATIM
)
add_custom_target(stage1 DEPENDS ${STAGE1_BIN})

# ─── VBE resolution ───────────────────────────────────────────────────────────
#
# Set these to the display resolution your target machine supports.
# QEMU advertises 2560x1440; real hardware often tops out at 1920x1080 or lower.
# Change with:  cmake -DVESA_WIDTH=1920 -DVESA_HEIGHT=1080 -B build

set(VESA_WIDTH  2560 CACHE STRING "VBE framebuffer width  the bootloader will request from BIOS")
set(VESA_HEIGHT 1440 CACHE STRING "VBE framebuffer height the bootloader will request from BIOS")

message(STATUS "──────────────────────────────────────────────────────────")
message(STATUS "  Bootloader VBE resolution : ${VESA_WIDTH} x ${VESA_HEIGHT}")
message(STATUS "  (QEMU default is 2560x1440 — real hardware often needs")
message(STATUS "   1920x1080 or lower. Override with -DVESA_WIDTH=W -DVESA_HEIGHT=H)")
message(STATUS "──────────────────────────────────────────────────────────")

# ─── Stage 2 bootloader ───────────────────────────────────────────────────────

set(STAGE2_SRC ${CMAKE_SOURCE_DIR}/src/Bootldr/mikroBootldr-stg2-x86.asm)
set(STAGE2_BIN ${CMAKE_BINARY_DIR}/stage2.bin)

add_custom_command(
    OUTPUT  ${STAGE2_BIN}
    COMMAND ${NASM} -f bin
                    -DVESA_WIDTH=${VESA_WIDTH}
                    -DVESA_HEIGHT=${VESA_HEIGHT}
                    -o ${STAGE2_BIN} ${STAGE2_SRC}
    DEPENDS ${STAGE2_SRC}
    COMMENT "[NASM] mikroBootldr-stg2-x86.asm → stage2.bin  (${VESA_WIDTH}x${VESA_HEIGHT})"
    VERBATIM
)
add_custom_target(stage2 DEPENDS ${STAGE2_BIN})

# ─── Kernel ───────────────────────────────────────────────────────────────────

set(KERNEL_LD   ${CMAKE_SOURCE_DIR}/cmake/kernel.ld)
set(KERNEL_ELF  ${CMAKE_BINARY_DIR}/kernel.elf)
set(KERNEL_BIN  ${CMAKE_BINARY_DIR}/kernel.bin)
set(KERNEL_SRCDIR ${CMAKE_SOURCE_DIR}/src/mikroKernellib)

set(KERNEL_CSRCS
    ${KERNEL_SRCDIR}/init_kernellib.c
    ${KERNEL_SRCDIR}/idt.c
    ${KERNEL_SRCDIR}/isr_dispatch.c
    ${KERNEL_SRCDIR}/phys_kmalloc.c
    ${KERNEL_SRCDIR}/kmath.c
    ${KERNEL_SRCDIR}/kstrings.c
    ${KERNEL_SRCDIR}/mmu_page_tables.c
    ${KERNEL_SRCDIR}/vesa_graphics_lib.c
    ${KERNEL_SRCDIR}/vma_rb_tree.c
)

set(KERNEL_ASMSRCS
    ${KERNEL_SRCDIR}/idt_common.asm
    ${KERNEL_SRCDIR}/spinlocks.asm
    ${KERNEL_SRCDIR}/mmu_paging_x86.asm
    ${KERNEL_SRCDIR}/nasm_font.asm
)

# Compile each C source to its own .o
set(KERNEL_OBJS)
foreach(_src ${KERNEL_CSRCS})
    get_filename_component(_name ${_src} NAME_WE)
    set(_obj ${CMAKE_BINARY_DIR}/${_name}.o)
    add_custom_command(
        OUTPUT  ${_obj}
        COMMAND ${CROSS_CC} ${KERNEL_CFLAGS} -I${CMAKE_SOURCE_DIR} -c ${_src} -o ${_obj}
        DEPENDS ${_src}
        COMMENT "[CC]   ${_name}.c → ${_name}.o"
        VERBATIM
    )
    list(APPEND KERNEL_OBJS ${_obj})
endforeach()

# Assemble each ASM source to its own .o (elf64 for kernel, not flat bin)
foreach(_src ${KERNEL_ASMSRCS})
    get_filename_component(_name ${_src} NAME_WE)
    set(_obj ${CMAKE_BINARY_DIR}/${_name}.o)
    add_custom_command(
        OUTPUT  ${_obj}
        COMMAND ${NASM} -f elf64 -I${KERNEL_SRCDIR}/ -o ${_obj} ${_src}
        DEPENDS ${_src}
        COMMENT "[NASM] ${_name}.asm → ${_name}.o"
        VERBATIM
    )
    list(APPEND KERNEL_OBJS ${_obj})
endforeach()

add_custom_command(
    OUTPUT  ${KERNEL_ELF}
    COMMAND ${CROSS_LD} -T ${KERNEL_LD} ${KERNEL_OBJS} -o ${KERNEL_ELF}
    DEPENDS ${KERNEL_OBJS} ${KERNEL_LD}
    COMMENT "[LD]   kernel.elf"
    VERBATIM
)

add_custom_command(
    OUTPUT  ${KERNEL_BIN}
    COMMAND ${CROSS_OBJCOPY} -O binary ${KERNEL_ELF} ${KERNEL_BIN}
    DEPENDS ${KERNEL_ELF}
    COMMENT "[OBJCOPY] kernel.elf → kernel.bin"
    VERBATIM
)
add_custom_target(kernel DEPENDS ${KERNEL_BIN})

# ─── Disk image ───────────────────────────────────────────────────────────────

set(DISK_IMG ${CMAKE_BINARY_DIR}/disk.img)

add_custom_command(
    OUTPUT  ${DISK_IMG}
    # Blank 64-sector (32 KiB) raw image — unused sectors read as zero
    COMMAND ${DD_TOOL} if=/dev/zero of=${DISK_IMG} bs=512 count=64 status=none
    # LBA 0 — Stage 1 (512 bytes; byte 510-511 = 0xAA55 boot signature)
    COMMAND ${DD_TOOL} if=${STAGE1_BIN} of=${DISK_IMG} bs=512 seek=0  conv=notrunc status=none
    # LBA 1 — Stage 2 (spans sectors 1-3; sector 4 stays zeroed as gap)
    COMMAND ${DD_TOOL} if=${STAGE2_BIN} of=${DISK_IMG} bs=512 seek=1  conv=notrunc status=none
    # LBA 5 — Kernel (stage 2 loads this to phys 0x9000 and jumps there)
    COMMAND ${DD_TOOL} if=${KERNEL_BIN} of=${DISK_IMG} bs=512 seek=5  conv=notrunc status=none
    DEPENDS ${STAGE1_BIN} ${STAGE2_BIN} ${KERNEL_BIN}
    COMMENT "[IMG]  disk.img"
    VERBATIM
)
add_custom_target(stage2_size_reminder
    COMMAND ${CMAKE_COMMAND} -E echo "WARNING: are you sure bootloader stage 2 is updated to fit new binary's size?"
    VERBATIM
)

add_custom_target(disk ALL DEPENDS ${DISK_IMG})
add_dependencies(disk stage2_size_reminder)

# ─── QEMU run targets ─────────────────────────────────────────────────────────

if(QEMU)
    add_custom_target(run
        COMMAND ${QEMU}
            -drive format=raw,file=${DISK_IMG}
            -m 256M
            -no-reboot
            -no-shutdown
            -serial stdio
        DEPENDS disk
        USES_TERMINAL
        VERBATIM
    )

    add_custom_target(run-debug
        COMMAND ${QEMU}
            -drive format=raw,file=${DISK_IMG}
            -m 256M
            -no-reboot
            -no-shutdown
            -serial stdio
            -s -S
        DEPENDS disk
        USES_TERMINAL
        VERBATIM
    )
endif()

# ─── CLion / IDE indexing support ─────────────────────────────────────────────
#
# The targets below are EXCLUDE_FROM_ALL — they are never part of a real build.
# Their sole purpose is to put entries into compile_commands.json so that
# clangd (and CLion's built-in code intelligence) can analyse the sources with
# the correct bare-metal compiler flags.
#
# ASM files: CLion's bundled "Assembly Language Support" plugin handles Intel /
# NASM syntax highlighting automatically by file extension.  The bootldr_ide
# custom target just ensures the .asm files appear in CLion's project view.

enable_language(C CXX)

# Kernel C / C++ sources
add_library(kernel_ide OBJECT EXCLUDE_FROM_ALL
    src/mikroKernellib/init_kernellib.c
    src/mikroKernellib/idt.c
    src/mikroKernellib/isr_dispatch.c
    src/mikroKernellib/phys_kmalloc.c
    src/mikroKernellib/kmath.c
    src/mikroKernellib/kstrings.c
    src/mikroKernellib/mmu_page_tables.c
    src/mikroKernellib/vesa_graphics_lib.c
)
target_compile_options(kernel_ide PRIVATE ${KERNEL_CFLAGS})
target_include_directories(kernel_ide PRIVATE
    ${CMAKE_SOURCE_DIR}
)

set_target_properties(kernel_ide PROPERTIES
    C_STANDARD            11
    CXX_STANDARD          17
    C_STANDARD_REQUIRED   ON
    CXX_STANDARD_REQUIRED ON
)

# ASM sources (file listing only — no compilation)
add_custom_target(bootldr_ide
    SOURCES
        src/Bootldr/mikroBootldr-stg1-x86.asm
        src/Bootldr/mikroBootldr-stg2-x86.asm
        src/mikroKernellib/idt_common.asm
        src/mikroKernellib/spinlocks.asm
        src/mikroKernellib/mmu_paging_x86.asm
        src/mikroKernellib/nasm_font.asm
)
