#include <stdint.h>
#include <stdbool.h>

#include "tm4c123gh6pm.h"
#include "app.h"
#include "gpio.h"
#include "uart.h"
#include "timers.h"
#include "adc.h"

static void DisableInterrupts(void);
static void EnableInterrupts(void);

static void DisableInterrupts(void)
{
    __asm(" CPSID I");
}

static void EnableInterrupts(void)
{
    __asm(" CPSIE I");
}

int main(void)
{
    DisableInterrupts();

    GPIO_Init();
    UART0_Init();
    APP_Init();
    ADC0_Init();
    Timers_Init();

    EnableInterrupts();

    UART0_SendString("\r\nUART TEST OK\r\n");
    APP_PrintWelcome();
    APP_PrintCurrentModeMenu();

    while (1)
    {
        APP_Run();
    }
}