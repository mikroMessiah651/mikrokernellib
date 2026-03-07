# cmake/toolchain-x86_64-elf.cmake
#
# Cross-compilation toolchain for bare-metal x86-64 (ELF).
# Use when you have an x86_64-elf-* or x86_64-linux-gnu-* toolchain installed.
#
# Usage:
#   cmake -DCMAKE_TOOLCHAIN_FILE=cmake/toolchain-x86_64-elf.cmake -B build
#
# If the x86_64-elf-* prefixed binaries are not in PATH, install one of:
#   - cross-compilation toolchain from your distro (e.g. gcc-x86-64-linux-gnu)
#   - OSDev cross-compiler built from source: https://wiki.osdev.org/GCC_Cross-Compiler
#   - pre-built: https://github.com/lordmilko/i686-elf-tools (x86_64 variant)

set(CMAKE_SYSTEM_NAME      Generic)
set(CMAKE_SYSTEM_PROCESSOR x86_64)

find_program(CMAKE_C_COMPILER   NAMES x86_64-elf-gcc   x86_64-linux-gnu-gcc   gcc   REQUIRED)
find_program(CMAKE_CXX_COMPILER NAMES x86_64-elf-g++   x86_64-linux-gnu-g++   g++   REQUIRED)
find_program(CMAKE_LINKER       NAMES x86_64-elf-ld    x86_64-linux-gnu-ld    ld    REQUIRED)
find_program(CMAKE_ASM_COMPILER NAMES x86_64-elf-as    x86_64-linux-gnu-as    as    REQUIRED)
find_program(CMAKE_OBJCOPY      NAMES x86_64-elf-objcopy x86_64-linux-gnu-objcopy objcopy)

# Prevent CMake from trying to link a test executable (it cannot run on the host)
set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)

# Initial flags — no hosted runtime
set(CMAKE_C_FLAGS_INIT   "-ffreestanding -nostdlib")
set(CMAKE_CXX_FLAGS_INIT "-ffreestanding -nostdlib -fno-rtti -fno-exceptions")

# Do not search for programs or libraries in host sysroot
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
