/* FreeRTOS includes. */
#include "FreeRTOS.h"
#include "task.h"
#include "riscv-virt.h"
#include "arch/cpuid.h"
#include "arch/spinlock.h"

/* Standard includes. */
#include <stdio.h>
#include <string.h>

#define mainVECTOR_MODE_DIRECT	0

#define mainPLIC_PENDING_0 ( * ( ( volatile uint32_t * ) 0x0C001000UL ) )
#define mainPLIC_PENDING_1 ( * ( ( volatile uint32_t * ) 0x0C001004UL ) )
#define mainPLIC_ENABLE_0  ( * ( ( volatile uint32_t * ) 0x0C002000UL ) )
#define mainPLIC_ENABLE_1  ( * ( ( volatile uint32_t * ) 0x0C002004UL ) )

extern void freertos_risc_v_trap_handler( void );
extern void freertos_vector_table( void );

/*
 * main_blinky() is used when mainCREATE_SIMPLE_BLINKY_DEMO_ONLY is set to 1.
 * main_full() is used when mainCREATE_SIMPLE_BLINKY_DEMO_ONLY is set to 0.
 * main_fpga() is AFTx07 specific to test on Socet FPGA
 */
extern void main_blinky( void );
extern void main_smp( void );
extern void main_full( void );
extern void main_fpga( void );
extern void uart_sendbyte(char c);

void vFullDemoTickHookFunction( void );
void vFullDemoIdleFunction( void );

/*-----------------------------------------------------------*/
void main_second(void)
{
	printf("You should not see this line!");
}

void main( void )
{
	#if( mainVECTOR_MODE_DIRECT == 1 )
	{
		__asm__ volatile( "csrw mtvec, %0" :: "r"( freertos_risc_v_trap_handler ) );
	}
	#else
	{
		__asm__ volatile( "csrw mtvec, %0" :: "r"( ( uintptr_t )freertos_vector_table | 0x1 ) );
	}
	#endif
	//board_init_uart();
	printf("Get into main\n");


	#if ( mainFPGA == 1)
	{
		main_fpga();
	}
	#elif ( mainCREATE_SMP_DEMO == 1 )
	{
		main_smp();
	}
	#elif ( mainCREATE_SIMPLE_BLINKY_DEMO_ONLY == 1 )
	{
		main_blinky();
	}
	#else //mainCREATE_SIMPLE_BLINKY_DEMO_ONLY == 1
	{
		// main_test();
	}
	#endif // ( mainFPGA == 1)
}
/*-----------------------------------------------------------*/

/* vApplicationMallocFailedHook() will only be called if
	configUSE_MALLOC_FAILED_HOOK is set to 1 in FreeRTOSConfig.h.  It is a hook
	function that will get called if a call to pvPortMalloc() fails.
	pvPortMalloc() is called internally by the kernel whenever a task, queue,
	timer or semaphore is created using the dynamic allocation (as opposed to
	static allocation) option.  It is also called by various parts of the
	demo application.  If heap_1.c, heap_2.c or heap_4.c is being used, then the
	size of the	heap available to pvPortMalloc() is defined by
	configTOTAL_HEAP_SIZE in FreeRTOSConfig.h, and the xPortGetFreeHeapSize()
	API function can be used to query the size of free heap space that remains
	(although it does not provide information on how the remaining heap might be
	fragmented).  See http://www.freertos.org/a00111.html for more
	information. */
void vApplicationMallocFailedHook( void )
{
	printf( "\r\n\r\nMalloc failed\r\n" );
	portDISABLE_INTERRUPTS();
	for( ;; );
}
/*-----------------------------------------------------------*/

/* vApplicationIdleHook() will only be called if configUSE_IDLE_HOOK is set
	to 1 in FreeRTOSConfig.h. It will be called on each iteration of the idle
	task. It is essential that code added to this hook function never attempts
	to block in any way (for example, call xQueueReceive() with a block time
	specified, or call vTaskDelay()).  If application tasks make use of the
	vTaskDelete() API function to delete themselves then it is also important
	that vApplicationIdleHook() is permitted to return to its calling function,
	because it is the responsibility of the idle task to clean up memory
	allocated by the kernel to any task that has since deleted itself. */
void vApplicationIdleHook( void ){}
/*-----------------------------------------------------------*/

void vApplicationPassiveIdleHook( void ){
	/* The passive idle loop has nothing else to do when no task is ready.
	 * gcc turns that empty loop into a jump-to-self, and this core halts on
	 * that instruction, which stops the hart that owns the tick. The call
	 * and the nop keep the loop as real instructions so a timer interrupt
	 * can still preempt it. */
	__asm volatile ( "nop" ); 
}
/*-----------------------------------------------------------*/

/* Run time stack overflow checking is performed if
	configCHECK_FOR_STACK_OVERFLOW is defined to 1 or 2.  This hook
	function is called if a stack overflow is detected. */
void vApplicationStackOverflowHook( TaskHandle_t pxTask, char *pcTaskName )
{
	( void ) pcTaskName;
	( void ) pxTask;

	printf( "\r\n\r\nStack overflow in %s\r\n", pcTaskName );
	portDISABLE_INTERRUPTS();
	for( ;; );
}
/*-----------------------------------------------------------*/

