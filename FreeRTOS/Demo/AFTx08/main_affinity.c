/*
 * Two tasks, same priority. Task A may run only on hart 0.
 * Task B may run only on hart 1. Each blocks in vTaskDelay, so the
 * scheduler has a chance to move it. A passing run keeps A on hart 0
 * and B on hart 1 after every wake.
 *
 * xTaskCreateAffinitySet() is a FreeRTOS SMP API, declared in task.h.
 * It is compiled only when configNUMBER_OF_CORES > 1 and
 * configUSE_CORE_AFFINITY is 1. The arguments match xTaskCreate(), plus uxCoreAffinityMask.
 * Bit 0 of that mask is hart 0, bit 1 is hart 1.
 *
 * Inside tasks.c it calls prvCreateTask(), the same helper xTaskCreate()
 * uses to allocate the task and its stack. It then stores the mask in
 * the task's TCB (uxCoreAffinityMask) and calls prvAddNewTaskToReadyList().
 * When a hart later picks a task, prvSelectHighestPriorityTask() skips
 * any task whose mask does not include that hart. xTaskCreate() is still
 * available; those tasks get tskNO_AFFINITY, which is every hart.
 */

#include <stdio.h>

#include "FreeRTOS.h"
#include "task.h"
#include "arch/cpuid.h"

#define mainHART_TASK_PRIORITY    ( tskIDLE_PRIORITY + 1 )
#define mainHART_TASK_PERIOD_MS   pdMS_TO_TICKS( 10 )

static void prvHartTask( void * pvParameters );

void main_affinity( void )
{
    printf( "Get into main_affinity\n" );

    if( xTaskCreateAffinitySet( prvHartTask, "A", configMINIMAL_STACK_SIZE, ( void * ) "A", mainHART_TASK_PRIORITY, ( 1U << 0 ), NULL ) != pdPASS )
    {
        printf( "Failed to create task A\n" );
    }

    if( xTaskCreateAffinitySet( prvHartTask, "B", configMINIMAL_STACK_SIZE, ( void * ) "B", mainHART_TASK_PRIORITY, ( 1U << 1 ), NULL ) != pdPASS )
    {
        printf( "Failed to create task B\n" );
    }

    printf( "Done created pinned tasks\n" );
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
