#include "FreeRTOS.h"

#include "spinlock.h"

struct arch_spinlock_t arch_task_lock =
{
    .owner = -1,
    .cnt = 0,
    .flag = { 0, 0 },
    .turn = 0,
};

struct arch_spinlock_t arch_isr_lock =
{
    .owner = -1,
    .cnt = 0,
    .flag = { 0, 0 },
    .turn = 0,
};

struct arch_spinlock_t arch_uart_lock =
{
    .owner = -1,
    .cnt = 0,
    .flag = { 0, 0 },
    .turn = 0,
};

void arch_spinlock_lock( int cpuid, struct arch_spinlock_t * lock )
{
    int other;

    configASSERT( ( cpuid == 0 ) || ( cpuid == 1 ) );
    if( lock->owner == cpuid )
    {
        lock->cnt++;
        return;
    }

    other = cpuid ^ 1;
    lock->flag[ cpuid ] = 1;
    __asm volatile ( "fence w, w" ::: "memory" );
    lock->turn = other;
    __asm volatile ( "fence w, r" ::: "memory" );

    while( ( lock->flag[ other ] != 0 ) && ( lock->turn == other ) )
    {
    }

    __asm volatile ( "fence r, rw" ::: "memory" );
    lock->cnt = 1;
    lock->owner = cpuid;
}

void arch_spinlock_unlock( int cpuid, struct arch_spinlock_t * lock )
{
    configASSERT( lock->owner == cpuid );

    lock->cnt--;

    if( lock->cnt > 0 )
    {
        return;
    }

    __asm volatile ( "fence rw, w" ::: "memory" );
    lock->owner = -1;
    __asm volatile ( "fence w, w" ::: "memory" );
    lock->flag[ cpuid ] = 0;
}
