#include <stdint.h>
#include <stdio.h>

#include "FreeRTOS.h"
#include "task.h"

#define LOAD_FIB_N              5
#define JITTER_PERIOD_TICKS     1U
#define REPORT_SAMPLE_COUNT     5U

#define PRIO_LOAD               ( tskIDLE_PRIORITY + 1 )
#define PRIO_MONITOR            ( tskIDLE_PRIORITY + 2 )
#define PRIO_JITTER             ( tskIDLE_PRIORITY + 3 )

#define STACK_LOAD              ( configMINIMAL_STACK_SIZE * 4 )
#define STACK_MONITOR           ( configMINIMAL_STACK_SIZE * 3 )
#define STACK_JITTER            ( configMINIMAL_STACK_SIZE * 3 )

static volatile uint32_t g_dummy_sink = 0;
static volatile uint32_t g_load_jobs = 0;
static volatile uint32_t g_jitter_min_cycles = UINT32_MAX;
static volatile uint32_t g_jitter_max_cycles = 0;
static volatile uint64_t g_jitter_sum_cycles = 0;
static volatile uint32_t g_jitter_samples = 0;

static uint32_t read_cycle32( void )
{
    uint32_t c;
#if defined( __riscv )
    __asm volatile ( "rdcycle %0" : "=r"( c ) );
#else
    c = 0;
#endif
    return c;
}

static uint32_t cycles_to_us( uint32_t cycles ) //depend on clock
{
    return ( uint32_t ) ( ( ( uint64_t ) cycles * 1000000ULL ) / ( uint64_t ) configCPU_CLOCK_HZ );
}

__attribute__( ( noinline ) ) static uint32_t fib( uint32_t n )
{
    if( n < 2U )
    {
        return n;
    }

    return fib( n - 1U ) + fib( n - 2U );
}

void AlarmKickFromTickISR( void )
{
    /* Not used in this jitter test. */
}

static void vLoadTask( void * pv )
{
    ( void ) pv;

    for( ;; )
    {
        g_dummy_sink = fib( LOAD_FIB_N );
        g_load_jobs++;
    }
}

static void vJitterTask( void * pv )
{
    const uint32_t expected_cycles =
        ( uint32_t ) ( ( ( uint64_t ) JITTER_PERIOD_TICKS * configCPU_CLOCK_HZ ) / configTICK_RATE_HZ );
    TickType_t last_wake_time = xTaskGetTickCount();
    uint32_t previous_cycle = read_cycle32();

    ( void ) pv;

    for( ;; )
    {
        uint32_t now;
        uint32_t elapsed_cycles;
        uint32_t jitter_cycles;

        xTaskDelayUntil( &last_wake_time, JITTER_PERIOD_TICKS );

        now = read_cycle32();
        elapsed_cycles = now - previous_cycle;
        previous_cycle = now;

        if( elapsed_cycles >= expected_cycles )
        {
            jitter_cycles = elapsed_cycles - expected_cycles;
        }
        else
        {
            jitter_cycles = expected_cycles - elapsed_cycles;
        }

        taskENTER_CRITICAL();
        {
            if( jitter_cycles < g_jitter_min_cycles )
            {
                g_jitter_min_cycles = jitter_cycles;
            }

            if( jitter_cycles > g_jitter_max_cycles )
            {
                g_jitter_max_cycles = jitter_cycles;
            }

            g_jitter_sum_cycles += jitter_cycles;
            g_jitter_samples++;
        }
        taskEXIT_CRITICAL();
    }
}

static void vMonitorTask( void * pv )
{
    uint32_t last_load_jobs = 0;

    ( void ) pv;

    printf( "\nFreeRTOS Jitter Mode\n" );
    printf( "period_ticks=%u expected_period_us=%u\n",
            ( unsigned ) JITTER_PERIOD_TICKS,
            ( unsigned ) ( ( 1000000ULL * JITTER_PERIOD_TICKS ) / configTICK_RATE_HZ ) );

    for( ;; )
    {
        uint32_t min_cycles;
        uint32_t max_cycles;
        uint32_t sample_count;
        uint32_t avg_cycles;
        uint32_t current_load_jobs;
        uint64_t sum_cycles;

        vTaskDelay( JITTER_PERIOD_TICKS * REPORT_SAMPLE_COUNT );

        taskENTER_CRITICAL();
        {
            min_cycles = g_jitter_min_cycles;
            max_cycles = g_jitter_max_cycles;
            sum_cycles = g_jitter_sum_cycles;
            sample_count = g_jitter_samples;
            g_jitter_min_cycles = UINT32_MAX;
            g_jitter_max_cycles = 0;
            g_jitter_sum_cycles = 0;
            g_jitter_samples = 0;
            current_load_jobs = g_load_jobs;
        }
        taskEXIT_CRITICAL();

        if( sample_count == 0U )
        {
            continue;
        }

        avg_cycles = ( uint32_t ) ( sum_cycles / sample_count );

        printf( "[RTOS] jitter min/max/avg: %u/%u/%u us, load/window: %u\n",
                ( unsigned ) cycles_to_us( min_cycles ),
                ( unsigned ) cycles_to_us( max_cycles ),
                ( unsigned ) cycles_to_us( avg_cycles ),
                ( unsigned ) ( current_load_jobs - last_load_jobs ) );

        last_load_jobs = current_load_jobs;
    }
}

void main_demo( void )
{
    xTaskCreate( vJitterTask, "JIT", STACK_JITTER, NULL, PRIO_JITTER, NULL );
    xTaskCreate( vMonitorTask, "MON", STACK_MONITOR, NULL, PRIO_MONITOR, NULL );
    xTaskCreate( vLoadTask, "LOAD", STACK_LOAD, NULL, PRIO_LOAD, NULL );

    vTaskStartScheduler();

    for( ;; )
    {
    }
}
