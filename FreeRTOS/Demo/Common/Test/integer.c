/*
 * FreeRTOS V202212.00
 * Copyright (C) 2020 Amazon.com, Inc. or its affiliates.  All Rights Reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy of
 * this software and associated documentation files (the "Software"), to deal in
 * the Software without restriction, including without limitation the rights to
 * use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of
 * the Software, and to permit persons to whom the Software is furnished to do so,
 * subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
 * FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
 * COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
 * IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 * https://www.FreeRTOS.org
 * https://github.com/FreeRTOS
 *
 */

/*
 * Creates one or more tasks that repeatedly perform a set of integer
 * calculations.  The result of each run-time calculation is compared to the
 * known expected result - with a mismatch being indicative of an error in the
 * context switch mechanism.
 */

#include <stdlib.h>
#include <stdio.h>

/* Scheduler include files. */
#include "FreeRTOS.h"
#include "task.h"

/* Demo program include files. */
#include "integer.h"

/* The constants used in the calculation. */
#define intgCONST1             ( ( long ) 123 )
#define intgCONST2             ( ( long ) 234567 )
#define intgCONST3             ( ( long ) -3 )
#define intgCONST4             ( ( long ) 7 )
#define intgEXPECTED_ANSWER    ( ( ( intgCONST1 + intgCONST2 ) * intgCONST3 ) / intgCONST4 )

#define intgSTACK_SIZE         configMINIMAL_STACK_SIZE
#define intgNUMBER_OF_TASKS    ( 1 )
static portTASK_FUNCTION_PROTO( vCompeteingIntMathTask, pvParameters );
static BaseType_t xTaskCheck[ intgNUMBER_OF_TASKS ] = { ( BaseType_t ) pdFALSE };

/*-----------------------------------------------------------*/

void main_test( UBaseType_t uxPriority )
{
    short sTask;
    if( uxPriority >= configMAX_PRIORITIES )
    {
        uxPriority = tskIDLE_PRIORITY + 1;
    }

    for( sTask = 0; sTask < intgNUMBER_OF_TASKS; sTask++ )
    {
        printf("[integer] Creating task %d with priority %u\n", sTask, (unsigned )uxPriority);
        xTaskCreate( vCompeteingIntMathTask, "IntMath", intgSTACK_SIZE, ( void * ) &( xTaskCheck[ sTask ] ), uxPriority, ( TaskHandle_t * ) NULL );
    }
    vTaskStartScheduler();
}
/*-----------------------------------------------------------*/

static portTASK_FUNCTION( vCompeteingIntMathTask, pvParameters )
{
/* These variables are all effectively set to constants so they are volatile to
 * ensure the compiler does not just get rid of them. */
    volatile long lValue;
    short sError = pdFALSE;
    volatile BaseType_t * pxTaskHasExecuted;

    /* Set a pointer to the variable we are going to set to true each
     * iteration.  This is also a good test of the parameter passing mechanism
     * within each port. */
    pxTaskHasExecuted = ( volatile BaseType_t * ) pvParameters;
    printf("[integer] Task started: expected result = %d\n", (int)intgEXPECTED_ANSWER);

    /* Keep performing a calculation and checking the result against a constant. */
    for( ; ; )
    {
        /* Perform the calculation.  This will store partial value in
         * registers, resulting in a good test of the context switch mechanism. */
        printf("[integer] Starting calculation...\n");
        lValue = intgCONST1;
        printf("[integer] Step1: lValue = %d\n", lValue);
        lValue += intgCONST2;
        printf("[integer] Step2: lValue += %d → %d\n", (int)intgCONST2, lValue);


        /* Yield in case cooperative scheduling is being used. */
        #if configUSE_PREEMPTION == 0
        {
            taskYIELD();
        }
        #endif

        /* Finish off the calculation. */
        lValue *= intgCONST3;
        printf("[integer] Step3: multiply by %d → %d\n", (long)intgCONST3, lValue);
        lValue /= intgCONST4;
        printf("[integer] Step4: divide by %d → %d\n", (long)intgCONST4, lValue);


        /* If the calculation is found to be incorrect we stop setting the
         * TaskHasExecuted variable so the check task can see an error has
         * occurred. */
        if( lValue != intgEXPECTED_ANSWER ) /*lint !e774 volatile used to prevent this being optimised out. */
        {
            printf("[integer] ERROR: got %d, expected %d\n", lValue, (long)intgEXPECTED_ANSWER);
            sError = pdTRUE;
        }
        else{
            printf("[integer] Correct result: %d\n", lValue);

        }

        if( sError == pdFALSE )
        {
            /* We have not encountered any errors, so set the flag that show
             * we are still executing.  This will be periodically cleared by
             * the check task. */
            portENTER_CRITICAL();
            *pxTaskHasExecuted = pdTRUE;
            portEXIT_CRITICAL();
        }
        printf("[integer] Loop done. Task still healthy = %d\n", (long)*pxTaskHasExecuted);


        /* Yield in case cooperative scheduling is being used. */
        #if configUSE_PREEMPTION == 0
        {
            taskYIELD();
        }
        #endif
    }
}
/*-----------------------------------------------------------*/

/* This is called to check that all the created tasks are still running. */
BaseType_t xAreIntegerMathsTaskStillRunning( void )
{
    BaseType_t xReturn = pdTRUE;
    short sTask;

    /* Check the maths tasks are still running by ensuring their check variables
     * are still being set to true. */
    for( sTask = 0; sTask < intgNUMBER_OF_TASKS; sTask++ )
    {
        if( xTaskCheck[ sTask ] == pdFALSE )
        {
            printf("[integer] Task %d check failed (no update)\n", sTask);
            /* The check has not incremented so an error exists. */
            xReturn = pdFALSE;
        }
        else
        {
            printf("[integer] Task %d check passed\n", sTask);
        }

        /* Reset the check variable so we can tell if it has been set by
         * the next time around. */
        xTaskCheck[ sTask ] = pdFALSE;
    }

    return xReturn;
}
