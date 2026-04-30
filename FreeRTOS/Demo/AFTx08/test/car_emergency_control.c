#include <stdint.h>
#include <stdio.h>

#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"

#define USE_FREERTOS_MODE         0

/* Units: mm, mm/s, mm/s^2 */
#define INITIAL_SPEED_MMPS        9000
#define INITIAL_DISTANCE_MM       20000
#define BRAKE_THRESHOLD_MM        10000
#define EMERGENCY_THRESHOLD_MM    3500
#define SAFE_STOP_SPEED_MMPS      500
#define MAX_BRAKE_MMPS2           12000

#define PHYSICS_WAKE_MS           10
#define SIMULATION_STEP_MS        250
#define SENSOR_WAKE_MS            10
#define CONTROL_WAKE_MS           10
#define MONITOR_PERIOD_MS         20

#define REPORT_WINDOW_TICKS       10U
#define SENSOR_QUEUE_LENGTH       1U
#define SENSOR_RING_LENGTH        8U

#define PID_KP                    2
#define PID_KI                    1
#define PID_KD                    8
#define PID_SCALE                 8
#define PID_INTEGRAL_LIMIT        50000

#define BACKGROUND_WORK_ITERATIONS 6000U

#define PRIO_BACKGROUND           ( tskIDLE_PRIORITY + 1 )
#define PRIO_MONITOR              ( tskIDLE_PRIORITY + 1 )
#define PRIO_PHYSICS              ( tskIDLE_PRIORITY + 2 )
#define PRIO_SENSOR               ( tskIDLE_PRIORITY + 3 )
#define PRIO_CONTROL              ( tskIDLE_PRIORITY + 4 )
#define PRIO_EMERGENCY            ( tskIDLE_PRIORITY + 5 )

typedef struct SensorSample
{
    int32_t distance_mm;
    int32_t speed_mmps;
    uint32_t sample_cycle;
    uint8_t emergency;
} SensorSample_t;

static volatile int32_t g_v_speed = INITIAL_SPEED_MMPS;
static volatile int32_t g_v_dist = INITIAL_DISTANCE_MM;
static volatile int32_t g_sensor_dist = INITIAL_DISTANCE_MM;
static volatile int32_t g_v_brake_decel = 0;
static volatile int g_v_done = 0; /* 0: running, 1: stopped safely, 2: collision */
static volatile uint8_t g_emergency_active = 0;
static volatile uint8_t g_threshold_detected = 0;
static volatile uint8_t g_emergency_detected = 0;

static uint32_t g_start_cycle = 0;
static uint32_t g_threshold_cycle = 0;
static uint32_t g_brake_cycle = 0;
static uint32_t g_emergency_detect_cycle = 0;
static uint32_t g_emergency_apply_cycle = 0;
static uint32_t g_final_cycle = 0;

static int32_t g_pid_integral = 0;
static int32_t g_pid_prev_error = 0;

static volatile uint32_t g_control_updates = 0;
static volatile uint32_t g_control_cycle_max = 0;
static volatile uint64_t g_control_cycle_sum = 0;
static volatile uint32_t g_control_jitter_min_cycles = UINT32_MAX;
static volatile uint32_t g_control_jitter_max_cycles = 0;
static volatile uint64_t g_control_jitter_sum_cycles = 0;
static volatile uint32_t g_control_jitter_samples = 0;
static volatile uint32_t g_deadline_miss_count = 0;
static volatile uint32_t g_sensor_to_control_cycle_max = 0;
static volatile uint64_t g_sensor_to_control_cycle_sum = 0;
static volatile uint32_t g_sensor_to_control_samples = 0;
static volatile uint32_t g_background_jobs = 0;
static volatile uint32_t g_background_cycle_max = 0;
static volatile uint64_t g_background_cycle_sum = 0;

#if ( USE_FREERTOS_MODE == 1 )
    static QueueHandle_t g_sensor_queue = NULL;
    static TaskHandle_t g_emergency_task_handle = NULL;
#else
    static SensorSample_t g_sensor_ring[ SENSOR_RING_LENGTH ];
    static volatile uint32_t g_sensor_ring_head = 0;
    static volatile uint32_t g_sensor_ring_tail = 0;
    static volatile uint32_t g_sensor_ring_count = 0;
    static volatile uint8_t g_bm_emergency_pending = 0;
