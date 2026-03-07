/*
 * kasm-expanded.h — Inline assembly helpers for bare-metal x86-64 C code
 *
 * Provides:
 *   • asm_intel(...)   — arbitrary Intel-syntax asm without __asm__ volatile
 *   • Statement macros for no-operand instructions (use like: hlt; or if(..) { cli; })
 *   • static inline functions for instructions that take C variable operands
 *
 * Author: Eyal Kaghanovich
 */
#pragma once
#include <stdint.h>

/* =========================================================================
 * §1 GENERAL INTEL-SYNTAX ASM MACRO
 *
 * Write NASM-like assembly directly in C without __asm__ volatile.
 *
 *   asm_intel(mov rax, 0x1234)
 *   asm_intel(xor rbx, rbx)
 *   asm_intel(push rdi)
 *   if (cond) { asm_intel(mov rax, 0x000f); }
 *
 * NOTE: C variables cannot be used as operands (no GCC constraint system).
 *       Use the typed inline functions below for C-variable operands.
 *
 * asm_intel_s() accepts a raw string for multi-line sequences:
 *   asm_intel_s("xor rax, rax\n\tinc rax")
 * ========================================================================= */

#define asm_intel(...) \
    __asm__ volatile( \
        ".intel_syntax noprefix\n\t" \
        #__VA_ARGS__ \
        "\n\t.att_syntax prefix" \
    )

#define asm_intel_s(str) \
    __asm__ volatile( \
        ".intel_syntax noprefix\n\t" \
        str \
        "\n\t.att_syntax prefix" \
    )

/* Compiler-only memory barrier — no CPU instruction, prevents reordering */
#define barrier()  __asm__ volatile("" ::: "memory")

/* =========================================================================
 * §2  NO-OPERAND INSTRUCTIONS
 * ========================================================================= */

/* --- System control --- */
#define nop        __asm__ volatile("nop")
#define hlt        __asm__ volatile("hlt")
#define cli        __asm__ volatile("cli")
#define sti        __asm__ volatile("sti")
#define ud2        __asm__ volatile("ud2")       /* Undefined; raises #UD    */
#define int3       __asm__ volatile("int3")      /* Breakpoint trap          */
#define pause_cpu  __asm__ volatile("pause")     /* Spin-loop hint (HT-safe) */
#define cpu_relax  pause_cpu

/* --- Memory ordering --- */
#define mfence  __asm__ volatile("mfence" ::: "memory")
#define sfence  __asm__ volatile("sfence" ::: "memory")
#define lfence  __asm__ volatile("lfence" ::: "memory")
#define wbinvd  __asm__ volatile("wbinvd")      /* Write-back + invalidate  */
#define invd    __asm__ volatile("invd")         /* Invalidate cache (no WB) */

/* --- Flag register ops --- */
#define clc      __asm__ volatile("clc")         /* CF ← 0                  */
#define stc      __asm__ volatile("stc")         /* CF ← 1                  */
#define cmc      __asm__ volatile("cmc")         /* CF ← ~CF                */
#define cld      __asm__ volatile("cld")         /* DF ← 0 (string forward) */
#define std_asm  __asm__ volatile("std")         /* DF ← 1 (avoid C++ std)  */
#define pushfq   __asm__ volatile("pushfq")
#define popfq    __asm__ volatile("popfq")
#define lahf     __asm__ volatile("lahf")        /* AH ← EFLAGS[7:0]       */
#define sahf     __asm__ volatile("sahf")        /* EFLAGS[7:0] ← AH       */

/* --- Control transfer --- */
#define leave_asm  __asm__ volatile("leave")     /* RSP←RBP; pop RBP       */
#define iretq  __asm__ volatile("iretq")     /* Interrupt return 64-bit */
#define swapgs     __asm__ volatile("swapgs")    /* GS ↔ KernelGSBase      */
#define sysret64   __asm__ volatile("sysretq")
#define syscall_asm __asm__ volatile("syscall" ::: "rcx", "r11", "memory")

/* --- Sign-extend --- */
#define cbw   __asm__ volatile("cbw")   /* AL  → AX       */
#define cwde  __asm__ volatile("cwde")  /* AX  → EAX      */
#define cdqe  __asm__ volatile("cdqe")  /* EAX → RAX      */
#define cwd   __asm__ volatile("cwd")   /* AX  → DX:AX    */
#define cdq   __asm__ volatile("cdq")   /* EAX → EDX:EAX  */
#define cqo   __asm__ volatile("cqo")   /* RAX → RDX:RAX  */