/* This function will be called by each tick interrupt if
	configUSE_TICK_HOOK is set to 1 in FreeRTOSConfig.h.  User code can be
	added here, but the tick hook is called from an interrupt context, so
	code must not attempt to block, and only the interrupt safe FreeRTOS API
	functions can be used (those that end in FromISR()). */
void vApplicationTickHook( void )
{
	/*
	#if ( mainCREATE_SIMPLE_BLINKY_DEMO_ONLY != 1 )
	{
		extern void vFullDemoTickHookFunction( void );

		vFullDemoTickHookFunction();
	}
	#endif /* mainCREATE_SIMPLE_BLINKY_DEMO_ONLY */
	// AlarmKickFromTickISR();
}
/*-----------------------------------------------------------*/

/* This function will be called once only, when the daemon task starts to
	execute (sometimes called the timer task).  This is useful if the
	application includes initialisation code that would benefit from executing
	after the scheduler has been started. */
void vApplicationDaemonTaskStartupHook( void ){}
/*-----------------------------------------------------------*/

/* Called if an assertion passed to configASSERT() fails.  See
	http://www.freertos.org/a00110.html#configASSERT for more information. */
void vAssertCalled( const char *pcFileName, uint32_t ulLine )
{
	volatile uint32_t ulSetToNonZeroInDebuggerToContinue = 0;

	printf( "ASSERT! Line %d, file %s\r\n", ( int ) ulLine, pcFileName );

 	taskENTER_CRITICAL();
	{
		while( ulSetToNonZeroInDebuggerToContinue == 0 )
		{
			__asm volatile( "NOP" );
			__asm volatile( "NOP" );
		}
	}
	taskEXIT_CRITICAL();
}
/*-----------------------------------------------------------*/

void vApplicationGetIdleTaskMemory( StaticTask_t **ppxIdleTaskTCBBuffer, StackType_t **ppxIdleTaskStackBuffer, uint32_t *pulIdleTaskStackSize )
{
	static StaticTask_t xIdleTaskTCB;
	static StackType_t uxIdleTaskStack[ configMINIMAL_STACK_SIZE ];

	*ppxIdleTaskTCBBuffer = &xIdleTaskTCB;

	*ppxIdleTaskStackBuffer = uxIdleTaskStack;

	*pulIdleTaskStackSize = configMINIMAL_STACK_SIZE;
}

void vApplicationGetPassiveIdleTaskMemory( StaticTask_t ** ppxIdleTaskTCBBuffer,
                                           StackType_t ** ppxIdleTaskStackBuffer,
                                           configSTACK_DEPTH_TYPE * puxIdleTaskStackSize,
                                           BaseType_t xPassiveIdleTaskIndex )
{
	static StaticTask_t xPassiveIdleTCB;
	static StackType_t uxPassiveIdleStack[ configMINIMAL_STACK_SIZE ];

	( void ) xPassiveIdleTaskIndex;

	*ppxIdleTaskTCBBuffer = &xPassiveIdleTCB;
	*ppxIdleTaskStackBuffer = uxPassiveIdleStack;
	*puxIdleTaskStackSize = configMINIMAL_STACK_SIZE;
}
/*-----------------------------------------------------------*/

void vApplicationGetTimerTaskMemory( StaticTask_t **ppxTimerTaskTCBBuffer, StackType_t **ppxTimerTaskStackBuffer, uint32_t *pulTimerTaskStackSize )
{
	static StaticTask_t xTimerTaskTCB;
	static StackType_t uxTimerTaskStack[ configTIMER_TASK_STACK_DEPTH ];

	*ppxTimerTaskTCBBuffer = &xTimerTaskTCB;

	*ppxTimerTaskStackBuffer = uxTimerTaskStack;

	*pulTimerTaskStackSize = configTIMER_TASK_STACK_DEPTH;
}
/*-----------------------------------------------------------*/

static struct arch_spinlock_t xUartLock = { .cnt = 0, .owner = -1 };

int __write(int iFile, char *pcString, int iStringLength)
{
    int xHart = arch_cpuid();

    (void)iFile;
    arch_spinlock_lock( xHart, &xUartLock );
    for (int i = 0; i < iStringLength; i++) {
        char c = pcString[i];
        if (c == '\n') uart_sendbyte('\r');
        uart_sendbyte(c);
    }
    arch_spinlock_unlock( xHart, &xUartLock );
    return iStringLength;
}
/*-----------------------------------------------------------*/

/* This project uses heap_4 so doesn't set up a heap for use by the C
	library - but something is calling the C library malloc().  See
	https://freertos.org/a00111.html for more information. */
void *malloc( size_t size )
{
	( void ) size;

	printf( "\r\n\r\nUnexpected call to malloc() - should be using pvPortMalloc()\r\n" );
	portDISABLE_INTERRUPTS();
	for( ;; );

}

void freertos_risc_v_application_exception_handler ( void ) 
{
	printf("application_exception_handler\n");
	uint32_t exc_data[3];
	asm volatile(
			"csrr %0, mepc\n"
			"csrr %1, mcause\n"
			"csrr %2, mtval\n"
			: "=r"(exc_data[0]), "=r"(exc_data[1]), "=r"(exc_data[2])
		);
	printf("mepc: 0x%x, mcause: 0x%x, mtval: 0x%x\n", exc_data[0], exc_data[1], exc_data[2]);
}
