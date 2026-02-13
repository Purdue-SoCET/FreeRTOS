#include <stdint.h>
#include <stdio.h>

#include "FreeRTOS.h"
#include "task.h"

/* ---- Simple knobs ---- */
#define PRIO_FIB     ( tskIDLE_PRIORITY + 1 )  /* low */
#define PRIO_ALARM   ( tskIDLE_PRIORITY + 4 )  /* high */

#define STACK_FIB    ( configMINIMAL_STACK_SIZE * 2 )
#define STACK_ALARM  ( configMINIMAL_STACK_SIZE * 2 )

#define PERIOD_MS          1       /* measurement resolution */
#define PRINT_EVERY_MS     100     /* how often we print results */
#define FIB_N              25

__attribute__((noinline))
static uint32_t fib(uint32_t n)
{
    if (n < 2) return n;
    return fib(n - 1) + fib(n - 2);
}

/* Low priority CPU load + small heartbeat print */
static void FibTask(void *pv)
{
    (void)pv;
    volatile uint32_t r = 0;
    uint32_t count = 0;

    for (;;)
    {
        r = fib(FIB_N);
        (void)r;

        /* show Fib is alive, but don't spam */
        if ((++count % 200) == 0) {
            printf("[T=%u] FIB running, last=%u\n",
                   (unsigned)xTaskGetTickCount(), (unsigned)r);
        }
    }
}

/* High priority periodic task measures latency but prints less */
static void AlarmTask(void *pv)
{
    (void)pv;

    const TickType_t period = pdMS_TO_TICKS(PERIOD_MS);
    TickType_t lastWake = xTaskGetTickCount();

    const uint32_t printEvery = PRINT_EVERY_MS / PERIOD_MS; /* e.g. 100 */
    uint32_t prints = 0;

    TickType_t maxLate = 0;

    printf("=== 2-task latency demo ===\n");
    printf("tick_hz=%u, alarm_period=%u ms, print_every=%u ms, fib_n=%u\n\n",
           (unsigned)configTICK_RATE_HZ,
           (unsigned)PERIOD_MS,
           (unsigned)PRINT_EVERY_MS,
           (unsigned)FIB_N);

    for (;;)
    {
        vTaskDelayUntil(&lastWake, period);

        TickType_t now = xTaskGetTickCount();
        TickType_t ideal = lastWake;
        TickType_t late = (now > ideal) ? (now - ideal) : 0;

        if (late > maxLate) maxLate = late;

        /* print only every PRINT_EVERY_MS */
        if ((++prints % printEvery) == 0) {
            uint32_t maxLateMs = (uint32_t)(maxLate * 1000UL / (uint32_t)configTICK_RATE_HZ);
            printf("[T=%u] ALARM max_latency=%u ticks (~%u ms)\n",
                   (unsigned)now, (unsigned)maxLate, (unsigned)maxLateMs);
            maxLate = 0;
        }
    }
}



void main_blinky(void)
{
    BaseType_t ok;

    ok = xTaskCreate(AlarmTask, "ALARM", STACK_ALARM, NULL, PRIO_ALARM, NULL);
    configASSERT(ok == pdPASS);

    ok = xTaskCreate(FibTask, "FIB", STACK_FIB, NULL, PRIO_FIB, NULL);
    configASSERT(ok == pdPASS);

    vTaskStartScheduler();

    printf("ERROR: scheduler returned\n");
    for (;;);
}
