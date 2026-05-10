#include <stdint.h>
#include <stdbool.h>

#include "tm4c123gh6pm.h"
#include "gpio.h"
#include "app.h"

/*
    Strong debounce / one-click lock:
    After one valid press, PB0/PB1 interrupts are disabled.
    They are enabled again only after all buttons are released and stable for 80 ms.
    This prevents one physical click from changing two modes because of mechanical bounce.
*/
#define GPIO_RELEASE_STABLE_TICKS_10MS   8U    /* 8 x 10 ms = 80 ms */

static volatile bool g_waitingForButtonRelease = false;
static volatile uint8_t g_releaseStableTicks = 0U;

static void GPIOB_ISR_Service(void);
static void GPIO_EnableButtonInterrupts(void);
static void GPIO_DisableButtonInterrupts(void);
static bool GPIO_AllButtonsReleased(void);

void GPIO_Init(void)
{
    /*
        Enable clock for Port B and Port F.

        Port B:
        PB0 = Next mode external pushbutton
        PB1 = Previous mode external pushbutton
        PB2 = Buzzer

        Port F:
        PF1 = Red alert LED
        PF2 = Blue pause LED
        PF3 = Green pushbutton LED
    */
    SYSCTL_RCGCGPIO_R |= 0x22UL;

    while ((SYSCTL_PRGPIO_R & 0x22UL) != 0x22UL)
    {
    }

    /*
        Port B configuration.
        PB0 and PB1 are inputs with internal pull-up.
        PB2 is buzzer output.
    */
    GPIO_PORTB_AFSEL_R &= ~(GPIO_BUTTON_MASK | GPIO_BUZZER);
    GPIO_PORTB_AMSEL_R &= ~(GPIO_BUTTON_MASK | GPIO_BUZZER);
    GPIO_PORTB_PCTL_R &= ~0x00000FFFUL;

    GPIO_PORTB_DIR_R &= ~GPIO_BUTTON_MASK;
    GPIO_PORTB_DIR_R |= GPIO_BUZZER;

    GPIO_PORTB_DEN_R |= (GPIO_BUTTON_MASK | GPIO_BUZZER);
    GPIO_PORTB_PUR_R |= GPIO_BUTTON_MASK;

    GPIO_PORTB_DATA_R &= ~GPIO_BUZZER;

    /*
        Falling-edge interrupt on PB0 and PB1.
        With internal pull-up:
        not pressed = HIGH
        pressed     = LOW
    */
    GPIO_PORTB_IM_R &= ~GPIO_BUTTON_MASK;
    GPIO_PORTB_IS_R &= ~GPIO_BUTTON_MASK;
    GPIO_PORTB_IBE_R &= ~GPIO_BUTTON_MASK;
    GPIO_PORTB_IEV_R &= ~GPIO_BUTTON_MASK;
    GPIO_PORTB_ICR_R = GPIO_BUTTON_MASK;
    GPIO_PORTB_IM_R |= GPIO_BUTTON_MASK;

    /* GPIO Port B IRQ number = 1. */
    NVIC_EN0_R |= (1UL << 1);

    /*
        Port F LED configuration only.
        PF0 and PF4 are not used.
    */
    GPIO_PORTF_AFSEL_R &= ~(GPIO_RED_LED | GPIO_BLUE_LED | GPIO_GREEN_LED);
    GPIO_PORTF_AMSEL_R &= ~(GPIO_RED_LED | GPIO_BLUE_LED | GPIO_GREEN_LED);
    GPIO_PORTF_PCTL_R &= ~0x0000FFF0UL;

    GPIO_PORTF_DIR_R |= (GPIO_RED_LED | GPIO_BLUE_LED | GPIO_GREEN_LED);
    GPIO_PORTF_DEN_R |= (GPIO_RED_LED | GPIO_BLUE_LED | GPIO_GREEN_LED);

    GPIO_PORTF_DATA_R &= ~(GPIO_RED_LED | GPIO_BLUE_LED | GPIO_GREEN_LED);
}

