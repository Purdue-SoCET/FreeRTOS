#include <stdint.h>
#include <stdio.h>

#include "FreeRTOS.h"
#include "task.h"

#define USE_FREERTOS_MODE         1

#define INITIAL_SPEED_MMPS        9000
#define INITIAL_DISTANCE_MM       20000
#define BRAKE_THRESHOLD_MM        10000
#define SAFE_STOP_SPEED_MMPS      500
#define MAX_BRAKE_MMPS2           12000

#define PHYSICS_WAKE_MS           10
#define SIMULATION_STEP_MS        250
#define SENSOR_WAKE_MS            10
#define CONTROL_WAKE_MS           10
#define MONITOR_PERIOD_MS         20

#define REPORT_WINDOW_TICKS       10U
#define REPORT_SAMPLE_COUNT       5U

/*
 * Integer PID used as the benchmark workload.
 * output_mmps2 = ( KP * error + KI * integral + KD * derivative ) / PID_SCALE
 */
#define PID_KP                    2
#define PID_KI                    1
#define PID_KD                    8
#define PID_SCALE                 8
#define PID_INTEGRAL_LIMIT        50000

static volatile int32_t g_v_speed = INITIAL_SPEED_MMPS;
static volatile int32_t g_v_dist = INITIAL_DISTANCE_MM;
static volatile int32_t g_sensor_dist = INITIAL_DISTANCE_MM;
static volatile int32_t g_v_brake_decel = 0;
static volatile int g_v_done = 0; /* 0: running, 1: stopped safely, 2: collision */

static uint32_t g_start_cycle = 0;
static uint32_t g_threshold_cycle = 0;
static uint32_t g_brake_cycle = 0;
static uint32_t g_final_cycle = 0;

static volatile int g_threshold_detected = 0;
static int32_t g_pid_integral = 0;
static int32_t g_pid_prev_error = 0;

static volatile uint32_t g_pid_updates = 0;
static volatile uint32_t g_pid_cycle_max = 0;
static volatile uint64_t g_pid_cycle_sum = 0;

static volatile uint32_t g_control_jitter_min_cycles = UINT32_MAX;
static volatile uint32_t g_control_jitter_max_cycles = 0;
static volatile uint64_t g_control_jitter_sum_cycles = 0;
static volatile uint32_t g_control_jitter_samples = 0;

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

static void RecordControlJitter( uint32_t elapsed_cycles, uint32_t expected_cycles )
{
    uint32_t jitter_cycles;

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
    }
    taskEXIT_CRITICAL();
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
}

static void SampleDistance( void )
{
    g_sensor_dist = g_v_dist;

    if( ( g_threshold_detected == 0 ) && ( g_sensor_dist <= BRAKE_THRESHOLD_MM ) )
    {
        g_threshold_detected = 1;
        g_threshold_cycle = read_cycle32();
        printf( "THRESHOLD S:%d D:%d CYC:%u US:%u\n",
                ( int ) g_v_speed,
                ( int ) g_sensor_dist,
                ( unsigned ) g_threshold_cycle,
                ( unsigned ) cycles_to_us( g_threshold_cycle - g_start_cycle ) );
    }
}