#endif

static uint32_t read_cycle32( void )
{
#if defined( __riscv )
    uint32_t c;
    __asm volatile ( "rdcycle %0" : "=r"( c ) );
    return c;
#else
    return 0;
#endif
}

static uint32_t cycles_to_us( uint32_t cycles )
{
    return ( uint32_t ) ( ( ( uint64_t ) cycles * 1000000ULL ) / ( uint64_t ) configCPU_CLOCK_HZ );
}

static int32_t clamp_i32( int32_t value, int32_t minimum, int32_t maximum )
{
    if( value < minimum )
    {
        return minimum;
    }

    if( value > maximum )
    {
        return maximum;
    }

    return value;
}

static void ResetSimulationState( void )
{
    g_v_speed = INITIAL_SPEED_MMPS;
    g_v_dist = INITIAL_DISTANCE_MM;
    g_sensor_dist = INITIAL_DISTANCE_MM;
    g_v_brake_decel = 0;
    g_v_done = 0;
    g_emergency_active = 0;
    g_threshold_detected = 0;
    g_emergency_detected = 0;

    g_start_cycle = 0;
    g_threshold_cycle = 0;
    g_brake_cycle = 0;
    g_emergency_detect_cycle = 0;
    g_emergency_apply_cycle = 0;
    g_final_cycle = 0;

    g_pid_integral = 0;
    g_pid_prev_error = 0;

    g_control_updates = 0;
    g_control_cycle_max = 0;
    g_control_cycle_sum = 0;
    g_control_jitter_min_cycles = UINT32_MAX;
    g_control_jitter_max_cycles = 0;
    g_control_jitter_sum_cycles = 0;
    g_control_jitter_samples = 0;
    g_deadline_miss_count = 0;
    g_sensor_to_control_cycle_max = 0;
    g_sensor_to_control_cycle_sum = 0;
    g_sensor_to_control_samples = 0;
    g_background_jobs = 0;
    g_background_cycle_max = 0;
    g_background_cycle_sum = 0;

#if ( USE_FREERTOS_MODE == 0 )
    g_sensor_ring_head = 0;
    g_sensor_ring_tail = 0;
    g_sensor_ring_count = 0;
    g_bm_emergency_pending = 0;
#endif
}

void AlarmKickFromTickISR( void )
{
    /* Not used in this simulation. */
}

static void StepPhysics( void )
{
    const int32_t dt_ms = SIMULATION_STEP_MS;

    if( g_v_done != 0 )
    {
        return;
    }

    g_v_speed -= ( g_v_brake_decel * dt_ms ) / 1000;
    g_v_speed = clamp_i32( g_v_speed, 0, INITIAL_SPEED_MMPS );

    g_v_dist -= ( g_v_speed * dt_ms ) / 1000;

    if( g_v_dist <= 0 )
    {
        g_v_dist = 0;
        g_v_done = ( g_v_speed > SAFE_STOP_SPEED_MMPS ) ? 2 : 1;
    }
    else if( g_v_speed == 0 )
    {
        g_v_done = 1;
    }
}

static SensorSample_t BuildSensorSample( void )
{
    SensorSample_t sample;

    sample.distance_mm = g_v_dist;
    sample.speed_mmps = g_v_speed;
    sample.sample_cycle = read_cycle32();
    sample.emergency = ( uint8_t ) ( sample.distance_mm <= EMERGENCY_THRESHOLD_MM );

    g_sensor_dist = sample.distance_mm;

    if( ( g_threshold_detected == 0U ) && ( sample.distance_mm <= BRAKE_THRESHOLD_MM ) )
    {
        g_threshold_detected = 1U;
        g_threshold_cycle = sample.sample_cycle;
        printf( "THRESHOLD S:%d D:%d CYC:%u US:%u\n",
                ( int ) sample.speed_mmps,
                ( int ) sample.distance_mm,
                ( unsigned ) g_threshold_cycle,
                ( unsigned ) cycles_to_us( g_threshold_cycle - g_start_cycle ) );
    }

    if( ( g_emergency_detected == 0U ) && ( sample.emergency != 0U ) )
    {
        g_emergency_detected = 1U;
        g_emergency_detect_cycle = sample.sample_cycle;
        printf( "EMERG_DETECT S:%d D:%d CYC:%u US:%u\n",
                ( int ) sample.speed_mmps,
                ( int ) sample.distance_mm,
                ( unsigned ) g_emergency_detect_cycle,
                ( unsigned ) cycles_to_us( g_emergency_detect_cycle - g_start_cycle ) );
    }

    return sample;
}