void GPIO_ButtonLedOn(void)
{
    GPIO_PORTF_DATA_R |= GPIO_GREEN_LED;
}

void GPIO_ButtonLedOff(void)
{
    GPIO_PORTF_DATA_R &= ~GPIO_GREEN_LED;
}

void GPIO_PauseLedOn(void)
{
    GPIO_PORTF_DATA_R |= GPIO_BLUE_LED;
}

void GPIO_PauseLedOff(void)
{
    GPIO_PORTF_DATA_R &= ~GPIO_BLUE_LED;
}

void GPIO_AlertOn(void)
{
    GPIO_PORTF_DATA_R |= GPIO_RED_LED;
    GPIO_PORTB_DATA_R |= GPIO_BUZZER;
}

void GPIO_AlertOff(void)
{
    GPIO_PORTF_DATA_R &= ~GPIO_RED_LED;
    GPIO_PORTB_DATA_R &= ~GPIO_BUZZER;
}

void GPIO_AlertToggle(void)
{
    GPIO_PORTF_DATA_R ^= GPIO_RED_LED;
    GPIO_PORTB_DATA_R ^= GPIO_BUZZER;
}

static void GPIO_EnableButtonInterrupts(void)
{
    GPIO_PORTB_ICR_R = GPIO_BUTTON_MASK;
    GPIO_PORTB_IM_R |= GPIO_BUTTON_MASK;
}

static void GPIO_DisableButtonInterrupts(void)
{
    GPIO_PORTB_IM_R &= ~GPIO_BUTTON_MASK;
}

static bool GPIO_AllButtonsReleased(void)
{
    return ((GPIO_PORTB_DATA_R & GPIO_BUTTON_MASK) == GPIO_BUTTON_MASK);
}

void GPIO_10msTick(void)
{
    if (g_waitingForButtonRelease)
    {
        if (GPIO_AllButtonsReleased())
        {
            if (g_releaseStableTicks < GPIO_RELEASE_STABLE_TICKS_10MS)
            {
                g_releaseStableTicks++;
            }
            else
            {
                g_waitingForButtonRelease = false;
                g_releaseStableTicks = 0U;
                GPIO_EnableButtonInterrupts();
            }
        }
        else
        {
            g_releaseStableTicks = 0U;
        }
    }
}

static void GPIOB_ISR_Service(void)
{
    uint32_t status;
    uint32_t pressedButtons;

    status = GPIO_PORTB_MIS_R & GPIO_BUTTON_MASK;

    /* Clear interrupt flags immediately. */
    GPIO_PORTB_ICR_R = status;

    if (status == 0UL)
    {
        return;
    }

    /*
        If a previous physical click is still being handled,
        ignore this interrupt and keep buttons locked.
    */
    if (g_waitingForButtonRelease)
    {
        GPIO_DisableButtonInterrupts();
        return;
    }

    pressedButtons = (~GPIO_PORTB_DATA_R) & GPIO_BUTTON_MASK;

    /*
        Accept only one action per interrupt.
        If noise makes both bits active, priority is PB0 then PB1.
    */
    if ((pressedButtons & GPIO_NEXT_BUTTON) != 0UL)
    {
        APP_ButtonNextMode();
        g_waitingForButtonRelease = true;
        g_releaseStableTicks = 0U;
        GPIO_DisableButtonInterrupts();
    }
    else if ((pressedButtons & GPIO_PREV_BUTTON) != 0UL)
    {
        APP_ButtonPreviousMode();
        g_waitingForButtonRelease = true;
        g_releaseStableTicks = 0U;
        GPIO_DisableButtonInterrupts();
    }
    else
    {
        /* Bounce/noise without a valid low level. Ignore it. */
    }
}

void GPIOB_Handler(void)
{
    GPIOB_ISR_Service();
}

void GPIOPortB_Handler(void)
{
    GPIOB_ISR_Service();
}

void GPIOB_IRQHandler(void)
{
    GPIOB_ISR_Service();
}

void GPIOPortB_IRQHandler(void)
{
    GPIOB_ISR_Service();
}