/*
 * ASCII Mandelbrot. MANDEL=single runs both halves on hart 0.
 * MANDEL=smp pins rows 0-9 to hart 0 and rows 10-19 to hart 1.
 *
 * compute_cycles is the mtime from releasing the two tasks until both
 * have finished. Printing happens after that.
 * Integer fixed point only. One unit is 1/256. No 64-bit divide.
 */
#include <stdint.h>
#include <stdio.h>

#include "FreeRTOS.h"
#include "semphr.h"
#include "task.h"

#define ROWS      20
#define COLS      40
#define HALF      10
#define MAX_ITER  32
#define ONE       256
#define ESCAPE    ( 4 * ONE * ONE )

static char frame[ ROWS ][ COLS ];
static StaticSemaphore_t xGoBuf[ 2 ];
static StaticSemaphore_t xDoneBuf[ 2 ];
static SemaphoreHandle_t xGo[ 2 ];
static SemaphoreHandle_t xDone[ 2 ];

static uint64_t prvReadMtime( void )
{
    volatile uint32_t * hi = ( volatile uint32_t * ) ( CLINT_ADDR + CLINT_MTIME + 4u );
    volatile uint32_t * lo = ( volatile uint32_t * ) ( CLINT_ADDR + CLINT_MTIME );
    uint32_t high;
    uint32_t low;

    do
    {
        high = *hi;
        low = *lo;
    } while( high != *hi );

    return ( ( uint64_t ) high << 32 ) | ( uint64_t ) low;
}

static char prvPixel( int col, int row )
{
    int x0 = ( -2 * ONE ) + ( col * ( 3 * ONE ) ) / ( COLS - 1 );
    int y0 = ( -1 * ONE ) + ( row * ( 2 * ONE ) ) / ( ROWS - 1 );
    int x = 0;
    int y = 0;
    int n;

    for( n = 0; n < MAX_ITER; n++ )
    {
        int xx;
        int yy;

        if( ( x > 2000 ) || ( x < -2000 ) || ( y > 2000 ) || ( y < -2000 ) )
        {
            break;
        }

        xx = x * x;
        yy = y * y;

        if( ( xx + yy ) > ESCAPE )
        {
            break;
        }

        y = ( ( x * y ) >> 7 ) + y0;
        x = ( ( xx - yy ) >> 8 ) + x0;
    }

    if( n == MAX_ITER )
    {
        return '@';
    }

    if( n > 16 )
    {
        return '*';
    }

    if( n > 8 )
    {
        return '+';
    }

    if( n > 3 )
    {
        return '.';
    }

    return ' ';
}

static void prvFill( int row0 )
{
    int row;
    int col;

    for( row = row0; row < ( row0 + HALF ); row++ )
    {
        for( col = 0; col < COLS; col++ )
        {
            frame[ row ][ col ] = prvPixel( col, row );
        }
    }
}

static void prvCompute( void * pv )
{
    int half = ( int ) ( uintptr_t ) pv;

    xSemaphoreTake( xGo[ half ], portMAX_DELAY );
    __asm volatile ( "fence rw, rw" ::: "memory" );
    portDISABLE_INTERRUPTS();
    prvFill( half * HALF );
    portENABLE_INTERRUPTS();
    __asm volatile ( "fence rw, rw" ::: "memory" );
    xSemaphoreGive( xDone[ half ] );
    vTaskSuspend( NULL );
}

static void prvPrint( void * pv )
{
    uint64_t t0;
    uint64_t t1;
    int row;
    char line[ COLS + 1 ];

    ( void ) pv;

    t0 = prvReadMtime();
    __asm volatile ( "fence rw, rw" ::: "memory" );
    xSemaphoreGive( xGo[ 0 ] );
    xSemaphoreGive( xGo[ 1 ] );

    xSemaphoreTake( xDone[ 0 ], portMAX_DELAY );
    xSemaphoreTake( xDone[ 1 ], portMAX_DELAY );
    __asm volatile ( "fence rw, rw" ::: "memory" );
    t1 = prvReadMtime();

#if defined( MANDEL_MODE_SMP )
    printf( "MODE smp\n" );
#else
    printf( "MODE single\n" );
#endif
    printf( "compute_cycles %u\n", ( unsigned ) ( t1 - t0 ) );

    line[ COLS ] = '\0';

    for( row = 0; row < ROWS; row++ )
    {
        int col;

        for( col = 0; col < COLS; col++ )
        {
            line[ col ] = frame[ row ][ col ];
        }

        printf( "%s\n", line );
    }

    for( ; ; )
    {
        __asm volatile ( "nop" );
    }
}

void main_mandelbrot( void )
{
    int i;

    printf( "Get into mandelbrot\n" );

    for( i = 0; i < 2; i++ )
    {
        xGo[ i ] = xSemaphoreCreateBinaryStatic( &xGoBuf[ i ] );
        xDone[ i ] = xSemaphoreCreateBinaryStatic( &xDoneBuf[ i ] );
        configASSERT( xGo[ i ] != NULL );
        configASSERT( xDone[ i ] != NULL );
    }

#if defined( MANDEL_MODE_SMP )
    configASSERT( xTaskCreateAffinitySet( prvCompute, "top", configMINIMAL_STACK_SIZE * 4, ( void * ) 0, tskIDLE_PRIORITY + 1, ( 1U << 0 ), NULL ) == pdPASS );
    configASSERT( xTaskCreateAffinitySet( prvCompute, "bot", configMINIMAL_STACK_SIZE * 4, ( void * ) 1, tskIDLE_PRIORITY + 1, ( 1U << 1 ), NULL ) == pdPASS );
    configASSERT( xTaskCreateAffinitySet( prvPrint, "print", configMINIMAL_STACK_SIZE * 4, NULL, tskIDLE_PRIORITY + 2, ( 1U << 0 ), NULL ) == pdPASS );
#else
    configASSERT( xTaskCreate( prvCompute, "top", configMINIMAL_STACK_SIZE * 4, ( void * ) 0, tskIDLE_PRIORITY + 1, NULL ) == pdPASS );
    configASSERT( xTaskCreate( prvCompute, "bot", configMINIMAL_STACK_SIZE * 4, ( void * ) 1, tskIDLE_PRIORITY + 1, NULL ) == pdPASS );
    configASSERT( xTaskCreate( prvPrint, "print", configMINIMAL_STACK_SIZE * 4, NULL, tskIDLE_PRIORITY + 2, NULL ) == pdPASS );
#endif

    vTaskStartScheduler();

    printf( "Scheduler did not start\n" );
    for( ; ; )
    {
    }
}
