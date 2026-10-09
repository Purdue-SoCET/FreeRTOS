#include "FreeRTOS.h"

#include "spinlock.h"

struct arch_spinlock_t arch_task_lock =
{
    .owner = -1,
    .cnt = 0,
};

struct arch_spinlock_t arch_isr_lock =
{
    .owner = -1,
    .cnt = 0,
};

struct arch_spinlock_t arch_uart_lock =
{
    .owner = -1,
    .cnt = 0,
};

static inline int compare_and_swap( volatile int * addr, int old_value, int new_value )
{
    int prev;
    int result;

    __asm volatile (
        "0: lr.w %[prev], (%[addr])\n"
        "   bne  %[prev], %[old], 1f\n"
        "   sc.w %[res], %[new], (%[addr])\n"
        "   bnez %[res], 0b\n"
        "1:\n"
        : [prev] "=&r" ( prev ), [res] "=&r" ( result )
        : [addr] "r" ( addr ), [old] "r" ( old_value ), [new] "r" ( new_value )
        : "memory" );
    return prev == old_value;
}

void arch_spinlock_lock( int cpuid, struct arch_spinlock_t * lock )
{
    if( lock->owner == cpuid )
    {
        lock->cnt++;
        return;
    }

    for( ;; )
    {
        if( compare_and_swap( &lock->owner, -1, cpuid ) != 0 )
        {
            __asm volatile ( "fence rw, rw" ::: "memory" );
            lock->cnt = 1;
            return;
        }
    }
}

void arch_spinlock_unlock( int cpuid, struct arch_spinlock_t * lock )
{
    configASSERT( lock->owner == cpuid );

    lock->cnt--;
    if( lock->cnt > 0 )
    {
        return;
    }

    __asm volatile ( "" ::: "memory" );
    lock->owner = -1;
    __asm volatile ( "fence rw, rw" ::: "memory" );
}
