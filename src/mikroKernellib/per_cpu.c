/* Eyal Kaghanovich
 * per-cpu state management
 * 19/07/26
 */

#include "include/per_cpu.h"
#include "include/kstrings.h"
#include "include/mapped_phys_kmalloc.h"
#include "include/mikroKernellib-common.h"
#include "include/mmu.h"
#include "include/phys_kmalloc.h"


extern void write_gs_base(uint64_t cpu_local_paddr);
extern void write_kernel_gs_base(uint64_t cpu_local_paddr);
extern void swapgs();

extern uint32_t get_cpu_id();
extern struct cpu_local* get_cpu_local_gs();

extern bool supports_fsgsbase();
extern void enable_fsgsbase();

static kmem_cache cpu_local_kmem_cache;

void percpu_init() {
    if (!supports_fsgsbase()) {
        // for now we panic, someday change the system to read fs/gs with different instructions
        PANIC("FSGSBASE hardware extension not supported");
    }
    enable_fsgsbase();

    kmem_cache_create_sl(&cpu_local_kmem_cache, sizeof(struct cpu_local));

    struct cpu_local* cpu_local = mapped_kmem_cache_kalloc(&cpu_local_kmem_cache);
    if (cpu_local == NULL) {
        PANIC("allocator failure: cant allocate BSP cpu_local data");
    }
    memset(cpu_local, 0, sizeof(struct cpu_local));

    // write the physical address of cpu_local to gs
    uint64_t paddr = (uint64_t)cpu_local - DIRECT_MAP_START;
    write_kernel_gs_base(paddr);
    write_gs_base(0); // zero the IA32_GS_BASE msr
    swapgs();
}

void percpu_init_ap() {
    enable_fsgsbase();

    struct cpu_local* cpu_local = mapped_kmem_cache_kalloc(&cpu_local_kmem_cache);
    if (cpu_local == NULL) {
        // should we panic or continue boot with 1 or potentially multiple less cores?
        // need to mark that CPU as offline, not schedule anything to it,
        // handle its APIC correctly, a non-trivial amount of bookkeeping
        PANIC("allocator failure: cant allocate cpu_local data");
    }
    memset(cpu_local, 0, sizeof(struct cpu_local));
    cpu_local->cpu_id = get_cpu_id();

    uint64_t paddr = (uint64_t)cpu_local - DIRECT_MAP_START;
    write_kernel_gs_base(paddr);
    swapgs();
}

inline struct cpu_local* get_cpu_local() {
    // read gs and cast to struct cpu_local*
    return get_cpu_local_gs();
}

void cpu_set_current(struct task_struct* task) {
    struct cpu_local* cpu_local = get_cpu_local();
    cpu_local->current = task;
}

struct task_struct* cpu_get_current_task() {
    const struct cpu_local* cpu_local = get_cpu_local();
    return cpu_local->current;
}
