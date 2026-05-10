#include <stdint.h>
#include <stdbool.h>

#include "tm4c123gh6pm.h"
#include "timers.h"
#include "app.h"

#define SYSTEM_CLOCK_HZ     16000000UL

#ifndef TIMER1_ADCEV_R
#define TIMER1_ADCEV_R      (*((volatile unsigned long *)0x40031070))
#endif

static void SysTick_Init_10ms(void);
static void Timer0A_Stopwatch_Init_1Hz(void);
static void Timer1A_ADCTrigger_Init_1Hz(void);
static void Timer0A_ISR_Service(void);

void Timers_Init(void)
{
    SysTick_Init_10ms();
    Timer0A_Stopwatch_Init_1Hz();
    Timer1A_ADCTrigger_Init_1Hz();
}

static void SysTick_Init_10ms(void)
{
    NVIC_ST_CTRL_R = 0UL;
    NVIC_ST_RELOAD_R = 160000UL - 1UL;
    NVIC_ST_CURRENT_R = 0UL;
    NVIC_ST_CTRL_R = 0x07UL;
}

static void Timer0A_Stopwatch_Init_1Hz(void)
{
    SYSCTL_RCGCTIMER_R |= (1UL << 0);

    while (((SYSCTL_PRTIMER_R >> 0) & 1UL) == 0UL)
    {
    }

    TIMER0_CTL_R &= ~(1UL << 0);
    TIMER0_CFG_R = 0UL;
    TIMER0_TAMR_R = 0x02UL;
    TIMER0_TAILR_R = SYSTEM_CLOCK_HZ - 1UL;
    TIMER0_TAPR_R = 0UL;
    TIMER0_ICR_R = 0x01UL;
    TIMER0_IMR_R |= 0x01UL;

    NVIC_EN0_R |= (1UL << 19);

    TIMER0_CTL_R |= (1UL << 0);
}

static void Timer1A_ADCTrigger_Init_1Hz(void)
{
    SYSCTL_RCGCTIMER_R |= (1UL << 1);

    while (((SYSCTL_PRTIMER_R >> 1) & 1UL) == 0UL)
    {
    }

    TIMER1_CTL_R &= ~(1UL << 0);
    TIMER1_CFG_R = 0UL;
    TIMER1_TAMR_R = 0x02UL;
    TIMER1_TAILR_R = SYSTEM_CLOCK_HZ - 1UL;
    TIMER1_TAPR_R = 0UL;
    TIMER1_ICR_R = 0x01UL;

    TIMER1_ADCEV_R = 0x01UL;
    TIMER1_CTL_R |= (1UL << 5);
    TIMER1_CTL_R |= (1UL << 0);
}

void SysTick_Handler(void)
{
    APP_10msTick();
}

static void Timer0A_ISR_Service(void)
{
    TIMER0_ICR_R = 0x01UL;
    APP_StopwatchOneSecondTick();
}

void Timer0A_Handler(void)
{
    Timer0A_ISR_Service();
}

void TIMER0A_Handler(void)
{
    Timer0A_ISR_Service();
}

void Timer0A_IRQHandler(void)
{
    Timer0A_ISR_Service();
}

void TIMER0A_IRQHandler(void)
{
    Timer0A_ISR_Service();
}