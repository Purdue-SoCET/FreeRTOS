#ifndef RISCV_VIRT_H_
#define RISCV_VIRT_H_

#include "riscv-reg.h"

#ifdef __ASSEMBLER__
#define CONS(NUM, TYPE) NUM
#else
#define CONS(NUM, TYPE) NUM##TYPE
#endif /* __ASSEMBLER__ */

/* 
If mainFPGA is 1 then it will build wil purpose of showing demo on FPGA, otherwise
the blinky demo is implemented

If mainCREATE_SIMPLE_BLINKY_DEMO_ONLY is 1 then the blinky demo will be built.
The blinky demo is implemented and described in main_blinky.c, which is a
simple blinky style demo application,.

NOTE: has not tested mainCREATE_SIMPLE_BLINKY_DEMO_ONLY = 0
If mainCREATE_SIMPLE_BLINKY_DEMO_ONLY is not 1 then the comprehensive test and
demo application will be built.  The comprehensive test and demo application is
implemented and described in main_full.c.
*/

#define SYNTHESIS 1 // SYNTHESIS macro, 1 for building fpga sysnthesis, 0 for simulation 
#define mainFPGA 1
#define mainCREATE_SIMPLE_BLINKY_DEMO_ONLY	1

#define CLINT_ADDR			CONS(0x90000000, UL)
#define CLINT_MSIP			CONS(0x0, UL)
#define CLINT_MTIME			CONS(0x4, UL)
#define CLINT_MTIMECMP	    CONS(0xC, UL)

#define PRIM_HART			0

#ifndef __ASSEMBLER__

int xGetCoreID( void );
void vSendString( const char * s );

#endif /* __ASSEMBLER__ */

#endif /* RISCV_VIRT_H_ */
