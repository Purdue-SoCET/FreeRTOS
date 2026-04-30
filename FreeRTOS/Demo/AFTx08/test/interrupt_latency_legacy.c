#include <stdint.h>
#include <stdio.h>

#include "FreeRTOS.h"
#include "task.h"

// freeRTOS mode 1 or bare-metal mode 0
#define USE_FREERTOS_MODE    1

#define FIB_N                5
#define SAMPLE_COUNT         10

#if ( USE_FREERTOS_MODE == 1 )
    #define PRIO_FIB         ( tskIDLE_PRIORITY + 1 )
    #define PRIO_ALARM       ( tskIDLE_PRIORITY + 4 )
    #define STACK_FIB        ( configMINIMAL_STACK_SIZE * 4 )
    #define STACK_ALARM      ( configMINIMAL_STACK_SIZE * 3 )
    static TaskHandle_t gAlarmTaskHandle = NULL;
#endif

static inline uint32_t read_cycle32(void) {
    uint32_t c;
#if defined(__riscv)
    __asm volatile ("rdcycle %0" : "=r"(c));
#else
    c = 0; 
#endif
    return c;
}

static inline uint32_t cycles_to_us(uint32_t cycles) {
    return (uint32_t)(((uint64_t)cycles * 1000000ULL) / (uint64_t)configCPU_CLOCK_HZ);
}

__attribute__((noinline))
static uint32_t fib(uint32_t n) {
    if (n < 2) return n;
    return fib(n - 1) + fib(n - 2);
}

volatile uint32_t g_isr_cycle = 0; // ISR entry timestamp in cycles


// interrupt trigger function.
void AlarmKickFromTickISR(void) {
#if ( USE_FREERTOS_MODE == 1 )
    g_isr_cycle = read_cycle32();
    if (gAlarmTaskHandle != NULL) {
        BaseType_t hpw = pdFALSE;
        vTaskNotifyGiveFromISR(gAlarmTaskHandle, &hpw);
        portYIELD_FROM_ISR(hpw);
    }
#endif
}

// freeRTOS main
#if ( USE_FREERTOS_MODE == 1 )
static void FibTask(void *pv) {
    (void)pv;
    for (;;) {
        fib(FIB_N);
    }
}

static void AlarmTask(void *pv) {
    (void)pv;
    uint32_t maxCycles = 0, sumCycles = 0, cnt = 0;
    printf("\nFreeRTOS Preemptive Mode\n");
    for (;;) {
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
        uint32_t now = read_cycle32();
        uint32_t late = now - g_isr_cycle;

        if (late > maxCycles) maxCycles = late;
        sumCycles += late;
        if (++cnt >= SAMPLE_COUNT) {
            printf("[RTOS] Max: %u us, Avg: %u us\n", 
                   cycles_to_us(maxCycles), cycles_to_us(sumCycles/cnt));
            maxCycles = 0; sumCycles = 0; cnt = 0;
        }
    }
}
#endif

// main function
void main_blinky(void) {
#if ( USE_FREERTOS_MODE == 1 )
    xTaskCreate(AlarmTask, "ALARM", STACK_ALARM, NULL, PRIO_ALARM, &gAlarmTaskHandle);
    xTaskCreate(FibTask, "FIB", STACK_FIB, NULL, PRIO_FIB, NULL);
    vTaskStartScheduler();
#else   // bare-metal simulation mode use timer check.
    uint32_t maxLat = 0, sumLat = 0, samples = 0;
    
    #define MY_CLINT_BASE         0x90010000ULL
    #define MY_CLINT_MTIME        (MY_CLINT_BASE + 0xBFF8)
    volatile uint64_t *mtime = (uint64_t*)(MY_CLINT_MTIME);

    //every 100us generate an event (simulate timer interrupt) and measure latency
    uint64_t tick_interval = (configCPU_CLOCK_HZ / 10000); 
    uint64_t next_event_time = *mtime + tick_interval;

    printf("\nBare-metal\n");
    printf("Simulating events via CLINT timer...\n");

    uint32_t last_check_cycle = read_cycle32();

while (1) {
        fib(FIB_N);
        uint64_t now = *mtime;// get current time from CLINT timer
        if (now >= next_event_time) {
            // late = current time - ideal event time
            uint32_t lat_cycles = (uint32_t)(now - next_event_time);

            if (lat_cycles > maxLat) maxLat = lat_cycles;
            sumLat += lat_cycles;
            samples++;

            // schedule next event
            next_event_time += tick_interval;
            
            while (now >= next_event_time) { next_event_time += tick_interval; }
            if (samples >= SAMPLE_COUNT) {
                printf("[Bare] Task Latency: %u us\n", cycles_to_us(maxLat));
                maxLat = 0; sumLat = 0; samples = 0;
            }
        }
    }
#endif
}