static void RecordControlTiming( uint32_t runtime_cycles,
                                 uint32_t actual_period_cycles,
                                 uint32_t expected_period_cycles )
{
    uint32_t jitter_cycles;

    if( actual_period_cycles >= expected_period_cycles )
    {
        jitter_cycles = actual_period_cycles - expected_period_cycles;
    }
    else
    {
        jitter_cycles = expected_period_cycles - actual_period_cycles;
    }

    taskENTER_CRITICAL();
    {
        g_control_updates++;
        g_control_cycle_sum += runtime_cycles;

        if( runtime_cycles > g_control_cycle_max )
        {
            g_control_cycle_max = runtime_cycles;
        }

        if( jitter_cycles < g_control_jitter_min_cycles )
        {
            g_control_jitter_min_cycles = jitter_cycles;
        }

        if( jitter_cycles > g_control_jitter_max_cycles )
        {
            g_control_jitter_max_cycles = jitter_cycles;
        }

        g_control_jitter_sum_cycles += jitter_cycles;
        g_control_jitter_samples++;

        if( actual_period_cycles > expected_period_cycles )
        {
            g_deadline_miss_count++;
        }
    }
    taskEXIT_CRITICAL();
}

static void RecordSensorToControlLatency( uint32_t latency_cycles )
{
    taskENTER_CRITICAL();
    {
        g_sensor_to_control_cycle_sum += latency_cycles;
        g_sensor_to_control_samples++;

        if( latency_cycles > g_sensor_to_control_cycle_max )
        {
            g_sensor_to_control_cycle_max = latency_cycles;
        }
    }
    taskEXIT_CRITICAL();
}

static uint32_t RunBackgroundWorkSlice( void )
{
    volatile uint32_t mix = 0x13579BDFUL;
    uint32_t start_cycle = read_cycle32();
    uint32_t i;

    for( i = 0; i < BACKGROUND_WORK_ITERATIONS; i++ )
    {
        mix ^= ( mix << 5 );
        mix += ( i * 17U ) + ( mix >> 3 );
        mix ^= ( mix >> 11 );
    }

    return read_cycle32() - start_cycle;
}

static void RecordBackgroundWork( uint32_t elapsed_cycles )
{
    taskENTER_CRITICAL();
    {
        g_background_jobs++;
        g_background_cycle_sum += elapsed_cycles;

        if( elapsed_cycles > g_background_cycle_max )
        {
            g_background_cycle_max = elapsed_cycles;
        }
    }
    taskEXIT_CRITICAL();
}

static void ApplyBrakeIfFirst( const char * tag, int32_t decel )
{
    if( ( decel > 0 ) && ( g_brake_cycle == 0U ) )
    {
        g_brake_cycle = read_cycle32();
        printf( "%s S:%d D:%d DEC:%d CYC:%u US:%u\n",
                tag,
                ( int ) g_v_speed,
                ( int ) g_sensor_dist,
                ( int ) decel,
                ( unsigned ) g_brake_cycle,
                ( unsigned ) cycles_to_us( g_brake_cycle - g_start_cycle ) );
    }
}

static void ActivateEmergencyBrake( const char * tag )
{
    if( g_v_done != 0 )
    {
        return;
    }

    g_emergency_active = 1U;
    g_v_brake_decel = MAX_BRAKE_MMPS2;

    if( g_emergency_apply_cycle == 0U )
    {
        g_emergency_apply_cycle = read_cycle32();
        printf( "%s S:%d D:%d DEC:%d CYC:%u US:%u\n",
                tag,
                ( int ) g_v_speed,
                ( int ) g_sensor_dist,
                ( int ) g_v_brake_decel,
                ( unsigned ) g_emergency_apply_cycle,
                ( unsigned ) cycles_to_us( g_emergency_apply_cycle - g_start_cycle ) );
    }

    ApplyBrakeIfFirst( "BRAKE_EMERG", g_v_brake_decel );
}