static void RunDistancePidControl( void )
{
    uint32_t start_cycle;
    uint32_t elapsed_cycles;
    int32_t distance_error;
    int32_t derivative;
    int32_t output;

    if( g_v_done != 0 )
    {
        return;
    }

    start_cycle = read_cycle32();

    if( g_sensor_dist > BRAKE_THRESHOLD_MM )
    {
        g_pid_integral = 0;
        g_pid_prev_error = 0;
        g_v_brake_decel = 0;
    }
    else
    {
        distance_error = BRAKE_THRESHOLD_MM - g_sensor_dist;
        g_pid_integral = clamp_i32( g_pid_integral + distance_error,
                                    -PID_INTEGRAL_LIMIT,
                                    PID_INTEGRAL_LIMIT );
        derivative = distance_error - g_pid_prev_error;
        g_pid_prev_error = distance_error;

        output = ( ( PID_KP * distance_error ) +
                   ( PID_KI * g_pid_integral ) +
                   ( PID_KD * derivative ) ) / PID_SCALE;
        g_v_brake_decel = clamp_i32( output, 0, MAX_BRAKE_MMPS2 );

        if( ( g_v_brake_decel > 0 ) && ( g_brake_cycle == 0U ) )
        {
            g_brake_cycle = read_cycle32();
            printf( "BRAKE_PID S:%d D:%d DEC:%d CYC:%u US:%u\n",
                    ( int ) g_v_speed,
                    ( int ) g_sensor_dist,
                    ( int ) g_v_brake_decel,
                    ( unsigned ) g_brake_cycle,
                    ( unsigned ) cycles_to_us( g_brake_cycle - g_start_cycle ) );
        }
    }

    elapsed_cycles = read_cycle32() - start_cycle;

    taskENTER_CRITICAL();
    {
        g_pid_updates++;
        g_pid_cycle_sum += elapsed_cycles;

        if( elapsed_cycles > g_pid_cycle_max )
        {
            g_pid_cycle_max = elapsed_cycles;
        }
    }
    taskEXIT_CRITICAL();
}

static void PrintFinalResultIfDone( void )
{
    if( ( g_v_done != 0 ) && ( g_final_cycle == 0U ) )
    {
        const uint32_t response_cycles =
            ( ( g_brake_cycle >= g_threshold_cycle ) && ( g_threshold_cycle != 0U ) ) ?
            ( g_brake_cycle - g_threshold_cycle ) : 0U;

        g_final_cycle = read_cycle32();

        printf( "FINAL %s S:%d D:%d DEC:%d CYC:%u US:%u\n",
                ( g_v_done == 1 ) ? "SAFE" : "CRASH",
                ( int ) g_v_speed,
                ( int ) g_v_dist,
                ( int ) g_v_brake_decel,
                ( unsigned ) g_final_cycle,
                ( unsigned ) cycles_to_us( g_final_cycle - g_start_cycle ) );
        printf( "PID_LATENCY CYC:%u US:%u\n",
                ( unsigned ) response_cycles,
                ( unsigned ) cycles_to_us( response_cycles ) );
    }
}

#if ( USE_FREERTOS_MODE == 1 )

static void PhysicsTask( void * pv )
{
    ( void ) pv;

    for( ;; )
    {
        StepPhysics();
        vTaskDelay( pdMS_TO_TICKS( PHYSICS_WAKE_MS ) );
    }
}

static void SensorTask( void * pv )
{
    ( void ) pv;

    for( ;; )
    {
        SampleDistance();
        vTaskDelay( pdMS_TO_TICKS( SENSOR_WAKE_MS ) );
    }
}

static void ControlTask( void * pv )
{
    const uint32_t expected_cycles =
        ( uint32_t ) ( ( ( uint64_t ) CONTROL_WAKE_MS * configCPU_CLOCK_HZ ) / 1000ULL );
    TickType_t last_wake_time = xTaskGetTickCount();
    uint32_t previous_release_cycle = read_cycle32();

    ( void ) pv;

    for( ;; )
    {
        uint32_t now;

        xTaskDelayUntil( &last_wake_time, pdMS_TO_TICKS( CONTROL_WAKE_MS ) );

        now = read_cycle32();
        RecordControlJitter( now - previous_release_cycle, expected_cycles );
        previous_release_cycle = now;
        RunDistancePidControl();
    }
}

