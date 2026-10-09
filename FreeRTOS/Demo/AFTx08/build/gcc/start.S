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

#include "riscv-virt.h"

	.extern main
	.section .init
	.globl _start
	.type _start,@function
_start:
	.cfi_startproc
	.cfi_undefined ra
.option push
.option norelax
	la  gp, __global_pointer$
.option pop

	// Continue primary hart
	csrr a0, mhartid
	li   a1, PRIM_HART
	bne  a0, a1, secondary

	// Primary hart
	la sp, _stack_top

	// Load data section
	la a0, _data_lma
	la a1, _data
	la a2, _edata
	bgeu a1, a2, 2f
1:
	LOAD t0, (a0)
	STOR t0, (a1)
	addi a0, a0, REGSIZE
	addi a1, a1, REGSIZE
	bltu a1, a2, 1b
2:

	// Clear bss section
	la a0, _bss
	la a1, _ebss
	bgeu a0, a1, 2f
1:
	STOR zero, (a0)
	addi a0, a0, REGSIZE
	bltu a0, a1, 1b
2:

	// Hart 1 is spinning on this word. It must not proceed until bss is cleared.
	la t0, boot_done
	li t1, 0xC0FFEE
	STOR t1, 0(t0)
	fence rw, rw

	// argc, argv, envp is 0
	li  a0, 0
	li  a1, 0
	li  a2, 0
	jal main
1:
	wfi
	j 1b

	/* Hart 1 waits here until hart 0 sends msip. That interrupt starts the scheduler. */
secondary:
	li t0, CLINT_ADDR + CLINT_MTIMECMP + 8
	li t1, -1
	STOR t1, 0(t0)
	STOR t1, 4(t0)

	la t0, boot_done
	li t2, 0xC0FFEE
wait_boot_done:
	LOAD t1, 0(t0)
	bne t1, t2, wait_boot_done
	fence rw, rw

	la t0, hart1_boot_stack
	addi sp, t0, 1024

	la t0, freertos_vector_table
	ori t0, t0, 1
	csrw mtvec, t0
	csrw mie, zero
	li t0, 8
	csrs mie, t0
	csrs mstatus, t0

wait_for_scheduler_ipi:
	wfi
	j wait_for_scheduler_ipi
	.cfi_endproc

	.section .data
	.align 2
boot_done:
	.word 0

	.section .bss
	.align 4
hart1_boot_stack:
	.skip 1024