/* --- Misc --- */
#define xlatb  __asm__ volatile("xlatb")

/* =========================================================================
 * §3  STRING / REPEAT INSTRUCTIONS
 *     Set RCX/RSI/RDI manually before using these (or via asm_intel).
 * ========================================================================= */

#define stosb       __asm__ volatile("stosb")
#define stosw       __asm__ volatile("stosw")
#define stosd       __asm__ volatile("stosd")
#define stosq       __asm__ volatile("stosq")
#define lodsb       __asm__ volatile("lodsb")
#define lodsw       __asm__ volatile("lodsw")
#define lodsd       __asm__ volatile("lodsd")
#define lodsq       __asm__ volatile("lodsq")
#define movsb_str   __asm__ volatile("movsb")
#define movsw_str   __asm__ volatile("movsw")
#define movsd_str   __asm__ volatile("movsd")  /* _str suffix avoids SSE name */
#define movsq_str   __asm__ volatile("movsq")
#define scasb       __asm__ volatile("scasb")
#define scasw       __asm__ volatile("scasw")
#define scasd       __asm__ volatile("scasd")
#define scasq       __asm__ volatile("scasq")

#define rep_stosb    __asm__ volatile("rep stosb")
#define rep_stosd    __asm__ volatile("rep stosd")
#define rep_stosq    __asm__ volatile("rep stosq")
#define rep_movsb    __asm__ volatile("rep movsb")
#define rep_movsq    __asm__ volatile("rep movsq")
#define repe_scasb   __asm__ volatile("repe scasb")
#define repe_scasq   __asm__ volatile("repe scasq")
#define repne_scasb  __asm__ volatile("repne scasb")
#define repne_scasq  __asm__ volatile("repne scasq")

/* =========================================================================
 * §4  SOFTWARE INTERRUPT
 *     n must be a compile-time constant (0–255).
 * ========================================================================= */

#define int_n(n)  __asm__ volatile("int %0" :: "N"(n))

/* =========================================================================
 * §5  GENERAL-PURPOSE REGISTER READ / WRITE
 * ========================================================================= */

static inline uint64_t read_rax(void) { uint64_t v; __asm__ volatile("mov %%rax,%0":"=r"(v)); return v; }
static inline uint64_t read_rbx(void) { uint64_t v; __asm__ volatile("mov %%rbx,%0":"=r"(v)); return v; }
static inline uint64_t read_rcx(void) { uint64_t v; __asm__ volatile("mov %%rcx,%0":"=r"(v)); return v; }
static inline uint64_t read_rdx(void) { uint64_t v; __asm__ volatile("mov %%rdx,%0":"=r"(v)); return v; }
static inline uint64_t read_rsi(void) { uint64_t v; __asm__ volatile("mov %%rsi,%0":"=r"(v)); return v; }
static inline uint64_t read_rdi(void) { uint64_t v; __asm__ volatile("mov %%rdi,%0":"=r"(v)); return v; }
static inline uint64_t read_rsp(void) { uint64_t v; __asm__ volatile("mov %%rsp,%0":"=r"(v)); return v; }
static inline uint64_t read_rbp(void) { uint64_t v; __asm__ volatile("mov %%rbp,%0":"=r"(v)); return v; }
static inline uint64_t read_r8 (void) { uint64_t v; __asm__ volatile("mov %%r8, %0":"=r"(v)); return v; }
static inline uint64_t read_r9 (void) { uint64_t v; __asm__ volatile("mov %%r9, %0":"=r"(v)); return v; }
static inline uint64_t read_r10(void) { uint64_t v; __asm__ volatile("mov %%r10,%0":"=r"(v)); return v; }
static inline uint64_t read_r11(void) { uint64_t v; __asm__ volatile("mov %%r11,%0":"=r"(v)); return v; }
static inline uint64_t read_r12(void) { uint64_t v; __asm__ volatile("mov %%r12,%0":"=r"(v)); return v; }
static inline uint64_t read_r13(void) { uint64_t v; __asm__ volatile("mov %%r13,%0":"=r"(v)); return v; }
static inline uint64_t read_r14(void) { uint64_t v; __asm__ volatile("mov %%r14,%0":"=r"(v)); return v; }
static inline uint64_t read_r15(void) { uint64_t v; __asm__ volatile("mov %%r15,%0":"=r"(v)); return v; }
static inline uint64_t read_rip(void) { uint64_t v; __asm__ volatile("lea 0(%%rip),%0":"=r"(v)); return v; }

