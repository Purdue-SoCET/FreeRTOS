/*
 * Single-core FreeRTOS. Build with configNUMBER_OF_CORES 1.
 * Hart 1 still comes out of reset on this chip, but the scheduler
 * never sends it an msip, so it stays in wfi in start.S.
 *
 * control is priority idle+3 and sleeps with vTaskDelayUntil.
 * load is priority idle+1 and runs in the gaps.
 * Lateness is measured from the tick that was supposed to wake
 * control, not from the previous wake.
 */
#include <string.h>

#include "compare_common.h"
#include "task.h"

static volatile uint32_t g_measure;
static volatile uint32_t g_done;
static volatile uint32_t g_window_start_lo;
static volatile uint32_t g_load0_iters;
static volatile uint32_t g_chunk_cycles;

static void prvLoadTask( void * pv )
{
    ( void ) pv;

    while( g_measure == 0u )
    {
        __asm volatile ( "nop" );
    }

    __asm volatile ( "fence rw, rw" ::: "memory" );

    while( g_done == 0u )
    {
        uint32_t elapsed = ( uint32_t ) compare_read_mtime() - g_window_start_lo;

        if( elapsed >= COMPARE_WINDOW_CYCLES )
        {
            break;
        }

        g_load0_iters += compare_load_chunk( ( g_chunk_cycles == 0u ) ? ( uint32_t * ) &g_chunk_cycles : NULL );
    }

    for( ; ; )
    {
        __asm volatile ( "nop" );
    }
}

static void prvControlTask( void * pv )
{
    CompareStats stats;
    TickType_t wake;
    uint64_t window_start;
    uint64_t window_end;

    ( void ) pv;
    memset( &stats, 0, sizeof( stats ) );

    window_start = compare_read_mtime();
    window_end = window_start + ( uint64_t ) COMPARE_WINDOW_CYCLES;
    g_window_start_lo = ( uint32_t ) window_start;
    __asm volatile ( "fence rw, rw" ::: "memory" );
    g_measure = 1u;
    __asm volatile ( "fence rw, rw" ::: "memory" );

    wake = xTaskGetTickCount();

    while( ( uint32_t ) ( compare_read_mtime() - window_start ) < COMPARE_WINDOW_CYCLES )
    {
        uint64_t now;
        uint64_t compare;
        uint64_t tick_edge;
        TickType_t tick_now;
        uint32_t extra_ticks;
        uint32_t within;
        uint32_t lateness;

        vTaskDelayUntil( &wake, pdMS_TO_TICKS( 10 ) );

        now = compare_read_mtime();

        if( now >= window_end )
        {
            break;
        }

        compare = compare_read_mtimecmp0();
        tick_now = xTaskGetTickCount();
        extra_ticks = ( uint32_t ) ( tick_now - wake );
        tick_edge = compare - ( uint64_t ) COMPARE_TICK_CYCLES;
        within = ( now >= tick_edge ) ? ( uint32_t ) ( now - tick_edge ) : 0u;
        lateness = ( extra_ticks * COMPARE_TICK_CYCLES ) + within;
        compare_record( &stats, lateness );
    }

    g_done = 1u;
    __asm volatile ( "fence rw, rw" ::: "memory" );

    /* The load task may be inside one chunk. Let that chunk finish
     * before copying the counter. */
    vTaskDelay( pdMS_TO_TICKS( 20 ) );

    stats.chunk_cycles = g_chunk_cycles;
    compare_print( "single", g_load0_iters, 0u, &stats );
    vTaskSuspend( NULL );
}

void main_compare_single( void )
{
    printf( "Get into compare single\n" );

    if( xTaskCreate( prvLoadTask, "load", configMINIMAL_STACK_SIZE * 2, NULL, tskIDLE_PRIORITY + 1, NULL ) != pdPASS )
    {
        printf( "Failed to create load\n" );
    }

    if( xTaskCreate( prvControlTask, "control", configMINIMAL_STACK_SIZE * 2, NULL, tskIDLE_PRIORITY + 3, NULL ) != pdPASS )
    {
        printf( "Failed to create control\n" );
    }

    vTaskStartScheduler();

    printf( "Scheduler did not start\n" );

    for( ; ; )
    {
    }
}
