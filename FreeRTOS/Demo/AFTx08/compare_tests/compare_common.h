/*
 * Shared work for the bare-metal, single-core, and SMP compare demos.
 * The loop and the summary text live here so the three builds measure
 * the same chunk.
 *
 * Load is a counted volatile increment of a local. The value is unused.
 * Each caller has its own local, so two harts do not bounce one address.
 * One chunk is aimed at about 100,000 cycles. The summary prints the
 * measured chunk_cycles from the first chunk; change COMPARE_CHUNK_ITERS
 * if that number is far from 100,000.
 *
 * Control does not compute anything. It records how late a fixed 10 ms
 * deadline was. Deadlines are start + n * 250,000 cycles. A late wake
 * does not move the next deadline.
 */
#ifndef COMPARE_COMMON_H
#define COMPARE_COMMON_H

#include <stdint.h>
#include <stdio.h>

#include "FreeRTOS.h"

#define COMPARE_PERIOD_CYCLES    250000u
#define COMPARE_TICK_CYCLES      25000u
#define COMPARE_WINDOW_CYCLES    5000000u
#define COMPARE_MISS_CYCLES      25000u
#define COMPARE_CHUNK_ITERS      16000u

typedef struct CompareStats
{
    uint32_t samples;
    uint32_t min_late;
    uint32_t max_late;
    uint32_t sum_late;
    uint32_t misses;
    uint32_t chunk_cycles;
} CompareStats;

static inline uint64_t compare_read_mtime( void )
{
    volatile uint32_t * const hi = ( volatile uint32_t * ) ( CLINT_ADDR + CLINT_MTIME + 4u );
    volatile uint32_t * const lo = ( volatile uint32_t * ) ( CLINT_ADDR + CLINT_MTIME );
    uint32_t high;
    uint32_t low;

    do
    {
        high = *hi;
        low = *lo;
    } while( high != *hi );

    return ( ( uint64_t ) high << 32 ) | ( uint64_t ) low;
}

/* mtimecmp[0], not ullNextTime. The tick handler advances ullNextTime
 * one tick past the register it just wrote. */
static inline uint64_t compare_read_mtimecmp0( void )
{
    volatile uint32_t * const hi = ( volatile uint32_t * ) ( CLINT_ADDR + CLINT_MTIMECMP + 4u );
    volatile uint32_t * const lo = ( volatile uint32_t * ) ( CLINT_ADDR + CLINT_MTIMECMP );
    uint32_t high;
    uint32_t low;

    do
    {
        high = *hi;
        low = *lo;
    } while( high != *hi );

    return ( ( uint64_t ) high << 32 ) | ( uint64_t ) low;
}

static inline uint32_t compare_load_chunk( uint32_t * chunk_cycles )
{
    uint32_t i;
    uint64_t started = 0;
    volatile uint32_t sink = 0;

    if( chunk_cycles != NULL )
    {
        started = compare_read_mtime();
    }

    for( i = 0; i < COMPARE_CHUNK_ITERS; i++ )
    {
        sink++;
    }

    if( chunk_cycles != NULL )
    {
        *chunk_cycles = ( uint32_t ) ( compare_read_mtime() - started );
    }

    return COMPARE_CHUNK_ITERS;
}

static inline void compare_record( CompareStats * stats, uint32_t lateness )
{
    if( ( stats->samples == 0u ) || ( lateness < stats->min_late ) )
    {
        stats->min_late = lateness;
    }

    if( lateness > stats->max_late )
    {
        stats->max_late = lateness;
    }

    stats->sum_late += lateness;
    stats->samples++;

    if( lateness > COMPARE_MISS_CYCLES )
    {
        stats->misses++;
    }
}

static inline void compare_print( const char * mode,
                                  uint32_t load0_iters,
                                  uint32_t load1_iters,
                                  const CompareStats * stats )
{
    uint32_t avg = 0;
    uint32_t spread = 0;

    if( stats->samples > 0u )
    {
        avg = stats->sum_late / stats->samples;
        spread = stats->max_late - stats->min_late;
    }

    printf( "MODE %s\n", mode );
    printf( "cores %u\n", ( unsigned ) configNUMBER_OF_CORES );
    printf( "run_multiple_priorities %u\n", ( unsigned ) configRUN_MULTIPLE_PRIORITIES );
    printf( "timers %u\n", ( unsigned ) configUSE_TIMERS );
    printf( "window_cycles %u\n", ( unsigned ) COMPARE_WINDOW_CYCLES );
    printf( "period_cycles %u\n", ( unsigned ) COMPARE_PERIOD_CYCLES );
    printf( "tick_cycles %u\n", ( unsigned ) COMPARE_TICK_CYCLES );
    printf( "chunk_cycles %u\n", ( unsigned ) stats->chunk_cycles );
    printf( "samples %u\n", ( unsigned ) stats->samples );
    printf( "lateness_cycles min %u max %u avg %u\n",
            ( unsigned ) stats->min_late,
            ( unsigned ) stats->max_late,
            ( unsigned ) avg );
    printf( "lateness_spread %u\n", ( unsigned ) spread );
    printf( "misses %u\n", ( unsigned ) stats->misses );
    printf( "load0_iters %u\n", ( unsigned ) load0_iters );
    printf( "load1_iters %u\n", ( unsigned ) load1_iters );
}

#endif /* COMPARE_COMMON_H */
