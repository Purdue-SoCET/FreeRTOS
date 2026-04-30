#include "pal.h"
#include "uart.h"

#define BAUD_CYCLES 2604

static UARTRegBlk *uart = (UARTRegBlk *) UART_BASE;


void uart_setup( void )
{
    
}

/* Magic print: write to simulator address to print a char to teminal */
void uart_sendbyte( char onechar )
{
    volatile char *MAGIC_ADDR = (volatile char *)0xB0000000;
    *MAGIC_ADDR = onechar;
}


/*
void uart_setup() 
{
    uart->rxstate = (BAUD_CYCLES / 16) << 16;
    uart->txstate = BAUD_CYCLES << 16;
}

void uart_sendbyte(char onechar)
{
    uart->txdata = (uint32_t) onechar | 1<<24;
    while (!(uart->txstate & 0x1));
}
*/