#ifndef UART_H
#define UART_H

#include <stdint.h>
#include <stdbool.h>

void UART0_Init(void);

void UART0_SendChar(char c);
void UART0_SendString(const char *str);
void UART0_SendCRLF(void);
void UART0_SendUnsigned(uint32_t n);
void UART0_SendSigned(int32_t n);

bool UART0_RxAvailable(void);
char UART0_RxPop(void);

void UART0_Handler(void);
void UART0_IRQHandler(void);

#endif