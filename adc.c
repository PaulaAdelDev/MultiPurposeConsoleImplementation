#include <stdint.h>
#include <stdbool.h>

#include "tm4c123gh6pm.h"
#include "adc.h"
#include "app.h"

static void ADC0SS3_Service(void);

void ADC0_Init(void)
{
    SYSCTL_RCGCADC_R |= (1UL << 0);
    SYSCTL_RCGCGPIO_R |= (1UL << 4);

    while (((SYSCTL_PRADC_R >> 0) & 1UL) == 0UL)
    {
    }

    while (((SYSCTL_PRGPIO_R >> 4) & 1UL) == 0UL)
    {
    }

    /*
        PE3 = AIN0.
    */
    GPIO_PORTE_AFSEL_R |= (1UL << 3);
    GPIO_PORTE_DEN_R &= ~(1UL << 3);
    GPIO_PORTE_AMSEL_R |= (1UL << 3);

    ADC0_ACTSS_R &= ~(1UL << 3);

    /*
        SS3 trigger source = timer trigger.
        ADCEMUX bits 15:12 = 0x5.
    */
    ADC0_EMUX_R &= ~(0xFUL << 12);
    ADC0_EMUX_R |= (0x5UL << 12);

    ADC0_SSMUX3_R = 0UL;
    ADC0_SSCTL3_R = 0x06UL;

    ADC0_ISC_R |= (1UL << 3);
    ADC0_IM_R |= (1UL << 3);

    /*
        ADC0 SS3 IRQ = 17.
    */
    NVIC_EN0_R |= (1UL << 17);

    ADC0_ACTSS_R |= (1UL << 3);
}

static void ADC0SS3_Service(void)
{
    uint32_t raw;

    raw = ADC0_SSFIFO3_R & 0x0FFFUL;
    ADC0_ISC_R |= (1UL << 3);

    APP_ADCNewReading(raw);
}

void ADC0SS3_Handler(void)
{
    ADC0SS3_Service();
}

void ADC0Seq3_Handler(void)
{
    ADC0SS3_Service();
}

void ADC0SS3_IRQHandler(void)
{
    ADC0SS3_Service();
}

void ADC0Seq3_IRQHandler(void)
{
    ADC0SS3_Service();
}