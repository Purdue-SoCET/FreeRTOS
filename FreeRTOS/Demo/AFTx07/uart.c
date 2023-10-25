#include "pal.h"
#include "uart.h"

#define BAUD_CYCLES 2604 //BAUD_RATE = 96000

static UARTRegBlk *uart = (UARTRegBlk *) UART_BASE;

/* Set up UART BAUD_RATE */
void uart_setup() 
{
    uart->rxstate = (BAUD_CYCLES / 16) << 16;
    uart->txstate = BAUD_CYCLES << 16;
}

/* Send one byte to UART TX */
void uart_sendbyte(char onechar)
{
    uart->txdata = (uint32_t) onechar | 1<<24;
    while (!(uart->txstate & 0x1));
}
