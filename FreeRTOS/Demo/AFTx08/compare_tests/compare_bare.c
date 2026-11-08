/*
 * Bare metal on hart 0. Hart 1 stays in wfi in start.S because this
 * function never calls vTaskStartScheduler().
 *
 * The loop finishes one load chunk, then checks a fixed deadline.
 * Nothing stops a chunk in the middle. Lateness is now - deadline,
 * and the next deadline stays on the original grid.
 */
#include <string.h>

#include "compare_common.h"

void main_compare_bare( void )
{
    CompareStats stats;
    uint32_t load0_iters = 0;
    uint64_t window_start;
    uint64_t window_end;
    uint64_t deadline;
    uint32_t step;

    memset( &stats, 0, sizeof( stats ) );
    printf( "Get into compare bare\n" );

    window_start = compare_read_mtime();
    window_end = window_start + ( uint64_t ) COMPARE_WINDOW_CYCLES;
    deadline = window_start + ( uint64_t ) COMPARE_PERIOD_CYCLES;
    step = 1u;

    while( compare_read_mtime() < window_end )
    {
        uint64_t now;

        load0_iters += compare_load_chunk( ( stats.chunk_cycles == 0u ) ? &stats.chunk_cycles : NULL );
        now = compare_read_mtime();

        if( now >= window_end )
        {
            break;
        }

        while( ( now >= deadline ) && ( deadline < window_end ) )
        {
            compare_record( &stats, ( uint32_t ) ( now - deadline ) );
            step++;
            deadline = window_start + ( ( uint64_t ) step * ( uint64_t ) COMPARE_PERIOD_CYCLES );
        }
    }

    compare_print( "bare", load0_iters, 0u, &stats );

    for( ; ; )
    {
        __asm volatile ( "nop" );
    }
}
