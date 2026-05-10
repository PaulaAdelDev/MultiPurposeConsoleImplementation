#ifndef GPIO_H
#define GPIO_H

#include <stdint.h>
#include <stdbool.h>

/*
    External pushbuttons:
    PB0 = Next / Forward mode
    PB1 = Previous / Backward mode

    Wiring:
    PB0 ---- pushbutton ---- GND
    PB1 ---- pushbutton ---- GND

    Internal pull-up resistors are enabled.
    Not pressed = HIGH
    Pressed     = LOW
*/
#define GPIO_NEXT_BUTTON        0x01UL   /* PB0 */
#define GPIO_PREV_BUTTON        0x02UL   /* PB1 */
#define GPIO_BUTTON_MASK        (GPIO_NEXT_BUTTON | GPIO_PREV_BUTTON)

/* PB2 = external buzzer output */
#define GPIO_BUZZER             0x04UL   /* PB2 */

/*
    LEDs on Port F:
    PF1 = Red alert LED
    PF2 = Blue pause LED
    PF3 = Green button-press LED
*/
#define GPIO_RED_LED            0x02UL   /* PF1 */
#define GPIO_BLUE_LED           0x04UL   /* PF2 */
#define GPIO_GREEN_LED          0x08UL   /* PF3 */

#define GPIO_ALERT_LED          GPIO_RED_LED
#define GPIO_PAUSE_LED          GPIO_BLUE_LED
#define GPIO_BUTTON_LED         GPIO_GREEN_LED

void GPIO_Init(void);

void GPIO_ButtonLedOn(void);
void GPIO_ButtonLedOff(void);

void GPIO_PauseLedOn(void);
void GPIO_PauseLedOff(void);

void GPIO_AlertOn(void);
void GPIO_AlertOff(void);
void GPIO_AlertToggle(void);

/*
    Called from SysTick every 10 ms.
    It does not create button events.
    It only keeps the button interrupt locked until the physical button is fully released.
*/
void GPIO_10msTick(void);

void GPIOB_Handler(void);
void GPIOPortB_Handler(void);
void GPIOB_IRQHandler(void);
void GPIOPortB_IRQHandler(void);

#endif