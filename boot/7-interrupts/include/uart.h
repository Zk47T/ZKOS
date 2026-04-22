#ifndef UART_H
#define UART_H

void uart_putc(char c);
void uart_puts(const char* s);
char uart_getc();
void uart_puthex(unsigned long val);
void uart_putint(unsigned long val);

#endif
