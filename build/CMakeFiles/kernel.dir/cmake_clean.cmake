file(REMOVE_RECURSE
  "CMakeFiles/kernel"
  "ekInitKernellib.o"
  "idt.o"
  "idt_common.o"
  "isr_dispatch.o"
  "kernel.bin"
  "phys_kmalloc.o"
  "vga-graphics.o"
)

# Per-language clean rules from dependency scanning.
foreach(lang )
  include(CMakeFiles/kernel.dir/cmake_clean_${lang}.cmake OPTIONAL)
endforeach()
