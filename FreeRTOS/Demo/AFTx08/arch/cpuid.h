#ifndef ARCH_CPUID_H
#define ARCH_CPUID_H

#ifdef __ASSEMBLER__
.macro arch_cpuid out_reg
    csrr \out_reg, mhartid
.endm
#else
static inline int arch_cpuid( void )
{
    unsigned long hartid;

    __asm volatile ( "csrr %0, mhartid" : "=r" ( hartid ) );
    return ( int ) hartid;
}
#endif

#endif
