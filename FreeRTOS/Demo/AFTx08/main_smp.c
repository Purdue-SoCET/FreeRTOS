/*
 * Two tasks, same priority, no hart pinned. Each prints mhartid.
 * A passing run shows both "hart 0" and "hart 1". Hart 1 sitting only
 * in the idle task would print hart 0 forever.
 */

#include <stdio.h>

#include "FreeRTOS.h"
#include "task.h"
#include "arch/cpuid.h"

#define mainHART_TASK_PRIORITY    ( tskIDLE_PRIORITY + 1 )
#define mainHART_TASK_PERIOD_MS   pdMS_TO_TICKS( 100 )

static void prvHartTask( void * pvParameters );

void main_smp( void )
{
    printf( "Get into main_smp\n" );

    if( xTaskCreate( prvHartTask, "A", configMINIMAL_STACK_SIZE, ( void * ) "A", mainHART_TASK_PRIORITY, NULL ) != pdPASS )
    {
        printf( "Failed to create task A\n" );
    }

    if( xTaskCreate( prvHartTask, "B", configMINIMAL_STACK_SIZE, ( void * ) "B", mainHART_TASK_PRIORITY, NULL ) != pdPASS )
    {
        printf( "Failed to create task B\n" );
    }

    printf( "Done created hart tasks\n" );
    vTaskStartScheduler();

    printf( "Scheduler did not start\n" );
    for( ; ; )
    {
    }
}

static void prvHartTask( void * pvParameters )
{
    const char * pcName = ( const char * ) pvParameters;
    uint32_t ulCount = 0;

    for( ; ; )
    {
        printf( "hart %d task %s count %u\n", arch_cpuid(), pcName, ( unsigned int ) ulCount );
        ulCount++;
        vTaskDelay( mainHART_TASK_PERIOD_MS );
    }
}
