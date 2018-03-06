void serial_init(void)
{
}

/* XXX correct for BCM6358 */
#define UART_BASE 0xfffe0100

void serial_putc(char c)
{
    while (!(*(volatile unsigned int *)(UART_BASE + 0x10) & (1 << 5)));
    *(volatile unsigned int *)(UART_BASE + 0x14) = c;
}