static void MonitorTask( void * pv )
{
    uint32_t report_count = 0;

    ( void ) pv;

    printf( "\nDistance PID Throughput Mode\n" );
    printf( "threshold_mm=%u control_period_ms=%u cpu_hz=%u\n",
            ( unsigned ) BRAKE_THRESHOLD_MM,
            ( unsigned ) CONTROL_WAKE_MS,
            ( unsigned ) configCPU_CLOCK_HZ );

    for( ;; )
    {
        uint32_t updates;
        uint32_t max_cycles;
        uint32_t avg_cycles;
        uint32_t jitter_min_cycles;
        uint32_t jitter_max_cycles;
        uint32_t jitter_avg_cycles;
        uint32_t jitter_samples;
        uint64_t sum_cycles;
        uint64_t jitter_sum_cycles;

        vTaskDelay( REPORT_WINDOW_TICKS );

        taskENTER_CRITICAL();
        {
            updates = g_pid_updates;
            max_cycles = g_pid_cycle_max;
            sum_cycles = g_pid_cycle_sum;
            jitter_min_cycles = g_control_jitter_min_cycles;
            jitter_max_cycles = g_control_jitter_max_cycles;
            jitter_sum_cycles = g_control_jitter_sum_cycles;
            jitter_samples = g_control_jitter_samples;
            g_pid_updates = 0;
            g_pid_cycle_max = 0;
            g_pid_cycle_sum = 0;
            g_control_jitter_min_cycles = UINT32_MAX;
            g_control_jitter_max_cycles = 0;
            g_control_jitter_sum_cycles = 0;
            g_control_jitter_samples = 0;
        }
        taskEXIT_CRITICAL();

        avg_cycles = ( updates > 0U ) ? ( uint32_t ) ( sum_cycles / updates ) : 0U;
        jitter_avg_cycles = ( jitter_samples > 0U ) ? ( uint32_t ) ( jitter_sum_cycles / jitter_samples ) : 0U;

        printf( "[PID] updates/window=%u updates/sec=%u cyc/update max/avg=%u/%u state S:%d D:%d DEC:%d\n",
                ( unsigned ) updates,
                ( unsigned ) ( ( updates * configTICK_RATE_HZ ) / REPORT_WINDOW_TICKS ),
                ( unsigned ) max_cycles,
                ( unsigned ) avg_cycles,
                ( int ) g_v_speed,
                ( int ) g_v_dist,
                ( int ) g_v_brake_decel );
        printf( "[JITTER] control min/max/avg=%u/%u/%u us samples=%u\n",
                ( unsigned ) ( ( jitter_samples > 0U ) ? jitter_min_cycles : 0U ),
                ( unsigned ) jitter_max_cycles ,
                ( unsigned ) jitter_avg_cycles ,
                ( unsigned ) jitter_samples );

        report_count++;

        if( g_v_done != 0 )
        {
            PrintFinalResultIfDone();
            vTaskDelay( pdMS_TO_TICKS( 100U ) );
            vTaskDelete( NULL );
        }

        if( report_count >= REPORT_SAMPLE_COUNT )
        {
            report_count = 0;
        }

        vTaskDelay( pdMS_TO_TICKS( MONITOR_PERIOD_MS ) );
    }
}

void main_blinky( void )
{
    g_start_cycle = read_cycle32();
    printf( "START S:%d D:%d CYC:%u\n",
            ( int ) g_v_speed,
            ( int ) g_v_dist,
            ( unsigned ) g_start_cycle );

    xTaskCreate( PhysicsTask, "PHY", 256, NULL, tskIDLE_PRIORITY + 3, NULL );
    xTaskCreate( SensorTask, "SNS", 256, NULL, tskIDLE_PRIORITY + 2, NULL );
    xTaskCreate( ControlTask, "CTL", 256, NULL, tskIDLE_PRIORITY + 4, NULL );
    xTaskCreate( MonitorTask, "MON", 512, NULL, tskIDLE_PRIORITY + 1, NULL );

    vTaskStartScheduler();
}