static inline void write_rax(uint64_t v) { __asm__ volatile("mov %0,%%rax"::"r"(v):"rax"); }
static inline void write_rbx(uint64_t v) { __asm__ volatile("mov %0,%%rbx"::"r"(v):"rbx"); }
static inline void write_rcx(uint64_t v) { __asm__ volatile("mov %0,%%rcx"::"r"(v):"rcx"); }
static inline void write_rdx(uint64_t v) { __asm__ volatile("mov %0,%%rdx"::"r"(v):"rdx"); }
static inline void write_rsi(uint64_t v) { __asm__ volatile("mov %0,%%rsi"::"r"(v):"rsi"); }
static inline void write_rdi(uint64_t v) { __asm__ volatile("mov %0,%%rdi"::"r"(v):"rdi"); }
static inline void write_r8 (uint64_t v) { __asm__ volatile("mov %0,%%r8" ::"r"(v):"r8");  }
static inline void write_r9 (uint64_t v) { __asm__ volatile("mov %0,%%r9" ::"r"(v):"r9");  }
static inline void write_r10(uint64_t v) { __asm__ volatile("mov %0,%%r10"::"r"(v):"r10"); }
static inline void write_r11(uint64_t v) { __asm__ volatile("mov %0,%%r11"::"r"(v):"r11"); }
static inline void write_r12(uint64_t v) { __asm__ volatile("mov %0,%%r12"::"r"(v):"r12"); }
static inline void write_r13(uint64_t v) { __asm__ volatile("mov %0,%%r13"::"r"(v):"r13"); }
static inline void write_r14(uint64_t v) { __asm__ volatile("mov %0,%%r14"::"r"(v):"r14"); }
static inline void write_r15(uint64_t v) { __asm__ volatile("mov %0,%%r15"::"r"(v):"r15"); }

/* =========================================================================
 * §6  PORT I/O
 * ========================================================================= */

static inline uint8_t  inb(uint16_t port) { uint8_t  v; __asm__ volatile("inb %1,%0":"=a"(v):"Nd"(port)); return v; }
static inline uint16_t inw(uint16_t port) { uint16_t v; __asm__ volatile("inw %1,%0":"=a"(v):"Nd"(port)); return v; }
static inline uint32_t inl(uint16_t port) { uint32_t v; __asm__ volatile("inl %1,%0":"=a"(v):"Nd"(port)); return v; }

static inline void outb(uint16_t port, uint8_t  v) { __asm__ volatile("outb %0,%1"::"a"(v),"Nd"(port)); }
static inline void outw(uint16_t port, uint16_t v) { __asm__ volatile("outw %0,%1"::"a"(v),"Nd"(port)); }
static inline void outl(uint16_t port, uint32_t v) { __asm__ volatile("outl %0,%1"::"a"(v),"Nd"(port)); }

/* Small I/O delay via an unused port write */
static inline void io_wait(void) { outb(0x80, 0x00); }

/* =========================================================================
 * §7  CONTROL REGISTERS
 * ========================================================================= */

static inline uint64_t read_cr0(void) { uint64_t v; __asm__ volatile("mov %%cr0,%0":"=r"(v)); return v; }
static inline uint64_t read_cr2(void) { uint64_t v; __asm__ volatile("mov %%cr2,%0":"=r"(v)); return v; }
static inline uint64_t read_cr3(void) { uint64_t v; __asm__ volatile("mov %%cr3,%0":"=r"(v)); return v; }
static inline uint64_t read_cr4(void) { uint64_t v; __asm__ volatile("mov %%cr4,%0":"=r"(v)); return v; }
static inline uint64_t read_cr8(void) { uint64_t v; __asm__ volatile("mov %%cr8,%0":"=r"(v)); return v; }

static inline void write_cr0(uint64_t v) { __asm__ volatile("mov %0,%%cr0"::"r"(v)); }
static inline void write_cr3(uint64_t v) { __asm__ volatile("mov %0,%%cr3"::"r"(v):"memory"); }
static inline void write_cr4(uint64_t v) { __asm__ volatile("mov %0,%%cr4"::"r"(v)); }
static inline void write_cr8(uint64_t v) { __asm__ volatile("mov %0,%%cr8"::"r"(v)); }

