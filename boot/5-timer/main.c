#include "uart.h"
#include "string.h"
#include "wdog.h"
#include "timer.h"

void exec_cmd(char* cmd)
{
    if(strcmp(cmd, "help") == 0)
    {
        uart_puts("Available command : \r\n");
        uart_puts(" help   - Show this help message\r\n");
        uart_puts(" info   - System information\r\n");
        uart_puts(" hello  - Show greeting message\r\n");
        uart_puts(" uptime - Show system uptime\r\n");
    }
    else if(strcmp(cmd, "info") == 0)
    {
        uart_puts("ZKOS - Author: Nguyen Minh Tien - Date 21/04/2026\r\n");
    }
    else if(strcmp(cmd, "hello") == 0)
    {
        uart_puts("Welcome to ZKOS\r\n");
    }
    // else if(strcmp(cmd, "uptime") == 0)
    // {
    //     unsigned long uptime_s = get_timer_value() / get_timer_frequency();
    //     uart_puts("Uptime: ");
    //     uart_putint(uptime_s);
    //     uart_puts(" seconds\r\n");
    // }
    else if(strlen(cmd) > 0)
    {
        uart_puts("Unknows cmd: ");
        uart_puts(cmd);
        uart_puts("\r\n");
    }
}

void main(void)
{
    // Disable WDOG3 to prevent periodic board reset
    wdog3_disable();

    char buf[64];
    int i = 0;

    uart_puts("Welcome to ZKOS\r\n");
    uart_puts("Type 'help' to see available command\r\n");

    uart_puts("ZKOS> ");

    while(1)
    {
        char c = uart_getc();
        if(c == '\r')
        {
            buf[i]='\0';
            uart_puts("\r\n");
            exec_cmd(buf);
            uart_puts("ZKOS> ");
            i = 0;
        }
        else if(c == '\b' | c == 0x7F) //Backspace or delete
        {
            if(i>0)
            {
                i --;
                uart_puts("\b \b");
            }
        }
        else 
        {
            if(i < 63)
            {
                uart_putc(c);
                buf[i++] = c;
            }
        }
    }

}