static void RunDistancePidControl( const SensorSample_t * sample )
{
    int32_t distance_error;
    int32_t derivative;
    int32_t output;

    if( ( sample == NULL ) || ( g_v_done != 0 ) )
    {
        return;
    }

    g_sensor_dist = sample->distance_mm;

    if( g_emergency_active != 0U )
    {
        g_v_brake_decel = MAX_BRAKE_MMPS2;
        ApplyBrakeIfFirst( "BRAKE_PID", g_v_brake_decel );
        return;
    }

    if( sample->distance_mm > BRAKE_THRESHOLD_MM )
    {
        g_pid_integral = 0;
        g_pid_prev_error = 0;
        g_v_brake_decel = 0;
        return;
    }

    distance_error = BRAKE_THRESHOLD_MM - sample->distance_mm;
    g_pid_integral = clamp_i32( g_pid_integral + distance_error,
                                -PID_INTEGRAL_LIMIT,
                                PID_INTEGRAL_LIMIT );
    derivative = distance_error - g_pid_prev_error;
    g_pid_prev_error = distance_error;

    output = ( ( PID_KP * distance_error ) +
               ( PID_KI * g_pid_integral ) +
               ( PID_KD * derivative ) ) / PID_SCALE;
    g_v_brake_decel = clamp_i32( output, 0, MAX_BRAKE_MMPS2 );
    ApplyBrakeIfFirst( "BRAKE_PID", g_v_brake_decel );
}

static void PrintPeriodicReport( const char * mode_tag )
{
    uint32_t updates;
    uint32_t max_cycles;
    uint32_t avg_cycles;
    uint32_t jitter_min;
    uint32_t jitter_max;
    uint32_t jitter_avg;
    uint32_t deadline_misses;
    uint32_t handoff_max;
    uint32_t handoff_avg;
    uint32_t bg_jobs;
    uint32_t bg_max;
    uint32_t bg_avg;
    uint64_t sum_cycles;
    uint64_t jitter_sum;
    uint64_t handoff_sum;
    uint64_t bg_sum;
    uint32_t jitter_samples;
    uint32_t handoff_samples;

    taskENTER_CRITICAL();
    {
        updates = g_control_updates;
        max_cycles = g_control_cycle_max;
        sum_cycles = g_control_cycle_sum;
        jitter_min = g_control_jitter_min_cycles;
        jitter_max = g_control_jitter_max_cycles;
        jitter_sum = g_control_jitter_sum_cycles;
        jitter_samples = g_control_jitter_samples;
        deadline_misses = g_deadline_miss_count;
        handoff_max = g_sensor_to_control_cycle_max;
        handoff_sum = g_sensor_to_control_cycle_sum;
        handoff_samples = g_sensor_to_control_samples;
        bg_jobs = g_background_jobs;
        bg_max = g_background_cycle_max;
        bg_sum = g_background_cycle_sum;

        g_control_updates = 0;
        g_control_cycle_max = 0;
        g_control_cycle_sum = 0;
        g_control_jitter_min_cycles = UINT32_MAX;
        g_control_jitter_max_cycles = 0;
        g_control_jitter_sum_cycles = 0;
        g_control_jitter_samples = 0;
        g_deadline_miss_count = 0;
        g_sensor_to_control_cycle_max = 0;
        g_sensor_to_control_cycle_sum = 0;
        g_sensor_to_control_samples = 0;
        g_background_jobs = 0;
        g_background_cycle_max = 0;
        g_background_cycle_sum = 0;
    }
    taskEXIT_CRITICAL();

    avg_cycles = ( updates > 0U ) ? ( uint32_t ) ( sum_cycles / updates ) : 0U;
    jitter_avg = ( jitter_samples > 0U ) ? ( uint32_t ) ( jitter_sum / jitter_samples ) : 0U;
    handoff_avg = ( handoff_samples > 0U ) ? ( uint32_t ) ( handoff_sum / handoff_samples ) : 0U;
    bg_avg = ( bg_jobs > 0U ) ? ( uint32_t ) ( bg_sum / bg_jobs ) : 0U;
    if( jitter_min == UINT32_MAX )
    {
        jitter_min = 0U;
    }

    printf( "[%s] ctl_updates=%u exec_us max/avg=%u/%u jitter_us min/max/avg=%u/%u/%u handoff_us max/avg=%u/%u bg_jobs=%u bg_us max/avg=%u/%u miss=%u state S:%d D:%d DEC:%d EMG:%u\n",
            mode_tag,
            ( unsigned ) updates,
            ( unsigned ) cycles_to_us( max_cycles ),
            ( unsigned ) cycles_to_us( avg_cycles ),
            ( unsigned ) cycles_to_us( jitter_min ),
            ( unsigned ) cycles_to_us( jitter_max ),
            ( unsigned ) cycles_to_us( jitter_avg ),
            ( unsigned ) cycles_to_us( handoff_max ),
            ( unsigned ) cycles_to_us( handoff_avg ),
            ( unsigned ) bg_jobs,
            ( unsigned ) cycles_to_us( bg_max ),
            ( unsigned ) cycles_to_us( bg_avg ),
            ( unsigned ) deadline_misses,
            ( int ) g_v_speed,
            ( int ) g_v_dist,
            ( int ) g_v_brake_decel,
            ( unsigned ) g_emergency_active );
}

