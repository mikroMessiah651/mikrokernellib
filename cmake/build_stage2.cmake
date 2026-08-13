# Invoked via `cmake -P` by the stage2 build step.
#
# Computes the kernel's size in 512-byte sectors from the freshly built
# kernel.bin and assembles stage2 with -DKERNEL_SECTORS set to that exact
# count. This is what guarantees the bootloader always loads the whole kernel:
# the loader's sector count is derived from the binary, never hand-maintained.
#
# Expected -D arguments: NASM STAGE2_SRC STAGE2_BIN KERNEL_BIN VESA_WIDTH VESA_HEIGHT

if(NOT EXISTS "${KERNEL_BIN}")
    message(FATAL_ERROR "build_stage2.cmake: kernel.bin not found at ${KERNEL_BIN}")
endif()

file(SIZE "${KERNEL_BIN}" _kernel_size)
math(EXPR _kernel_sectors "(${_kernel_size} + 511) / 512")

# The LBA loader issues a single int 13h / AH=42h read. The Disk Address Packet
# sector count is a word, but real BIOSes commonly cap one call at 127 sectors.
# Past that, the read in mikroBootldr-stg2-x86.asm must be split into a loop.
if(_kernel_sectors GREATER 127)
    message(FATAL_ERROR
        "kernel.bin is ${_kernel_size} bytes (${_kernel_sectors} sectors). "
        "The stage2 loader does a single int13h read (max 127 sectors / ~64 KiB). "
        "Split the read into a loop in mikroBootldr-stg2-x86.asm before growing further.")
endif()

message(STATUS "[stage2] kernel.bin = ${_kernel_size} bytes -> loader will read ${_kernel_sectors} sectors")

execute_process(
    COMMAND ${NASM} -f bin
            -DVESA_WIDTH=${VESA_WIDTH}
            -DVESA_HEIGHT=${VESA_HEIGHT}
            -DKERNEL_SECTORS=${_kernel_sectors}
            -o ${STAGE2_BIN} ${STAGE2_SRC}
    RESULT_VARIABLE _nasm_result
)
if(NOT _nasm_result EQUAL 0)
    message(FATAL_ERROR "NASM failed assembling stage2 (exit ${_nasm_result})")
endif()
