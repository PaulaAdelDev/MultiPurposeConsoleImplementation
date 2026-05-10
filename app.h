#ifndef APP_H
#define APP_H

#include <stdint.h>
#include <stdbool.h>

typedef enum
{
    APP_MODE_CALCULATOR = 0,
    APP_MODE_TIMER,
    APP_MODE_STOPWATCH,
    APP_MODE_MONITOR,
    APP_MODE_COUNT
} AppMode_t;

void APP_Init(void);
void APP_Run(void);

void APP_PrintWelcome(void);
void APP_PrintCurrentModeMenu(void);

void APP_ButtonNextMode(void);
void APP_ButtonPreviousMode(void);

void APP_10msTick(void);
void APP_StopwatchOneSecondTick(void);
void APP_ADCNewReading(uint32_t raw);

#endif