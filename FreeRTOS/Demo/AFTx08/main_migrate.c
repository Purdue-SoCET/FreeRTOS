/*
 * Task M is created with xTaskCreate(), so its affinity is
 * tskNO_AFFINITY and either hart may run it. H0 and H1 are the same
 * priority. H0 is pinned to hart 0 and never blocks, so M wakes on
 * hart 1. H1 is pinned to hart 1 and starts suspended.
 *
 * After three wakes H0 resumes H1 and suspends itself. H1 then holds
 * hart 1, so M's next wake is on hart 0. That is the migration.
 *
 * The tasks share tskIDLE_PRIORITY + 1. configRUN_MULTIPLE_PRIORITIES
 * is 0, so a higher-priority task would keep the others off both
 * harts. H0 is created first and is taken by hart 0 at startup.
 *
 * A passing run prints M on hart 1, then one line marked
 * MIGRATED 1->0, then M on hart 0, then MIGRATION_OK.
 */

#include <stdio.h>

#include "FreeRTOS.h"
#include "task.h"
#include "arch/cpuid.h"

#define mainTASK_PRIORITY       ( tskIDLE_PRIORITY + 1 )
#define mainTASK_PERIOD         pdMS_TO_TICKS( 10 )
#define mainPRINTS_BEFORE_MOVE  3
#define mainTOTAL_PRINTS        6

static volatile uint32_t ulMigrantPrints;
static TaskHandle_t xHoldHart1;

static void prvMigrantTask( void * pvParameters );
static void prvHoldHart0( void * pvParameters );
static void prvHoldHart1( void * pvParameters );

void main_migrate( void )
{
    printf( "Get into main_migrate\n" );

    if( xTaskCreateAffinitySet( prvHoldHart0, "H0", configMINIMAL_STACK_SIZE, NULL, mainTASK_PRIORITY, ( 1U << 0 ), NULL ) != pdPASS )
    {
        printf( "Failed to create hold task H0\n" );
    }

    if( xTaskCreateAffinitySet( prvHoldHart1, "H1", configMINIMAL_STACK_SIZE, NULL, mainTASK_PRIORITY, ( 1U << 1 ), &xHoldHart1 ) != pdPASS )
    {
        printf( "Failed to create hold task H1\n" );
    }
    else
    {
        vTaskSuspend( xHoldHart1 );
    }

    if( xTaskCreate( prvMigrantTask, "M", configMINIMAL_STACK_SIZE, NULL, mainTASK_PRIORITY, NULL ) != pdPASS )
    {
        printf( "Failed to create migrant task\n" );
    }

    printf( "Done created tasks\n" );
    vTaskStartScheduler();

    printf( "Scheduler did not start\n" );
    for( ; ; )
    {
    }
}

static void prvHoldHart0( void * pvParameters )
{
    ( void ) pvParameters;

    while( ulMigrantPrints < mainPRINTS_BEFORE_MOVE )
    {
        __asm volatile ( "nop" );
    }

    vTaskResume( xHoldHart1 );
    vTaskSuspend( NULL );
}

static void prvHoldHart1( void * pvParameters )
{
    ( void ) pvParameters;

    for( ; ; )
    {
        __asm volatile ( "nop" );
    }
}

static void prvMigrantTask( void * pvParameters )
{
    int lLastHart = -1;
    uint32_t ulCount = 0;
    uint32_t ulMigrations = 0;

    ( void ) pvParameters;

    for( ; ; )
    {
        int lHart = arch_cpuid();

        if( ( lLastHart >= 0 ) && ( lHart != lLastHart ) )
        {
            printf( "hart %d task M count %u MIGRATED %d->%d\n",
                    lHart, ( unsigned int ) ulCount, lLastHart, lHart );
            ulMigrations++;
        }
        else
        {
            printf( "hart %d task M count %u\n", lHart, ( unsigned int ) ulCount );
        }

        lLastHart = lHart;
        ulCount++;
        ulMigrantPrints = ulCount;

        if( ulCount >= mainTOTAL_PRINTS )
        {
            if( ( ulMigrations == 1U ) && ( lHart == 0 ) )
            {
                printf( "MIGRATION_OK\n" );
            }
            else
            {
                printf( "MIGRATION_FAIL migrations %u hart %d\n",
                        ( unsigned int ) ulMigrations, lHart );
            }

            vTaskSuspend( NULL );
        }

        vTaskDelay( mainTASK_PERIOD );
    }
}