static void PrintFinalResultIfDone( const char * mode_tag )
{
    if( ( g_v_done == 0 ) || ( g_final_cycle != 0U ) )
    {
        return;
    }

    g_final_cycle = read_cycle32();

    printf( "FINAL[%s] %s S:%d D:%d DEC:%d T_US:%u\n",
            mode_tag,
            ( g_v_done == 1 ) ? "SAFE" : "CRASH",
            ( int ) g_v_speed,
            ( int ) g_v_dist,
            ( int ) g_v_brake_decel,
            ( unsigned ) cycles_to_us( g_final_cycle - g_start_cycle ) );

    printf( "METRIC[%s] brake_response_us=%u emergency_response_us=%u threshold_to_emergency_us=%u\n",
            mode_tag,
            ( unsigned ) cycles_to_us(
                ( ( g_brake_cycle >= g_threshold_cycle ) && ( g_threshold_cycle != 0U ) ) ?
                ( g_brake_cycle - g_threshold_cycle ) : 0U ),
            ( unsigned ) cycles_to_us(
                ( ( g_emergency_apply_cycle >= g_emergency_detect_cycle ) && ( g_emergency_detect_cycle != 0U ) ) ?
                ( g_emergency_apply_cycle - g_emergency_detect_cycle ) : 0U ),
            ( unsigned ) cycles_to_us(
                ( ( g_emergency_detect_cycle >= g_threshold_cycle ) && ( g_threshold_cycle != 0U ) ) ?
                ( g_emergency_detect_cycle - g_threshold_cycle ) : 0U ) );
}

#if ( USE_FREERTOS_MODE == 1 )

static void PhysicsTask( void * pv )
{
    TickType_t last_wake;

    ( void ) pv;
    last_wake = xTaskGetTickCount();

    for( ;; )
    {
        StepPhysics();
        vTaskDelayUntil( &last_wake, pdMS_TO_TICKS( PHYSICS_WAKE_MS ) );
    }
}

static void BackgroundTask( void * pv )
{
    ( void ) pv;

    for( ;; )
    {
        RecordBackgroundWork( RunBackgroundWorkSlice() );
        taskYIELD();
    }
}

static void SensorTask( void * pv )
{
    TickType_t last_wake;

    ( void ) pv;
    last_wake = xTaskGetTickCount();

    for( ;; )
    {
        SensorSample_t sample = BuildSensorSample();

        if( g_sensor_queue != NULL )
        {
            ( void ) xQueueOverwrite( g_sensor_queue, &sample );
        }

        if( ( sample.emergency != 0U ) && ( g_emergency_task_handle != NULL ) )
        {
            ( void ) xTaskNotifyGive( g_emergency_task_handle );
        }

        vTaskDelayUntil( &last_wake, pdMS_TO_TICKS( SENSOR_WAKE_MS ) );
    }
}

static void EmergencyTask( void * pv )
{
    ( void ) pv;

    for( ;; )
    {
        ulTaskNotifyTake( pdTRUE, portMAX_DELAY );
        ActivateEmergencyBrake( "EMERG_APPLY" );
    }
}

