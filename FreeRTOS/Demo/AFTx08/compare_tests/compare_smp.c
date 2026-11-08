/*
 * SMP on this port. control is pinned to hart 0. load0 is pinned to
 * hart 0 and runs when control is blocked. load1 is pinned to hart 1.
 * The second load task is the second hart: one task can occupy only
 * one hart, so without load1 hart 1 would sit in idle.
 *
 * configRUN_MULTIPLE_PRIORITIES must be 1. With it at 0, hart 1 drops
 * to idle while control is actually running. Control's body is short,
 * so load1_iters would still look like a full hart. The summary prints
 * the macro so that mistake is visible.
 */
#include <string.h>

#include "compare_common.h"
#include "task.h"

static volatile uint32_t g_measure;
static volatile uint32_t g_done;
static volatile uint32_t g_window_start_lo;
static volatile uint32_t g_load0_iters;
static volatile uint32_t g_load1_iters;
static volatile uint32_t g_chunk_cycles;

static void prvLoadTask( void * pv )
{
    volatile uint32_t * counter = ( volatile uint32_t * ) pv;

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

        *counter += compare_load_chunk( ( ( counter == &g_load0_iters ) && ( g_chunk_cycles == 0u ) ) ? ( uint32_t * ) &g_chunk_cycles : NULL );
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
    vTaskDelay( pdMS_TO_TICKS( 20 ) );

    stats.chunk_cycles = g_chunk_cycles;
    compare_print( "smp", g_load0_iters, g_load1_iters, &stats );
    vTaskSuspend( NULL );
}

void main_compare_smp( void )
{
    printf( "Get into compare smp\n" );

    if( xTaskCreateAffinitySet( prvLoadTask, "load0", configMINIMAL_STACK_SIZE * 2, ( void * ) &g_load0_iters, tskIDLE_PRIORITY + 1, ( 1U << 0 ), NULL ) != pdPASS )
    {
        printf( "Failed to create load0\n" );
    }

    if( xTaskCreateAffinitySet( prvLoadTask, "load1", configMINIMAL_STACK_SIZE * 2, ( void * ) &g_load1_iters, tskIDLE_PRIORITY + 1, ( 1U << 1 ), NULL ) != pdPASS )
    {
        printf( "Failed to create load1\n" );
    }

    if( xTaskCreateAffinitySet( prvControlTask, "control", configMINIMAL_STACK_SIZE * 2, NULL, tskIDLE_PRIORITY + 3, ( 1U << 0 ), NULL ) != pdPASS )
    {
        printf( "Failed to create control\n" );
    }

    vTaskStartScheduler();

    printf( "Scheduler did not start\n" );

    for( ; ; )
    {
    }
}
