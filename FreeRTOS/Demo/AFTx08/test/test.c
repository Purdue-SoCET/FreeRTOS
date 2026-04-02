#include <stdint.h>
#include <stdio.h>
#include "FreeRTOS.h"
#include "task.h"

#define USE_FREERTOS_MODE         1

// Units: mm, mm/s
#define INITIAL_SPEED_MMPS        9000
#define INITIAL_DISTANCE_MM       20000
#define BRAKE_THRESHOLD_MM        10000
#define BRAKE_DECEL_MMPS2         8000

volatile int32_t g_v_speed = INITIAL_SPEED_MMPS;
volatile int32_t g_v_dist  = INITIAL_DISTANCE_MM;
volatile int     g_v_brake = 0;
volatile int     g_v_done  = 0; // 0: running, 1: stopped safely, 2: collision

#define PHYSICS_WAKE_MS           10
#define SIMULATION_STEP_MS        250
#define SENSOR_WAKE_MS            10
#define BRAKE_COMMAND_DELAY_MS    20
#define MONITOR_PERIOD_MS         20

static uint32_t g_start_cycle = 0;
static uint32_t g_threshold_cycle = 0;
static uint32_t g_brake_cycle = 0;
static uint32_t g_final_cycle = 0;
static volatile int g_threshold_detected = 0;
static volatile int g_brake_request = 0;

static uint32_t read_cycle32(void) {
#if defined(__riscv)
    uint32_t c;
    __asm volatile ("rdcycle %0" : "=r"(c));
    return c;
#else
    return 0;
#endif
}

static uint32_t cycles_to_us(uint32_t cycles) {
    return (uint32_t)(((uint64_t)cycles * 1000000ULL) / (uint64_t)configCPU_CLOCK_HZ);
}

void AlarmKickFromTickISR(void) { /* not used in this throughput test */ }

static void StepPhysics(void) {
    const int32_t dt_ms = SIMULATION_STEP_MS;
    const int32_t decel = BRAKE_DECEL_MMPS2;

    if (g_v_done != 0) {
        return;
    }

    // Update the physical state
    g_v_dist -= (g_v_speed * dt_ms) / 1000;

    // Once braking is active, keep decelerating on each physics step.
    if (g_v_brake != 0) {
        g_v_speed -= (decel * dt_ms) / 1000;

        if (g_v_speed <= 0) {
            g_v_speed = 0;
            g_v_done = 1; // Safe stop
        }
    }

    // Check whether the vehicle has reached the obstacle
    if (g_v_dist <= 0) {
        g_v_dist = 0;

        // If the remaining speed is still too high, treat it as a collision
        if (g_v_speed > 500) {
            g_v_done = 2; // Crash
        } else {
            g_v_done = 1; // Safe stop near the obstacle
        }
    }
}

static void DetectThresholdIfNeeded(void) {
    if ((g_threshold_detected == 0) && (g_v_dist <= BRAKE_THRESHOLD_MM)) {
        g_threshold_detected = 1;
        g_brake_request = 1;
        g_threshold_cycle = read_cycle32();
        printf("THRESHOLD S:%d D:%d CYC:%u US:%u\n",
               (int)g_v_speed,
               (int)g_v_dist,
               (unsigned)g_threshold_cycle,
               (unsigned)cycles_to_us(g_threshold_cycle - g_start_cycle));
    }
}

static void StartBrakeIfRequested(void) {
    if ((g_brake_request != 0) && (g_v_brake == 0)) {
        g_v_brake = 1;
        g_brake_cycle = read_cycle32();
        printf("BRAKE_START S:%d D:%d CYC:%u US:%u\n",
               (int)g_v_speed,
               (int)g_v_dist,
               (unsigned)g_brake_cycle,
               (unsigned)cycles_to_us(g_brake_cycle - g_start_cycle));
    }
}

static void PrintFinalResultIfDone(void) {
    if (g_v_done != 0) {
        if (g_final_cycle == 0) {
            const uint32_t response_cycles =
                (g_brake_cycle >= g_threshold_cycle) ? (g_brake_cycle - g_threshold_cycle) : 0;

            g_final_cycle = read_cycle32();
            printf("FINAL %s S:%d D:%d CYC:%u US:%u\n",
                   (g_v_done == 1) ? "SAFE" : "CRASH",
                   (int)g_v_speed,
                   (int)g_v_dist,
                   (unsigned)g_final_cycle,
                   (unsigned)cycles_to_us(g_final_cycle - g_start_cycle));
            printf("LATENCY CYC:%u US:%u\n",
                   (unsigned)response_cycles,
                   (unsigned)cycles_to_us(response_cycles));
        }
    }
}

#if ( USE_FREERTOS_MODE == 1 )
static void PhysicsBrakeTask(void *pv) {
    (void)pv;

    for (;;) {
        StepPhysics();
        vTaskDelay(pdMS_TO_TICKS(PHYSICS_WAKE_MS));
    }
}

static void SensorTask(void *pv) {
    (void)pv;

    for (;;) {
        DetectThresholdIfNeeded();
        vTaskDelay(pdMS_TO_TICKS(SENSOR_WAKE_MS));
    }
}

static void BrakeTask(void *pv) {
    (void)pv;

    for (;;) {
        if ((g_brake_request != 0) && (g_v_brake == 0)) {
            vTaskDelay(pdMS_TO_TICKS(BRAKE_COMMAND_DELAY_MS));
            StartBrakeIfRequested();
        }

        vTaskDelay(pdMS_TO_TICKS(1));
    }
}

static void MonitorTask(void *pv) {
    (void)pv;

    for (;;) {
        if (g_v_done != 0) {
            PrintFinalResultIfDone();
            vTaskDelay(pdMS_TO_TICKS(100));
            vTaskDelete(NULL);
        }

        vTaskDelay(pdMS_TO_TICKS(MONITOR_PERIOD_MS));
    }
}

void main_blinky(void) {
    g_start_cycle = read_cycle32();
    printf("START S:%d D:%d CYC:%u\n", (int)g_v_speed, (int)g_v_dist, (unsigned)g_start_cycle);

    xTaskCreate(PhysicsBrakeTask, "PHY", 256, NULL, tskIDLE_PRIORITY + 3, NULL);
    xTaskCreate(SensorTask,       "SNS", 256, NULL, tskIDLE_PRIORITY + 2, NULL);
    xTaskCreate(BrakeTask,        "BRK", 256, NULL, tskIDLE_PRIORITY + 1, NULL);
    xTaskCreate(MonitorTask,      "MON", 512, NULL, tskIDLE_PRIORITY + 2, NULL);

    vTaskStartScheduler();
}
#else
void main_blinky(void) {
    g_start_cycle = read_cycle32();
    printf("START S:%d D:%d CYC:%u\n", (int)g_v_speed, (int)g_v_dist, (unsigned)g_start_cycle);

    for (;;) {
        StepPhysics();
        DetectThresholdIfNeeded();
        StartBrakeIfRequested();

        if (g_v_done != 0) {
            PrintFinalResultIfDone();
            break;
        }
    }

    for (;;) {
    }
}
#endif
