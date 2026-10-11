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

void vApplicationMallocFailedHook( void )
{
	printf( "\r\n\r\nMalloc failed\r\n" );
	portDISABLE_INTERRUPTS();
	for( ;; );
}
/*-----------------------------------------------------------*/

void vApplicationIdleHook( void ){}
/*-----------------------------------------------------------*/

void vApplicationPassiveIdleHook( void ){__asm volatile ( "nop" ); }
/*-----------------------------------------------------------*/

void vApplicationStackOverflowHook( TaskHandle_t pxTask, char *pcTaskName )
{
	( void ) pcTaskName;
	( void ) pxTask;

	printf( "\r\n\r\nStack overflow in %s\r\n", pcTaskName );
	portDISABLE_INTERRUPTS();
	for( ;; );
}
/*-----------------------------------------------------------*/

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

void vApplicationDaemonTaskStartupHook( void ){}
/*-----------------------------------------------------------*/

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
