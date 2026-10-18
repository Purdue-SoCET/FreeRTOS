#ifndef RISCV_VIRT_H_
#define RISCV_VIRT_H_

#include "riscv-reg.h"

#ifdef __ASSEMBLER__
#define CONS(NUM, TYPE) NUM
#else
#define CONS(NUM, TYPE) NUM##TYPE
#endif /* __ASSEMBLER__ */


#define SYNTHESIS 0 // SYNTHESIS macro, 1 for building fpga sysnthesis, 0 for simulation 
#define mainFPGA 0
#define mainCREATE_AFFINITY_DEMO	1
#define mainCREATE_SMP_DEMO	0
#define mainCREATE_SIMPLE_BLINKY_DEMO_ONLY	0

#define CLINT_ADDR			CONS(0x90010000, UL) // address for clint
#define CLINT_MSIP			CONS(0x0, UL)
#define CLINT_MTIME			CONS(0xBFF8, UL)
#define CLINT_MTIMECMP	    CONS(0x4000, UL)

#define PRIM_HART			0

#ifndef __ASSEMBLER__

int xGetCoreID( void );
void vSendString( const char * s );

#endif /* __ASSEMBLER__ */

#endif /* RISCV_VIRT_H_ */