/* CR0 bits */
#define CR0_PE  (1ULL <<  0)  /* Protected mode enable  */
#define CR0_MP  (1ULL <<  1)  /* Monitor co-processor   */
#define CR0_EM  (1ULL <<  2)  /* x87 emulation          */
#define CR0_TS  (1ULL <<  3)  /* Task switched          */
#define CR0_NE  (1ULL <<  5)  /* Numeric error          */
#define CR0_WP  (1ULL << 16)  /* Write protect          */
#define CR0_AM  (1ULL << 18)  /* Alignment mask         */
#define CR0_NW  (1ULL << 29)  /* Not-write through      */
#define CR0_CD  (1ULL << 30)  /* Cache disable          */
#define CR0_PG  (1ULL << 31)  /* Paging                 */

/* CR4 bits */
#define CR4_VME        (1ULL <<  0)
#define CR4_PVI        (1ULL <<  1)
#define CR4_TSD        (1ULL <<  2)
#define CR4_DE         (1ULL <<  3)
#define CR4_PSE        (1ULL <<  4)
#define CR4_PAE        (1ULL <<  5)
#define CR4_MCE        (1ULL <<  6)
#define CR4_PGE        (1ULL <<  7)
#define CR4_OSFXSR     (1ULL <<  9)
#define CR4_OSXMMEXCPT (1ULL << 10)
#define CR4_UMIP       (1ULL << 11)
#define CR4_VMXE       (1ULL << 13)
#define CR4_FSGSBASE   (1ULL << 16)
#define CR4_PCIDE      (1ULL << 17)
#define CR4_OSXSAVE    (1ULL << 18)
#define CR4_SMEP       (1ULL << 20)
#define CR4_SMAP       (1ULL << 21)

/* =========================================================================
 * §8  DEBUG REGISTERS
 * ========================================================================= */

static inline uint64_t read_dr0(void) { uint64_t v; __asm__ volatile("mov %%dr0,%0":"=r"(v)); return v; }
static inline uint64_t read_dr1(void) { uint64_t v; __asm__ volatile("mov %%dr1,%0":"=r"(v)); return v; }
static inline uint64_t read_dr2(void) { uint64_t v; __asm__ volatile("mov %%dr2,%0":"=r"(v)); return v; }
static inline uint64_t read_dr3(void) { uint64_t v; __asm__ volatile("mov %%dr3,%0":"=r"(v)); return v; }
static inline uint64_t read_dr6(void) { uint64_t v; __asm__ volatile("mov %%dr6,%0":"=r"(v)); return v; }
static inline uint64_t read_dr7(void) { uint64_t v; __asm__ volatile("mov %%dr7,%0":"=r"(v)); return v; }

static inline void write_dr0(uint64_t v) { __asm__ volatile("mov %0,%%dr0"::"r"(v)); }
static inline void write_dr1(uint64_t v) { __asm__ volatile("mov %0,%%dr1"::"r"(v)); }
static inline void write_dr2(uint64_t v) { __asm__ volatile("mov %0,%%dr2"::"r"(v)); }
static inline void write_dr3(uint64_t v) { __asm__ volatile("mov %0,%%dr3"::"r"(v)); }
static inline void write_dr6(uint64_t v) { __asm__ volatile("mov %0,%%dr6"::"r"(v)); }
static inline void write_dr7(uint64_t v) { __asm__ volatile("mov %0,%%dr7"::"r"(v)); }

/* =========================================================================
 * §9  SEGMENT REGISTERS
 * ========================================================================= */

static inline uint16_t read_cs(void) { uint16_t v; __asm__ volatile("mov %%cs,%0":"=r"(v)); return v; }
static inline uint16_t read_ds(void) { uint16_t v; __asm__ volatile("mov %%ds,%0":"=r"(v)); return v; }
static inline uint16_t read_es(void) { uint16_t v; __asm__ volatile("mov %%es,%0":"=r"(v)); return v; }
static inline uint16_t read_fs(void) { uint16_t v; __asm__ volatile("mov %%fs,%0":"=r"(v)); return v; }
static inline uint16_t read_gs(void) { uint16_t v; __asm__ volatile("mov %%gs,%0":"=r"(v)); return v; }
static inline uint16_t read_ss(void) { uint16_t v; __asm__ volatile("mov %%ss,%0":"=r"(v)); return v; }

static inline void write_ds(uint16_t v) { __asm__ volatile("movl %0,%%ds"::"r"((uint32_t)v)); }
static inline void write_es(uint16_t v) { __asm__ volatile("movl %0,%%es"::"r"((uint32_t)v)); }
static inline void write_fs(uint16_t v) { __asm__ volatile("movl %0,%%fs"::"r"((uint32_t)v)); }
static inline void write_gs(uint16_t v) { __asm__ volatile("movl %0,%%gs"::"r"((uint32_t)v)); }
static inline void write_ss(uint16_t v) { __asm__ volatile("movl %0,%%ss"::"r"((uint32_t)v)); }

