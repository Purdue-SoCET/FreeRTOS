#include <stdint.h>
#include <stdio.h>

#include "FreeRTOS.h"
#include "task.h"

#define USE_FREERTOS_MODE    0

#define ALARM_PERIOD_MS      10
#define FIB_N                15 
#define SAMPLE_COUNT         5

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

volatile uint32_t g_trigger_cycle = 0;   
volatile uint32_t g_isr_cycle = 0;     
volatile uint8_t  g_bare_metal_flag = 0; 

void AlarmKickFromTickISR(void) {
    g_isr_cycle = read_cycle32();

#if ( USE_FREERTOS_MODE == 1 )
    if (gAlarmTaskHandle != NULL) {
        BaseType_t hpw = pdFALSE;
        vTaskNotifyGiveFromISR(gAlarmTaskHandle, &hpw);
        portYIELD_FROM_ISR(hpw);
    }
#else
    g_bare_metal_flag = 1;
#endif
}

#if ( USE_FREERTOS_MODE == 1 )

static void FibTask(void *pv) {
    (void)pv;
    for (;;) {
        fib(FIB_N);
        for (volatile int i = 0; i < 1000; i++); 
    }
}

static void AlarmTask(void *pv) {
    (void)pv;
    uint32_t maxCycles = 0, sumCycles = 0, cnt = 0;

    printf("\n--- FreeRTOS Latency Mode ---\n");
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

void main_blinky(void) {
#if ( USE_FREERTOS_MODE == 1 )
    xTaskCreate(AlarmTask, "ALARM", STACK_ALARM, NULL, PRIO_ALARM, &gAlarmTaskHandle);
    xTaskCreate(FibTask, "FIB", STACK_FIB, NULL, PRIO_FIB, NULL);
    vTaskStartScheduler();
#else
    uint32_t maxMainCycles = 0;
    uint64_t sumMainCycles = 0;
    uint32_t samples = 0;

    printf("\n--- Bare-metal Latency Mode ---\n");

    while (1) {
        fib(FIB_N);
        g_trigger_cycle = read_cycle32();
        AlarmKickFromTickISR();

        if (g_bare_metal_flag) {
            uint32_t now = read_cycle32();
            g_bare_metal_flag = 0;

            uint32_t main_lat_cycles = now - g_isr_cycle;

            if (main_lat_cycles > maxMainCycles) {
                maxMainCycles = main_lat_cycles;
            }
            sumMainCycles += main_lat_cycles;
            samples++;

            if (samples >= SAMPLE_COUNT) {
                uint32_t avgMainCycles = (uint32_t)(sumMainCycles / samples);
                
                printf("[Bare] Max: %u us, Avg: %u us\n", 
                       cycles_to_us(maxMainCycles), 
                       cycles_to_us(avgMainCycles));

                maxMainCycles = 0;
                sumMainCycles = 0;
                samples = 0;
            }
        }
    }
#endif
}