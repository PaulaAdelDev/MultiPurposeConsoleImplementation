#ifndef ADC_H
#define ADC_H

#include <stdint.h>

void ADC0_Init(void);

void ADC0SS3_Handler(void);
void ADC0Seq3_Handler(void);
void ADC0SS3_IRQHandler(void);
void ADC0Seq3_IRQHandler(void);

#endif