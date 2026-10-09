#include "interrupt.h"

void arch_disable_interrupts( void )
{
    __asm volatile ( "csrc mstatus, 0x8" ::: "memory" );
}

void arch_enable_interrupts( void )
{
    __asm volatile ( "csrs mstatus, 0x8" ::: "memory" );
}

unsigned long arch_save_and_disable_interrupts( void )
{
    unsigned long status;

    __asm volatile ( "csrrci %0, mstatus, 0x8" : "=r" ( status ) :: "memory" );
    return status;
}

void arch_restore_interrupts( unsigned long status )
{
    if( ( status & 0x8ul ) != 0ul )
    {
        __asm volatile ( "csrs mstatus, 0x8" ::: "memory" );
    }
    else
    {
        __asm volatile ( "csrc mstatus, 0x8" ::: "memory" );
    }
}

unsigned long arch_set_interrupt_mask_from_isr( void )
{
    return arch_save_and_disable_interrupts();
}

void arch_clear_interrupt_mask_from_isr( unsigned long mask )
{
    arch_restore_interrupts( mask );
}