/* FS/GS base via RDFSBASE/WRFSBASE (requires CR4.FSGSBASE) */
static inline uint64_t read_fsbase(void) { uint64_t v; __asm__ volatile("rdfsbase %0":"=r"(v)); return v; }
static inline uint64_t read_gsbase(void) { uint64_t v; __asm__ volatile("rdgsbase %0":"=r"(v)); return v; }
static inline void write_fsbase(uint64_t v) { __asm__ volatile("wrfsbase %0"::"r"(v)); }
static inline void write_gsbase(uint64_t v) { __asm__ volatile("wrgsbase %0"::"r"(v)); }

/* =========================================================================
 * §10  RFLAGS / EFLAGS
 * ========================================================================= */

static inline uint64_t read_rflags(void) {
    uint64_t v;
    __asm__ volatile("pushfq; popq %0" : "=r"(v));
    return v;
}
static inline void write_rflags(uint64_t v) {
    __asm__ volatile("pushq %0; popfq" :: "r"(v) : "cc", "memory");
}
static inline uint32_t read_eflags(void) {
    uint32_t v;
    __asm__ volatile("pushfl; popl %0" : "=r"(v));
    return v;
}

/* RFLAGS bit definitions */
#define RFLAGS_CF   (1ULL <<  0)  /* Carry              */
#define RFLAGS_PF   (1ULL <<  2)  /* Parity             */
#define RFLAGS_AF   (1ULL <<  4)  /* Adjust             */
#define RFLAGS_ZF   (1ULL <<  6)  /* Zero               */
#define RFLAGS_SF   (1ULL <<  7)  /* Sign               */
#define RFLAGS_TF   (1ULL <<  8)  /* Trap               */
#define RFLAGS_IF   (1ULL <<  9)  /* Interrupt enable   */
#define RFLAGS_DF   (1ULL << 10)  /* Direction          */
#define RFLAGS_OF   (1ULL << 11)  /* Overflow           */
#define RFLAGS_IOPL (3ULL << 12)  /* I/O privilege      */
#define RFLAGS_NT   (1ULL << 14)  /* Nested task        */
#define RFLAGS_RF   (1ULL << 16)  /* Resume             */
#define RFLAGS_VM   (1ULL << 17)  /* Virtual 8086       */
#define RFLAGS_AC   (1ULL << 18)  /* Alignment check    */
#define RFLAGS_VIF  (1ULL << 19)  /* Virtual interrupt  */
#define RFLAGS_VIP  (1ULL << 20)  /* Virtual int pend.  */
#define RFLAGS_ID   (1ULL << 21)  /* CPUID usable       */

/* =========================================================================
 * §11  MODEL-SPECIFIC REGISTERS (MSR)
 * ========================================================================= */

static inline uint64_t rdmsr(uint32_t msr) {
    uint32_t lo, hi;
    __asm__ volatile("rdmsr" : "=a"(lo), "=d"(hi) : "c"(msr));
    return ((uint64_t)hi << 32) | lo;
}
static inline void wrmsr(uint32_t msr, uint64_t val) {
    __asm__ volatile("wrmsr" ::
        "a"((uint32_t)val),
        "d"((uint32_t)(val >> 32)),
        "c"(msr));
}

/* Common MSR addresses */
#define MSR_APIC_BASE       0x0000001BU
#define MSR_TSC             0x00000010U
#define MSR_PAT             0x00000277U
#define MSR_MTRR_DEF_TYPE   0x000002FFU
#define MSR_EFER            0xC0000080U
#define MSR_STAR            0xC0000081U
#define MSR_LSTAR           0xC0000082U
#define MSR_CSTAR           0xC0000083U
#define MSR_SFMASK          0xC0000084U
#define MSR_FS_BASE         0xC0000100U
#define MSR_GS_BASE         0xC0000101U
#define MSR_KERNEL_GS_BASE  0xC0000102U

/* EFER bits */
#define EFER_SCE   (1ULL <<  0)  /* SYSCALL enable    */
#define EFER_LME   (1ULL <<  8)  /* Long mode enable  */
#define EFER_LMA   (1ULL << 10)  /* Long mode active  */
#define EFER_NXE   (1ULL << 11)  /* No-execute enable */

/* =========================================================================
 * §12  CPUID
 * ========================================================================= */

