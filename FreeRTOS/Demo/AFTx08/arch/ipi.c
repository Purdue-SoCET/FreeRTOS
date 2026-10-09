#include <stdint.h>

#include "ipi.h"
#include "riscv-virt.h"

#define MSIP_REG( hartid )    ( ( volatile uint32_t * ) ( CLINT_ADDR + ( ( uint32_t ) ( hartid ) * 4u ) ) )

void arch_ipi_send( int cpuid )
{
    __asm volatile ( "fence rw, rw" ::: "memory" );
    *MSIP_REG( cpuid ) = 1u;
}

void arch_ipi_clear( void )
{
    unsigned long hartid;

    __asm volatile ( "csrr %0, mhartid" : "=r" ( hartid ) );
    *MSIP_REG( hartid ) = 0u;
}
