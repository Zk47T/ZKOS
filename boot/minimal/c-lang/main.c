#define LPUART1_BASE   0x44380000
#define STAT_OFFSET    0x14
#define DATA_OFFSET    0x1C
#define TDRE_BIT       (1 << 23)

void main(void)
{
    volatile int *lpuart1 = (volatile int *)LPUART1_BASE;
    volatile int *STAT    = (volatile int *)(LPUART1_BASE + STAT_OFFSET);
    volatile int *DATA    = (volatile int *)(LPUART1_BASE + DATA_OFFSET);

    while (!(*STAT & TDRE_BIT));
    *DATA = 'Z';

    while (1);
}