typedef struct { uint32_t eax, ebx, ecx, edx; } cpuid_result_t;

static inline cpuid_result_t cpuid(uint32_t leaf) {
    cpuid_result_t r;
    __asm__ volatile("cpuid"
        : "=a"(r.eax), "=b"(r.ebx), "=c"(r.ecx), "=d"(r.edx)
        : "a"(leaf), "c"(0));
    return r;
}
static inline cpuid_result_t cpuid_ex(uint32_t leaf, uint32_t subleaf) {
    cpuid_result_t r;
    __asm__ volatile("cpuid"
        : "=a"(r.eax), "=b"(r.ebx), "=c"(r.ecx), "=d"(r.edx)
        : "a"(leaf), "c"(subleaf));
    return r;
}

/* Common CPUID leaves */
#define CPUID_VENDOR        0x00000000U
#define CPUID_FEATURES      0x00000001U
#define CPUID_EXT_FEATURES  0x00000007U
#define CPUID_EXT_INFO      0x80000000U
#define CPUID_EXT_BRAND0    0x80000002U
#define CPUID_EXT_BRAND1    0x80000003U
#define CPUID_EXT_BRAND2    0x80000004U
#define CPUID_EXT_ADDR      0x80000008U

/* =========================================================================
 * §13  TIMESTAMP COUNTER
 * ========================================================================= */

static inline uint64_t rdtsc(void) {
    uint32_t lo, hi;
    __asm__ volatile("rdtsc" : "=a"(lo), "=d"(hi));
    return ((uint64_t)hi << 32) | lo;
}
/* rdtscp also stores IA32_TSC_AUX (CPU id) into *aux_out (may be NULL) */
static inline uint64_t rdtscp(uint32_t *aux_out) {
    uint32_t lo, hi, aux;
    __asm__ volatile("rdtscp" : "=a"(lo), "=d"(hi), "=c"(aux));
    if (aux_out) *aux_out = aux;
    return ((uint64_t)hi << 32) | lo;
}

/* =========================================================================
 * §14  DESCRIPTOR TABLES  (LIDT / LGDT / LLDT / LTR / STR / SLDT)
 * ========================================================================= */

/* 10-byte packed descriptor-table pointer used by LIDT/LGDT */
typedef struct __attribute__((packed)) {
    uint16_t limit;
    uint64_t base;
} desc_ptr_t;

static inline void lidt(const desc_ptr_t *p) { __asm__ volatile("lidt (%0)"::"r"(p):"memory"); }
static inline void lgdt(const desc_ptr_t *p) { __asm__ volatile("lgdt (%0)"::"r"(p):"memory"); }
static inline void sidt(desc_ptr_t *p)        { __asm__ volatile("sidt (%0)"::"r"(p):"memory"); }
static inline void sgdt(desc_ptr_t *p)        { __asm__ volatile("sgdt (%0)"::"r"(p):"memory"); }

static inline void lldt(uint16_t sel) { __asm__ volatile("lldt %0"::"rm"(sel)); }
static inline void ltr (uint16_t sel) { __asm__ volatile("ltr  %0"::"rm"(sel)); }

static inline uint16_t sldt_r(void) { uint16_t v; __asm__ volatile("sldt %0":"=rm"(v)); return v; }
static inline uint16_t str_r (void) { uint16_t v; __asm__ volatile("str  %0":"=rm"(v)); return v; }

/* =========================================================================
 * §15  TLB / CACHE MANAGEMENT
 * ========================================================================= */

/* Invalidate a single page's TLB entry */
static inline void invlpg(void *addr) {
    __asm__ volatile("invlpg (%0)"::"r"(addr):"memory");
}
/* Flush entire TLB by reloading CR3 */
static inline void flush_tlb(void) { write_cr3(read_cr3()); }

/* Cache-line flush instructions */
static inline void clflush   (void *a) { __asm__ volatile("clflush    (%0)"::"r"(a):"memory"); }
static inline void clflushopt(void *a) { __asm__ volatile("clflushopt (%0)"::"r"(a):"memory"); }
static inline void clwb      (void *a) { __asm__ volatile("clwb       (%0)"::"r"(a):"memory"); }

/* Software prefetch hints */
static inline void prefetcht0 (const void *a) { __asm__ volatile("prefetcht0  (%0)"::"r"(a)); }
static inline void prefetcht1 (const void *a) { __asm__ volatile("prefetcht1  (%0)"::"r"(a)); }
static inline void prefetcht2 (const void *a) { __asm__ volatile("prefetcht2  (%0)"::"r"(a)); }
static inline void prefetchnta(const void *a) { __asm__ volatile("prefetchnta (%0)"::"r"(a)); }

