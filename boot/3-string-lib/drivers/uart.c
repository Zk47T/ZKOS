#include "uart.h"

#define LPUART1_BASE 0x44380000
#define STAT_OFFSET 0x14
#define DATA_OFFSET 0x1C
#define TDRE_BIT (1<<23)
#define RDRF_BIT (1<<21)

volatile int *stat = (volatile int*) (LPUART1_BASE + STAT_OFFSET);
volatile int *data = (volatile int*) (LPUART1_BASE + DATA_OFFSET);

void uart_putc(char c)
{
    while( (*stat & TDRE_BIT) == 0);
    *data = c;
}

void uart_puts(const char* s)
{
    while(*s)
    {
        uart_putc(*s++);
    }
}

char uart_getc()
{
    while((*stat & RDRF_BIT) ==0 );
    return *data;
}

void uart_puthex(unsigned long val)
{
    const char hex_nums[] = "0123456789ABCDEF";
    uart_puts("0x");

    for(int shift_amount = 60; shift_amount >= 0; shift_amount -= 4)
    {
        unsigned long shift_val = val >> shift_amount;

        unsigned int tmp = shift_val & 0xF;

        char hex_c = hex_nums[tmp];
        
        uart_putc(hex_c);
    }
}