static void ControlTask( void * pv )
{
    const uint32_t expected_period_cycles =
        ( uint32_t ) ( ( ( uint64_t ) CONTROL_WAKE_MS * configCPU_CLOCK_HZ ) / 1000ULL );
    uint32_t prev_start_cycle = 0U;

    ( void ) pv;

    for( ;; )
    {
        SensorSample_t sample;
        uint32_t start_cycle;
        uint32_t runtime_cycles;
        uint32_t actual_period_cycles = expected_period_cycles;

        if( xQueueReceive( g_sensor_queue, &sample, portMAX_DELAY ) != pdPASS )
        {
            continue;
        }

        start_cycle = read_cycle32();
        RecordSensorToControlLatency( start_cycle - sample.sample_cycle );
        RunDistancePidControl( &sample );

        runtime_cycles = read_cycle32() - start_cycle;
        if( prev_start_cycle != 0U )
        {
            actual_period_cycles = start_cycle - prev_start_cycle;
        }

        RecordControlTiming( runtime_cycles, actual_period_cycles, expected_period_cycles );
        prev_start_cycle = start_cycle;
    }
}

static void MonitorTask( void * pv )
{
    TickType_t last_wake;

    ( void ) pv;
    last_wake = xTaskGetTickCount();

    printf( "\nAutonomous Braking Benchmark [%s]\n", "RTOS" );
    printf( "threshold_mm=%u emergency_mm=%u control_period_ms=%u cpu_hz=%u\n",
            ( unsigned ) BRAKE_THRESHOLD_MM,
            ( unsigned ) EMERGENCY_THRESHOLD_MM,
            ( unsigned ) CONTROL_WAKE_MS,
            ( unsigned ) configCPU_CLOCK_HZ );

    for( ;; )
    {
        PrintPeriodicReport( "RTOS" );

        if( g_v_done != 0 )
        {
            PrintFinalResultIfDone( "RTOS" );
            vTaskDelete( NULL );
        }

        vTaskDelayUntil( &last_wake, pdMS_TO_TICKS( MONITOR_PERIOD_MS ) );
    }
}

void main_blinky( void )
{
    ResetSimulationState();
    g_start_cycle = read_cycle32();

    printf( "START[%s] S:%d D:%d CYC:%u\n",
            "RTOS",
            ( int ) g_v_speed,
            ( int ) g_v_dist,
            ( unsigned ) g_start_cycle );

    g_sensor_queue = xQueueCreate( SENSOR_QUEUE_LENGTH, sizeof( SensorSample_t ) );
    configASSERT( g_sensor_queue != NULL );

    xTaskCreate( EmergencyTask, "EMG", 256, NULL, PRIO_EMERGENCY, &g_emergency_task_handle );
    xTaskCreate( ControlTask, "CTL", 256, NULL, PRIO_CONTROL, NULL );
    xTaskCreate( SensorTask, "SNS", 256, NULL, PRIO_SENSOR, NULL );
    xTaskCreate( PhysicsTask, "PHY", 256, NULL, PRIO_PHYSICS, NULL );
    xTaskCreate( BackgroundTask, "BG", 256, NULL, PRIO_BACKGROUND, NULL );
    xTaskCreate( MonitorTask, "MON", 512, NULL, PRIO_MONITOR, NULL );

    vTaskStartScheduler();
}

#else

static void PushSensorSampleToRing( const SensorSample_t * sample )
{
    if( sample == NULL )
    {
        return;
    }

    g_sensor_ring[ g_sensor_ring_head ] = *sample;
    g_sensor_ring_head = ( g_sensor_ring_head + 1U ) % SENSOR_RING_LENGTH;

    if( g_sensor_ring_count < SENSOR_RING_LENGTH )
    {
        g_sensor_ring_count++;
    }
    else
    {
        g_sensor_ring_tail = ( g_sensor_ring_tail + 1U ) % SENSOR_RING_LENGTH;
    }
}

static BaseType_t PopSensorSampleFromRing( SensorSample_t * sample )
{
    if( ( sample == NULL ) || ( g_sensor_ring_count == 0U ) )
    {
        return pdFAIL;
    }

    *sample = g_sensor_ring[ g_sensor_ring_tail ];
    g_sensor_ring_tail = ( g_sensor_ring_tail + 1U ) % SENSOR_RING_LENGTH;
    g_sensor_ring_count--;
    return pdPASS;
}