/* =========================================================================
 * §16  ATOMIC / LOCKING OPERATIONS
 * ========================================================================= */

/* Atomic exchange — returns old value */
static inline uint64_t xchg64(uint64_t *addr, uint64_t val) {
    __asm__ volatile("xchg %0,%1" : "+m"(*addr), "+r"(val) :: "memory"); return val;
}
static inline uint32_t xchg32(uint32_t *addr, uint32_t val) {
    __asm__ volatile("xchg %0,%1" : "+m"(*addr), "+r"(val) :: "memory"); return val;
}
static inline uint8_t xchg8(uint8_t *addr, uint8_t val) {
    __asm__ volatile("xchg %0,%1" : "+m"(*addr), "+r"(val) :: "memory"); return val;
}

/* Atomic compare-and-swap — returns old value at *addr */
static inline uint64_t cmpxchg64(uint64_t *addr, uint64_t expected, uint64_t desired) {
    __asm__ volatile("lock cmpxchg %2,%0"
        : "+m"(*addr), "+a"(expected) : "r"(desired) : "memory", "cc");
    return expected;
}
static inline uint32_t cmpxchg32(uint32_t *addr, uint32_t expected, uint32_t desired) {
    __asm__ volatile("lock cmpxchg %2,%0"
        : "+m"(*addr), "+a"(expected) : "r"(desired) : "memory", "cc");
    return expected;
}

/* Atomic fetch-and-add — returns old value */
static inline uint64_t xadd64(uint64_t *addr, uint64_t inc) {
    __asm__ volatile("lock xadd %0,%1" : "+r"(inc), "+m"(*addr) :: "memory"); return inc;
}
static inline uint32_t xadd32(uint32_t *addr, uint32_t inc) {
    __asm__ volatile("lock xadd %0,%1" : "+r"(inc), "+m"(*addr) :: "memory"); return inc;
}

/* Atomic bit test-and-set/reset/complement — returns old bit value */
static inline int lock_bts64(uint64_t *base, uint64_t bit) {
    int c; __asm__ volatile("lock bts %2,%1; setc %b0"
        : "=r"(c), "+m"(*base) : "r"(bit) : "memory", "cc"); return c;
}
static inline int lock_btr64(uint64_t *base, uint64_t bit) {
    int c; __asm__ volatile("lock btr %2,%1; setc %b0"
        : "=r"(c), "+m"(*base) : "r"(bit) : "memory", "cc"); return c;
}
static inline int lock_btc64(uint64_t *base, uint64_t bit) {
    int c; __asm__ volatile("lock btc %2,%1; setc %b0"
        : "=r"(c), "+m"(*base) : "r"(bit) : "memory", "cc"); return c;
}

/* =========================================================================
 * §17  BIT MANIPULATION
 * ========================================================================= */

/* Bit scan forward/reverse — return index of lowest/highest set bit */
static inline int bsf64(uint64_t v) { int r; __asm__ volatile("bsf  %1,%0":"=r"(r):"r"(v):"cc"); return r; }
static inline int bsr64(uint64_t v) { int r; __asm__ volatile("bsr  %1,%0":"=r"(r):"r"(v):"cc"); return r; }
static inline int bsf32(uint32_t v) { int r; __asm__ volatile("bsfl %1,%0":"=r"(r):"r"(v):"cc"); return r; }
static inline int bsr32(uint32_t v) { int r; __asm__ volatile("bsrl %1,%0":"=r"(r):"r"(v):"cc"); return r; }

/* Bit test (non-atomic) — returns old bit value */
static inline int bt64 (const uint64_t *b, uint64_t bit) { int c; __asm__ volatile("bt  %2,%1; setc %b0":"=r"(c):"m"(*b),"r"(bit):"cc"); return c; }
static inline int bts64(      uint64_t *b, uint64_t bit) { int c; __asm__ volatile("bts %2,%1; setc %b0":"=r"(c),"+m"(*b):"r"(bit):"cc"); return c; }
static inline int btr64(      uint64_t *b, uint64_t bit) { int c; __asm__ volatile("btr %2,%1; setc %b0":"=r"(c),"+m"(*b):"r"(bit):"cc"); return c; }
static inline int btc64(      uint64_t *b, uint64_t bit) { int c; __asm__ volatile("btc %2,%1; setc %b0":"=r"(c),"+m"(*b):"r"(bit):"cc"); return c; }

