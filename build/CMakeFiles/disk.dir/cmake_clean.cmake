file(REMOVE_RECURSE
  "CMakeFiles/disk"
  "disk.img"
  "ekInitKernellib.o"
  "idt.o"
  "idt_common.o"
  "isr_dispatch.o"
  "kernel.bin"
  "phys_kmalloc.o"
  "stage1.bin"
  "stage2.bin"
  "vga-graphics.o"
)

# Per-language clean rules from dependency scanning.
foreach(lang )
  include(CMakeFiles/disk.dir/cmake_clean_${lang}.cmake OPTIONAL)
endforeach()
