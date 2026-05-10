#include <stdint.h>
#include <stdbool.h>

#include "tm4c123gh6pm.h"
#include "uart.h"

#define UART0_RX_BUFFER_SIZE 128U

static volatile char g_uart0RxBuffer[UART0_RX_BUFFER_SIZE];
static volatile uint32_t g_uart0RxHead = 0U;
static volatile uint32_t g_uart0RxTail = 0U;

static void UART0_RxPush(char c);
static void UART0_ISR_Service(void);

void UART0_Init(void)
{
    SYSCTL_RCGCUART_R |= (1UL << 0);
    SYSCTL_RCGCGPIO_R |= (1UL << 0);

    while (((SYSCTL_PRUART_R >> 0) & 1UL) == 0UL)
    {
    }

    while (((SYSCTL_PRGPIO_R >> 0) & 1UL) == 0UL)
    {
    }

    UART0_CTL_R &= ~(1UL << 0);

    GPIO_PORTA_AFSEL_R |= 0x03UL;
    GPIO_PORTA_PCTL_R &= ~0x000000FFUL;
    GPIO_PORTA_PCTL_R |= 0x00000011UL;
    GPIO_PORTA_DEN_R |= 0x03UL;
    GPIO_PORTA_AMSEL_R &= ~0x03UL;

    /*
        UART0 clock source = PIOSC 16 MHz.
        PuTTY baud rate = 9600.
    */
    UART0_CC_R = 0x05UL;

    UART0_IBRD_R = 104UL;
    UART0_FBRD_R = 11UL;

    /*
        8-bit, no parity, 1 stop bit, FIFO disabled.
    */
    UART0_LCRH_R = 0x60UL;

    UART0_ICR_R = 0x7FFUL;
    UART0_IM_R |= (1UL << 4);

    /*
        UART0 IRQ = 5.
    */
    NVIC_EN0_R |= (1UL << 5);

    UART0_CTL_R |= (1UL << 0) | (1UL << 8) | (1UL << 9);
}

void UART0_SendChar(char c)
{
    while ((UART0_FR_R & (1UL << 5)) != 0UL)
    {
    }

    UART0_DR_R = (uint32_t)c;
}

void UART0_SendString(const char *str)
{
    while (*str != '\0')
    {
        UART0_SendChar(*str);
        str++;
    }
}

void UART0_SendCRLF(void)
{
    UART0_SendString("\r\n");
}

void UART0_SendUnsigned(uint32_t n)
{
    char buffer[11];
    int32_t i = 0;

    if (n == 0U)
    {
        UART0_SendChar('0');
        return;
    }

    while ((n > 0U) && (i < 10))
    {
        buffer[i] = (char)('0' + (char)(n % 10U));
        n /= 10U;
        i++;
    }

    while (i > 0)
    {
        i--;
        UART0_SendChar(buffer[i]);
    }
}

void UART0_SendSigned(int32_t n)
{
    if (n < 0)
    {
        UART0_SendChar('-');

        if (n == (-2147483647 - 1))
        {
            UART0_SendUnsigned(2147483648UL);
            return;
        }

        n = -n;
    }

    UART0_SendUnsigned((uint32_t)n);
}

bool UART0_RxAvailable(void)
{
    return (g_uart0RxHead != g_uart0RxTail);
}

char UART0_RxPop(void)
{
    char c = '\0';

    if (g_uart0RxHead != g_uart0RxTail)
    {
        c = g_uart0RxBuffer[g_uart0RxTail];
        g_uart0RxTail = (g_uart0RxTail + 1U) % UART0_RX_BUFFER_SIZE;
    }

    return c;
}

static void UART0_RxPush(char c)
{
    uint32_t nextHead;

    nextHead = (g_uart0RxHead + 1U) % UART0_RX_BUFFER_SIZE;

    if (nextHead != g_uart0RxTail)
    {
        g_uart0RxBuffer[g_uart0RxHead] = c;
        g_uart0RxHead = nextHead;
    }
}

static void UART0_ISR_Service(void)
{
    char c;

    if ((UART0_MIS_R & (1UL << 4)) != 0UL)
    {
        while ((UART0_FR_R & (1UL << 4)) == 0UL)
        {
            c = (char)(UART0_DR_R & 0xFFUL);
            UART0_RxPush(c);
        }

        UART0_ICR_R = (1UL << 4);
    }
}

void UART0_Handler(void)
{
    UART0_ISR_Service();
}

void UART0_IRQHandler(void)
{
    UART0_ISR_Service();
}