/*
 * main_fpga is specifically for demo on AFTx07 chip
 * For now it will blink 2 leds at different rates to 
 * demonstate multictaking is working
 */

/******************************************************************************
 * main_fpga() creates two tasks and set up one timer that expires every 2s. It 
 * then starts the scheduler.
 *
 * The blink_fast tasks:
 * Blinks the first LED every 200ms. This task will use sleep directly
 * inside the task. This means that if in the future we have interrupt, it
 * will be interrupted.
 * 
 * The blink_slow tasks: 
 * Blinks the second LED every 2000ms = 2s. This task will be called 
 * as a callback function when the software timer expires.
 * 
 * Every task after set the gpio value will read back and print out
 * the value, the print only works for simulation to debug only,
 * it will not show anything on the fpga
 *
 */

/* Standard includes. */
#include <stdio.h>

/* Kernel includes. */
#include "FreeRTOS.h"
#include "task.h"
#include "timers.h"

/* AFTx07 include*/
#include "pal.h"
#include "uart.h"

/* Priorities at which the tasks are created. */
#define mainBLINK_FAST_TASK_PRIOTIRY		( tskIDLE_PRIORITY + 2 )
#define	mainBLINK_SLOW_TASK_PRIORITY		( tskIDLE_PRIORITY + 1 )

/* The rate at which data is sent to the queue.  The times are converted from
milliseconds to ticks using the pdMS_TO_TICKS() macro. */
#define mainTASK_BLINK_FAST_FREQUENCY_MS			pdMS_TO_TICKS( 20UL )
#define mainTIMER_BLINK_SLOW_FREQUENCY_MS			pdMS_TO_TICKS( 200UL )

/*-----------------------------------------------------------*/

/*
 * The tasks as described in the comments at the top of this file.
 */
static void prvBlinkFastTask( void *pvParameters );

/*
 * The callback function executed when the software timer expires.
 */
static void prvBlinkSlowCallback( TimerHandle_t xTimerHandle );

/* A software timer that is started from the tick hook. */
static TimerHandle_t xTimer = NULL;

/* A GPIO object to blink an LED */
static GPIORegBlk *GPIO = (GPIORegBlk*) GPIO_BASE;

/*-----------------------------------------------------------*/

/*** SEE THE COMMENTS AT THE TOP OF THIS FILE ***/
void main_fpga( void )
{
const TickType_t xTimerPeriod = mainTIMER_BLINK_SLOW_FREQUENCY_MS;

	printf("Get into main_fpga\n");
	
	// Setup UART
	uart_setup();

	// GPIO set-up
	GPIO->ddr = 0xFFFFFFFF; // Set all GPIO output
	GPIO->data = 0;
	printf("GPIO->ddr = %x\n",GPIO->ddr);
	
    /* Start the two tasks as described in the comments at the top of this
    file. */
    xTaskCreate( prvBlinkFastTask,			/* The function that implements the task. */
                "Blink_fast", 					/* The text name assigned to the task - for debug only as it is not used by the kernel. */
                configMINIMAL_STACK_SIZE, 		/* The size of the stack to allocate to the task. */
                NULL, 							/* The parameter passed to the task - not used in this simple case. */
                mainBLINK_FAST_TASK_PRIOTIRY,/* The priority assigned to the task. */
                NULL );							/* The task handle is not required, so NULL is passed. */
    printf("Done created blink fast\n");

    /* Create the software timer, but don't start it yet. */
    xTimer = xTimerCreate( "Timer",				/* The text name assigned to the software timer - for debug only as it is not used by the kernel. */
                            xTimerPeriod,		/* The period of the software timer in ticks. */
                            pdTRUE,				/* xAutoReload is set to pdTRUE, so this is an auto-reload timer. */
                            NULL,				/* The timer's ID is not used. */
                            prvBlinkSlowCallback );/* The function executed when the timer expires. */
    printf("Done created timer for blink slow task\n");

    xTimerStart( xTimer, 0 ); /* The scheduler has not started so use a block time of 0. */

    /* Start the tasks and timer running. */
    vTaskStartScheduler();

	/* If all is well, the scheduler will now be running, and the following
	line will never be reached.  If the following line does execute, then
	there was insufficient FreeRTOS heap memory available for the idle and/or
	timer tasks	to be created.  See the memory management section on the
	FreeRTOS web site for more details.  NOTE: This demo uses static allocation
	for the idle and timer tasks so this line should never execute. */
	printf("This line should never be seen\nMaybe increase heap size in FreeRTOSConfig.h");
	for( ;; );
}
/*-----------------------------------------------------------*/

static void prvBlinkFastTask( void *pvParameters )
{
TickType_t xNextWakeTime;
    const TickType_t xBlockTime = mainTASK_BLINK_FAST_FREQUENCY_MS;

	/* Prevent the compiler warning about the unused parameter. */
	( void ) pvParameters;

	/* Initialise xNextWakeTime - this only needs to be done once. */
	xNextWakeTime = xTaskGetTickCount();

	for( ;; )
	{
		// First set GPIO1 to 1, i.e, turn on LED1
		printf("About to turn on LED1: GPIO->data: %x\n", GPIO->data);
		GPIO->data |= 1;
		printf("Turning on LED1, GPIO: %x\n",GPIO->data);
		/* Place this task in the blocked state until it is time to run again.
		The block time is specified in ticks, pdMS_TO_TICKS() was used to
		convert a time specified in milliseconds into a time specified in ticks.
		While in the Blocked state this task will not consume any CPU time. */
		vTaskDelayUntil( &xNextWakeTime, xBlockTime );
		// After delay, turn off LED1
		GPIO->data &= ~1;

		printf("read back value: %x - Should have bit 1 clered\n", GPIO->data);
	}
}
/*-----------------------------------------------------------*/

static void prvBlinkSlowCallback( TimerHandle_t xTimerHandle )
{
	/* Software timer callback function will flip bit #2 of 
	GPIO everytime it being called */

	/* Avoid compiler warnings resulting from the unused parameter. */
	( void ) xTimerHandle;
	uint32_t bit_flip = 2;
	printf("About to flip bit, GPIO->data %x\n", GPIO->data);
	GPIO->data ^= bit_flip;
	printf("Timer expires, bit flip %x\n", GPIO->data);
}
/*-----------------------------------------------------------*/