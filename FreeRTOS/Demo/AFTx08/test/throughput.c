#include <stdint.h>
#include <stdio.h>

#include "FreeRTOS.h"
#include "task.h"

/* freeRTOS mode 1 or bare-metal mode 0 */
#define USE_FREERTOS_MODE    1

#define FIB_N                5
#define SAMPLE_COUNT         10
#define TEST_WINDOW_TICKS    100  // 1 tick = 10 ms , 100 tick = 1s

#if ( USE_FREERTOS_MODE == 1 )
    #define PRIO_FIB         ( tskIDLE_PRIORITY + 1 )
    #define PRIO_MONITOR     ( tskIDLE_PRIORITY + 3 )
    #define STACK_FIB        ( configMINIMAL_STACK_SIZE * 4 )
    #define STACK_MONITOR    ( configMINIMAL_STACK_SIZE * 3 )
#endif

static inline uint32_t read_cycle32(void)
{
    uint32_t c;
#if defined(__riscv)
    __asm volatile ("rdcycle %0" : "=r"(c));
#else
    c = 0;
#endif
    return c;
}

static inline uint32_t cycles_to_us(uint32_t cycles)
{
    return (uint32_t)(((uint64_t)cycles * 1000000ULL) / (uint64_t)configCPU_CLOCK_HZ);
}

__attribute__((noinline))
static uint32_t fib(uint32_t n)
{
    if (n < 2) return n;
    return fib(n - 1) + fib(n - 2);
}

/* global counters */
volatile uint32_t g_job_count = 0;
volatile uint32_t g_dummy_sink = 0;
/* keep this for main.c tick hook */
void AlarmKickFromTickISR(void)
{
    /* not used in throughput test */
}


#if ( USE_FREERTOS_MODE == 1 )

static void FibTask(void *pv)
{
    (void)pv;

    for (;;)
    {
        g_dummy_sink = fib(FIB_N);
        g_job_count++;
    }
}


static void MonitorTask(void *pv)
{
    (void)pv;

    uint32_t max_jobs = 0;
    uint32_t sum_jobs = 0;
    uint32_t cnt = 0;

    printf("\nFreeRTOS Throughput Mode\n");

    for (;;)
    {
        uint32_t start_jobs = g_job_count;
        uint32_t start_cycle = read_cycle32();

        vTaskDelay(TEST_WINDOW_TICKS);

        uint32_t end_cycle = read_cycle32();
        uint32_t end_jobs = g_job_count;

        uint32_t jobs_done = end_jobs - start_jobs;
        uint32_t elapsed_cycles = end_cycle - start_cycle;

        if (jobs_done > max_jobs) max_jobs = jobs_done;
        sum_jobs += jobs_done;
        cnt++;

        printf("[RTOS] jobs/window: %u, cycles/job: %u, time: %u us\n",
            jobs_done,
            (jobs_done > 0) ? (elapsed_cycles / jobs_done) : 0,
            cycles_to_us(elapsed_cycles));

        if (cnt >= SAMPLE_COUNT)
        {
            printf("[RTOS] Max jobs/sec: %u, Avg jobs/sec: %u\n",
                   max_jobs,
                   sum_jobs / cnt);

            max_jobs = 0;
            sum_jobs = 0;
            cnt = 0;
        }
    }
}

#endif

void main_blinky(void)
{
#if ( USE_FREERTOS_MODE == 1 )

    xTaskCreate(MonitorTask, "MON", STACK_MONITOR, NULL, PRIO_MONITOR, NULL);
    xTaskCreate(FibTask, "FIB", STACK_FIB, NULL, PRIO_FIB, NULL);
    vTaskStartScheduler();
    for (;;);

#else   //bare-metal simulation mode

    uint32_t max_jobs = 0;
    uint32_t sum_jobs = 0;
    uint32_t samples = 0;

    const uint32_t test_cycles = configCPU_CLOCK_HZ;   // 1s

    printf("\nBare-metal Throughput Mode\n");

    while (1)
    {
        uint32_t start_cycle = read_cycle32();
        uint32_t jobs_done = 0;

        while ((uint32_t)(read_cycle32() - start_cycle) < test_cycles)
        {
            g_dummy_sink = fib(FIB_N);
            jobs_done++;
        }

        uint32_t elapsed_cycles = (uint32_t)(read_cycle32() - start_cycle);

        if (jobs_done > max_jobs) max_jobs = jobs_done;
        sum_jobs += jobs_done;
        samples++;

        printf("[Bare] jobs/window: %u, cycles/job: %u, time: %u us\n",
               jobs_done,
               (jobs_done > 0) ? (elapsed_cycles / jobs_done) : 0,
               cycles_to_us(elapsed_cycles));

        if (samples >= SAMPLE_COUNT)
        {
            printf("[Bare] Max jobs/window: %u, Avg jobs/window: %u\n",
                   max_jobs,
                   sum_jobs / samples);

            max_jobs = 0;
            sum_jobs = 0;
            samples = 0;
        }
    }

#endif
}