void main_blinky( void )
{
    const uint32_t physics_period_cycles =
        ( uint32_t ) ( ( ( uint64_t ) PHYSICS_WAKE_MS * configCPU_CLOCK_HZ ) / 1000ULL );
    const uint32_t sensor_period_cycles =
        ( uint32_t ) ( ( ( uint64_t ) SENSOR_WAKE_MS * configCPU_CLOCK_HZ ) / 1000ULL );
    const uint32_t control_period_cycles =
        ( uint32_t ) ( ( ( uint64_t ) CONTROL_WAKE_MS * configCPU_CLOCK_HZ ) / 1000ULL );
    const uint32_t monitor_period_cycles =
        ( uint32_t ) ( ( ( uint64_t ) MONITOR_PERIOD_MS * configCPU_CLOCK_HZ ) / 1000ULL );
    uint32_t now;
    uint32_t next_physics_cycle;
    uint32_t next_sensor_cycle;
    uint32_t next_control_cycle;
    uint32_t next_monitor_cycle;
    uint32_t prev_control_start_cycle = 0U;

    ResetSimulationState();
    g_start_cycle = read_cycle32();
    now = g_start_cycle;
    next_physics_cycle = g_start_cycle + physics_period_cycles;
    next_sensor_cycle = g_start_cycle + sensor_period_cycles;
    next_control_cycle = g_start_cycle + control_period_cycles;
    next_monitor_cycle = g_start_cycle + monitor_period_cycles;

    printf( "START[%s] S:%d D:%d CYC:%u\n",
            "BARE",
            ( int ) g_v_speed,
            ( int ) g_v_dist,
            ( unsigned ) g_start_cycle );
    printf( "\nAutonomous Braking Benchmark [%s]\n", "BARE" );
    printf( "threshold_mm=%u emergency_mm=%u control_period_ms=%u cpu_hz=%u\n",
            ( unsigned ) BRAKE_THRESHOLD_MM,
            ( unsigned ) EMERGENCY_THRESHOLD_MM,
            ( unsigned ) CONTROL_WAKE_MS,
            ( unsigned ) configCPU_CLOCK_HZ );

    for( ;; )
    {
        now = read_cycle32();

        if( ( int32_t ) ( now - next_physics_cycle ) >= 0 )
        {
            StepPhysics();
            next_physics_cycle += physics_period_cycles;
        }

        if( ( int32_t ) ( now - next_sensor_cycle ) >= 0 )
        {
            SensorSample_t sample = BuildSensorSample();
            PushSensorSampleToRing( &sample );

            if( sample.emergency != 0U )
            {
                g_bm_emergency_pending = 1U;
            }

            next_sensor_cycle += sensor_period_cycles;
        }

        if( g_bm_emergency_pending != 0U )
        {
            g_bm_emergency_pending = 0U;
            ActivateEmergencyBrake( "EMERG_APPLY" );
        }

        if( ( int32_t ) ( now - next_control_cycle ) >= 0 )
        {
            SensorSample_t sample;
            uint32_t start_cycle = read_cycle32();
            uint32_t runtime_cycles;
            uint32_t actual_period_cycles = control_period_cycles;

            if( PopSensorSampleFromRing( &sample ) == pdPASS )
            {
                RunDistancePidControl( &sample );
            }
            else
            {
                SensorSample_t fallback = BuildSensorSample();
                RunDistancePidControl( &fallback );
            }

            runtime_cycles = read_cycle32() - start_cycle;
            if( prev_control_start_cycle != 0U )
            {
                actual_period_cycles = start_cycle - prev_control_start_cycle;
            }

            RecordControlTiming( runtime_cycles, actual_period_cycles, control_period_cycles );
            prev_control_start_cycle = start_cycle;
            next_control_cycle += control_period_cycles;
        }

        if( ( int32_t ) ( now - next_monitor_cycle ) >= 0 )
        {
            PrintPeriodicReport( "BARE" );

            if( g_v_done != 0 )
            {
                PrintFinalResultIfDone( "BARE" );
                break;
            }

            next_monitor_cycle += monitor_period_cycles;
        }

        RecordBackgroundWork( RunBackgroundWorkSlice() );
    }

    for( ;; )
    {
    }
}

#endif