#else

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
    const uint32_t report_window_cycles =
        ( uint32_t ) ( ( ( uint64_t ) REPORT_WINDOW_TICKS * configCPU_CLOCK_HZ ) / configTICK_RATE_HZ );
    uint32_t now;
    uint32_t last_control_release_cycle;
    uint32_t next_physics_cycle;
    uint32_t next_sensor_cycle;
    uint32_t next_control_cycle;
    uint32_t next_monitor_cycle;
    uint32_t next_report_cycle;

    g_start_cycle = read_cycle32();
    now = g_start_cycle;
    next_physics_cycle = g_start_cycle + physics_period_cycles;
    next_sensor_cycle = g_start_cycle + sensor_period_cycles;
    next_control_cycle = g_start_cycle + control_period_cycles;
    next_monitor_cycle = g_start_cycle + monitor_period_cycles;
    next_report_cycle = g_start_cycle + report_window_cycles;
    last_control_release_cycle = g_start_cycle;

    printf( "START S:%d D:%d CYC:%u\n",
            ( int ) g_v_speed,
            ( int ) g_v_dist,
            ( unsigned ) g_start_cycle );
    printf( "\nDistance PID Throughput Mode\n" );
    printf( "threshold_mm=%u control_period_ms=%u cpu_hz=%u\n",
            ( unsigned ) BRAKE_THRESHOLD_MM,
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
            SampleDistance();
            next_sensor_cycle += sensor_period_cycles;
        }

        if( ( int32_t ) ( now - next_control_cycle ) >= 0 )
        {
            RecordControlJitter( now - last_control_release_cycle, control_period_cycles );
            last_control_release_cycle = now;
            RunDistancePidControl();
            next_control_cycle += control_period_cycles;
        }

        if( ( int32_t ) ( now - next_report_cycle ) >= 0 )
        {
            uint32_t updates;
            uint32_t max_cycles;
            uint32_t avg_cycles;
            uint32_t jitter_min_cycles;
            uint32_t jitter_max_cycles;
            uint32_t jitter_avg_cycles;
            uint32_t jitter_samples;
            uint64_t sum_cycles;
            uint64_t jitter_sum_cycles;

            updates = g_pid_updates;
            max_cycles = g_pid_cycle_max;
            sum_cycles = g_pid_cycle_sum;
            jitter_min_cycles = g_control_jitter_min_cycles;
            jitter_max_cycles = g_control_jitter_max_cycles;
            jitter_sum_cycles = g_control_jitter_sum_cycles;
            jitter_samples = g_control_jitter_samples;
            g_pid_updates = 0;
            g_pid_cycle_max = 0;
            g_pid_cycle_sum = 0;
            g_control_jitter_min_cycles = UINT32_MAX;
            g_control_jitter_max_cycles = 0;
            g_control_jitter_sum_cycles = 0;
            g_control_jitter_samples = 0;
            avg_cycles = ( updates > 0U ) ? ( uint32_t ) ( sum_cycles / updates ) : 0U;
            jitter_avg_cycles = ( jitter_samples > 0U ) ? ( uint32_t ) ( jitter_sum_cycles / jitter_samples ) : 0U;

            printf( "[PID] updates/window=%u updates/sec=%u cyc/update max/avg=%u/%u state S:%d D:%d DEC:%d\n",
                    ( unsigned ) updates,
                    ( unsigned ) ( ( updates * configTICK_RATE_HZ ) / REPORT_WINDOW_TICKS ),
                    ( unsigned ) max_cycles,
                    ( unsigned ) avg_cycles,
                    ( int ) g_v_speed,
                    ( int ) g_v_dist,
                    ( int ) g_v_brake_decel );
            printf( "[JITTER] control min/max/avg=%u/%u/%u us samples=%u\n",
                    ( unsigned ) ( ( jitter_samples > 0U ) ? jitter_min_cycles : 0U ),
                    ( unsigned ) jitter_max_cycles,
                    ( unsigned ) jitter_avg_cycles,
                    ( unsigned ) jitter_samples );

            next_report_cycle += report_window_cycles;
        }

        if( ( int32_t ) ( now - next_monitor_cycle ) >= 0 )
        {
            if( g_v_done != 0 )
            {
                PrintFinalResultIfDone();
                break;
            }

            next_monitor_cycle += monitor_period_cycles;
        }

        if( g_v_done != 0 )
        {
            if( ( int32_t ) ( now - next_monitor_cycle ) < 0 )
            {
                PrintFinalResultIfDone();
                break;
            }
        }
    }

    for( ;; )
    {
    }
}

#endif