/* Population count (requires POPCNT feature) */
static inline int popcnt64(uint64_t v) { uint64_t r; __asm__ volatile("popcntq %1,%0":"=r"(r):"r"(v)); return (int)r; }
static inline int popcnt32(uint32_t v) { uint32_t r; __asm__ volatile("popcntl %1,%0":"=r"(r):"r"(v)); return (int)r; }

/* Leading/trailing zero count (requires LZCNT/TZCNT — BMI1) */
static inline int lzcnt64(uint64_t v) { uint64_t r; __asm__ volatile("lzcntq %1,%0":"=r"(r):"r"(v):"cc"); return (int)r; }
static inline int lzcnt32(uint32_t v) { uint32_t r; __asm__ volatile("lzcntl %1,%0":"=r"(r):"r"(v):"cc"); return (int)r; }
static inline int tzcnt64(uint64_t v) { uint64_t r; __asm__ volatile("tzcntq %1,%0":"=r"(r):"r"(v):"cc"); return (int)r; }
static inline int tzcnt32(uint32_t v) { uint32_t r; __asm__ volatile("tzcntl %1,%0":"=r"(r):"r"(v):"cc"); return (int)r; }

/* Byte swap */
static inline uint64_t bswap64(uint64_t v) { __asm__ volatile("bswapq %0":"=r"(v):"0"(v)); return v; }
static inline uint32_t bswap32(uint32_t v) { __asm__ volatile("bswapl %0":"=r"(v):"0"(v)); return v; }

/* Rotate left / right */
static inline uint64_t rol64(uint64_t v, uint8_t n) { __asm__ volatile("rolq %%cl,%0":"+r"(v):"c"(n):"cc"); return v; }
static inline uint64_t ror64(uint64_t v, uint8_t n) { __asm__ volatile("rorq %%cl,%0":"+r"(v):"c"(n):"cc"); return v; }
static inline uint32_t rol32(uint32_t v, uint8_t n) { __asm__ volatile("roll %%cl,%0":"+r"(v):"c"(n):"cc"); return v; }
static inline uint32_t ror32(uint32_t v, uint8_t n) { __asm__ volatile("rorl %%cl,%0":"+r"(v):"c"(n):"cc"); return v; }
static inline uint16_t rol16(uint16_t v, uint8_t n) { __asm__ volatile("rolw %%cl,%0":"+r"(v):"c"(n):"cc"); return v; }
static inline uint16_t ror16(uint16_t v, uint8_t n) { __asm__ volatile("rorw %%cl,%0":"+r"(v):"c"(n):"cc"); return v; }
static inline uint8_t  rol8 (uint8_t  v, uint8_t n) { __asm__ volatile("rolb %%cl,%0":"+r"(v):"c"(n):"cc"); return v; }
static inline uint8_t  ror8 (uint8_t  v, uint8_t n) { __asm__ volatile("rorb %%cl,%0":"+r"(v):"c"(n):"cc"); return v; }

/* =========================================================================
 * §18  XSAVE / XRSTOR / XGETBV / XSETBV
 * ========================================================================= */

static inline uint64_t xgetbv(uint32_t xcr) {
    uint32_t lo, hi;
    __asm__ volatile("xgetbv" : "=a"(lo), "=d"(hi) : "c"(xcr));
    return ((uint64_t)hi << 32) | lo;
}
static inline void xsetbv(uint32_t xcr, uint64_t val) {
    __asm__ volatile("xsetbv" :: "c"(xcr), "a"((uint32_t)val), "d"((uint32_t)(val >> 32)));
}
static inline void xsave(void *area, uint64_t mask) {
    __asm__ volatile("xsave (%0)" ::
        "r"(area), "a"((uint32_t)mask), "d"((uint32_t)(mask >> 32)) : "memory");
}
static inline void xrstor(const void *area, uint64_t mask) {
    __asm__ volatile("xrstor (%0)" ::
        "r"(area), "a"((uint32_t)mask), "d"((uint32_t)(mask >> 32)) : "memory");
}

/* =========================================================================
 * §19  POWER MANAGEMENT  (MONITOR / MWAIT)
 * ========================================================================= */

static inline void cpu_monitor(void *addr, uint32_t ext, uint32_t hint) {
    __asm__ volatile("monitor" :: "a"(addr), "c"(ext), "d"(hint));
}
static inline void cpu_mwait(uint32_t ext, uint32_t hint) {
    __asm__ volatile("mwait" :: "c"(ext), "a"(hint));
}