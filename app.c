#include <stdint.h>
#include <stdbool.h>

#include "app.h"
#include "uart.h"
#include "gpio.h"

#define APP_CALC_BUFFER_SIZE        64U
#define APP_CMD_BUFFER_SIZE         64U
#define APP_FIXED_SCALE             1000L

#define APP_ALERT_TOGGLE_COUNT      6U
#define APP_ALERT_HALF_PERIOD_TICKS 50U
#define APP_BUTTON_LED_TICKS        10U

static volatile AppMode_t g_currentMode = APP_MODE_CALCULATOR;
static volatile bool g_modeChanged = false;

static char g_calcBuffer[APP_CALC_BUFFER_SIZE];
static uint32_t g_calcIndex = 0U;

static char g_cmdBuffer[APP_CMD_BUFFER_SIZE];
static uint32_t g_cmdIndex = 0U;

static volatile uint32_t g_timerSeconds = 0U;
static volatile bool g_timerRunning = false;
static volatile bool g_timerPausedIndicator = false;
static volatile bool g_timerFinishedFlag = false;
static volatile bool g_timerPrintFlag = false;

static volatile uint32_t g_stopwatchSeconds = 0U;
static volatile bool g_stopwatchRunning = false;
static volatile bool g_stopwatchPausedIndicator = false;
static volatile bool g_stopwatchPrintFlag = false;

static volatile uint32_t g_adcRaw = 0U;
static volatile uint32_t g_adcMilliVolt = 0U;
static volatile uint32_t g_adcPercent = 0U;
static volatile bool g_adcPrintFlag = false;
static volatile uint32_t g_thresholdPercent = 70U;
static volatile bool g_thresholdWarningLatched = false;

static volatile bool g_alertRunning = false;
static volatile uint8_t g_alertTogglesRemaining = 0U;
static volatile uint16_t g_alertTickCounter = 0U;

static volatile uint16_t g_buttonLedTicks = 0U;
static volatile uint16_t g_systick10msCounter = 0U;

static const char *APP_ModeName(AppMode_t mode);
static void APP_ProcessUARTChar(char c);

static void APP_ResetCalcBuffer(void);
static void APP_ResetCommandBuffer(void);

static void APP_CalculatorProcessChar(char c);
static void APP_CalculatorEvaluate(const char *expr);
static bool APP_ParseFixedNumber(const char *s, uint32_t *index, int32_t *value);
static void APP_PrintFixed3(int32_t value);

static void APP_TimerProcessChar(char c);
static void APP_TimerProcessCommandLine(const char *cmd);
static bool APP_ParseTimeMMSS(const char *cmd, uint32_t *secondsOut);

static void APP_StopwatchProcessChar(char c);

static void APP_MonitorProcessChar(char c);
static void APP_MonitorProcessCommandLine(const char *cmd);

static bool APP_StartsWithSET(const char *s);
static uint32_t APP_StringToUnsigned(const char *s, bool *ok);

static void APP_StartAlert(void);
static void APP_ServiceAlert10ms(void);
static void APP_ServiceButtonLed10ms(void);
static void APP_UpdatePauseLedForCurrentMode(void);

static void APP_PrintTime(uint32_t totalSeconds);
static void APP_PrintADCReading(void);

void APP_Init(void)
{
    g_currentMode = APP_MODE_CALCULATOR;
    g_modeChanged = false;

    g_timerSeconds = 0U;
    g_timerRunning = false;
    g_timerPausedIndicator = false;
    g_timerFinishedFlag = false;
    g_timerPrintFlag = false;

    g_stopwatchSeconds = 0U;
    g_stopwatchRunning = false;
    g_stopwatchPausedIndicator = false;
    g_stopwatchPrintFlag = false;

    g_adcRaw = 0U;
    g_adcMilliVolt = 0U;
    g_adcPercent = 0U;
    g_adcPrintFlag = false;

    g_thresholdPercent = 70U;
    g_thresholdWarningLatched = false;

    g_alertRunning = false;
    g_alertTogglesRemaining = 0U;
    g_alertTickCounter = 0U;

    g_buttonLedTicks = 0U;
    g_systick10msCounter = 0U;

    GPIO_PauseLedOff();

    APP_ResetCalcBuffer();
    APP_ResetCommandBuffer();
}

void APP_Run(void)
{
    char c;

    while (UART0_RxAvailable())
    {
        c = UART0_RxPop();
        APP_ProcessUARTChar(c);
    }

    if (g_modeChanged)
    {
        g_modeChanged = false;
        APP_PrintCurrentModeMenu();
    }

    if (g_timerPrintFlag)
    {
        g_timerPrintFlag = false;

        if (g_currentMode == APP_MODE_TIMER)
        {
            UART0_SendString("Timer: ");
            APP_PrintTime(g_timerSeconds);
            UART0_SendCRLF();
        }
    }

    if (g_timerFinishedFlag)
    {
        g_timerFinishedFlag = false;
        g_timerPausedIndicator = false;
        APP_UpdatePauseLedForCurrentMode();
        UART0_SendString("Timer Done!");
        UART0_SendCRLF();
        APP_StartAlert();
    }

    if (g_stopwatchPrintFlag)
    {
        g_stopwatchPrintFlag = false;

        if (g_currentMode == APP_MODE_STOPWATCH)
        {
            UART0_SendString("Stopwatch: ");
            APP_PrintTime(g_stopwatchSeconds);
            UART0_SendCRLF();
        }
    }

    if (g_adcPrintFlag)
	{
    g_adcPrintFlag = false;

    if (g_currentMode == APP_MODE_MONITOR)
    {
        /*
            If user is typing a command like SET 70,
            do not print ADC readings until Enter is pressed.
        */
        if (g_cmdIndex == 0U)
        {
            APP_PrintADCReading();
        }

        if (g_adcPercent > g_thresholdPercent)
        {
            if (!g_thresholdWarningLatched)
            {
                g_thresholdWarningLatched = true;

                if (g_cmdIndex == 0U)
                {
                    UART0_SendString("WARNING: Threshold exceeded!");
                    UART0_SendCRLF();
                }

                APP_StartAlert();
            }
        }
        else
        {
            g_thresholdWarningLatched = false;
        }
    }
	}
}

void APP_PrintWelcome(void)
{
    UART0_SendCRLF();
    UART0_SendString("==============================================");
    UART0_SendCRLF();
    UART0_SendString(" Multi-Function Embedded Console Project");
    UART0_SendCRLF();
    UART0_SendString(" MCU: TM4C123GH6PM / Tiva C");
    UART0_SendCRLF();
    UART0_SendString(" External Button on PB0 = Next Mode");
    UART0_SendCRLF();
    UART0_SendString(" External Button on PB1 = Previous Mode");
    UART0_SendCRLF();
    UART0_SendString(" PF3 Green LED = Button Press LED");
    UART0_SendCRLF();
    UART0_SendString(" PF2 Blue LED = Timer/Stopwatch Pause LED");
    UART0_SendCRLF();
    UART0_SendString(" PF1 Red LED = Alert LED");
    UART0_SendCRLF();
    UART0_SendString(" PB2 = External Buzzer");
    UART0_SendCRLF();
    UART0_SendString(" PE3 / AIN0 = Potentiometer");
    UART0_SendCRLF();
    UART0_SendString("==============================================");
    UART0_SendCRLF();
}

void APP_PrintCurrentModeMenu(void)
{
    UART0_SendCRLF();
    UART0_SendString("----------------------------------------------");
    UART0_SendCRLF();

    UART0_SendString("Current Mode: ");
    UART0_SendString(APP_ModeName(g_currentMode));
    UART0_SendCRLF();

    if (g_currentMode == APP_MODE_CALCULATOR)
    {
        UART0_SendString("Calculator Mode");
        UART0_SendCRLF();
        UART0_SendString("Input format: number operator number D");
        UART0_SendCRLF();
        UART0_SendString("A = +, B = -, C = /, * = x, D = =");
        UART0_SendCRLF();
        UART0_SendString("Example: 12.5A3.2D");
        UART0_SendCRLF();
    }
    else if (g_currentMode == APP_MODE_TIMER)
    {
        UART0_SendString("Timer Mode");
        UART0_SendCRLF();
        UART0_SendString("Enter time as MM:SS then press Enter.");
        UART0_SendCRLF();
        UART0_SendString("S = Start, P = Pause, R = Reset");
        UART0_SendCRLF();
        UART0_SendString("Current Timer: ");
        APP_PrintTime(g_timerSeconds);
        UART0_SendCRLF();
    }
    else if (g_currentMode == APP_MODE_STOPWATCH)
    {
        UART0_SendString("Stopwatch Mode");
        UART0_SendCRLF();
        UART0_SendString("S = Start, P = Pause, R = Reset");
        UART0_SendCRLF();
        UART0_SendString("Current Stopwatch: ");
        APP_PrintTime(g_stopwatchSeconds);
        UART0_SendCRLF();
    }
    else if (g_currentMode == APP_MODE_MONITOR)
    {
        UART0_SendString("Potentiometer Monitor Mode");
        UART0_SendCRLF();
        UART0_SendString("ADC auto-sampling every 1 second.");
        UART0_SendCRLF();
        UART0_SendString("Use: SET xx");
        UART0_SendCRLF();
        UART0_SendString("Example: SET 70");
        UART0_SendCRLF();
        UART0_SendString("Current Threshold: ");
        UART0_SendUnsigned(g_thresholdPercent);
        UART0_SendString("%");
        UART0_SendCRLF();
    }

    UART0_SendString("----------------------------------------------");
    UART0_SendCRLF();
}

void APP_ButtonNextMode(void)
{
    uint32_t mode;

    GPIO_ButtonLedOn();
    g_buttonLedTicks = APP_BUTTON_LED_TICKS;

    mode = (uint32_t)g_currentMode;
    mode++;

    if (mode >= (uint32_t)APP_MODE_COUNT)
    {
        mode = 0U;
    }

    g_currentMode = (AppMode_t)mode;
    g_modeChanged = true;

    APP_ResetCalcBuffer();
    APP_ResetCommandBuffer();
    APP_UpdatePauseLedForCurrentMode();

}

void APP_ButtonPreviousMode(void)
{
    int32_t mode;

    GPIO_ButtonLedOn();
    g_buttonLedTicks = APP_BUTTON_LED_TICKS;

    mode = (int32_t)g_currentMode;
    mode--;

    if (mode < 0)
    {
        mode = ((int32_t)APP_MODE_COUNT) - 1;
    }

    g_currentMode = (AppMode_t)mode;
    g_modeChanged = true;

    APP_ResetCalcBuffer();
    APP_ResetCommandBuffer();
    APP_UpdatePauseLedForCurrentMode();

}

void APP_10msTick(void)
{
    GPIO_10msTick();
    APP_ServiceButtonLed10ms();
    APP_ServiceAlert10ms();

    g_systick10msCounter++;

    if (g_systick10msCounter >= 100U)
    {
        g_systick10msCounter = 0U;

        if (g_timerRunning)
        {
            if (g_timerSeconds > 0U)
            {
                g_timerSeconds--;
                g_timerPrintFlag = true;

                if (g_timerSeconds == 0U)
                {
                    g_timerRunning = false;
                    g_timerPausedIndicator = false;
                    g_timerFinishedFlag = true;
                }
            }
        }
    }
}

void APP_StopwatchOneSecondTick(void)
{
    if (g_stopwatchRunning)
    {
        g_stopwatchSeconds++;
        g_stopwatchPrintFlag = true;
    }
}

void APP_ADCNewReading(uint32_t raw)
{
    g_adcRaw = raw;
    g_adcMilliVolt = (raw * 3300UL) / 4095UL;
    g_adcPercent = (raw * 100UL) / 4095UL;
    g_adcPrintFlag = true;
}

static const char *APP_ModeName(AppMode_t mode)
{
    if (mode == APP_MODE_CALCULATOR)
    {
        return "Calculator";
    }
    else if (mode == APP_MODE_TIMER)
    {
        return "Timer";
    }
    else if (mode == APP_MODE_STOPWATCH)
    {
        return "Stopwatch";
    }
    else if (mode == APP_MODE_MONITOR)
    {
        return "Potentiometer Monitor";
    }

    return "Unknown";
}

static void APP_ProcessUARTChar(char c)
{
    UART0_SendChar(c);

    if (g_currentMode == APP_MODE_CALCULATOR)
    {
        APP_CalculatorProcessChar(c);
    }
    else if (g_currentMode == APP_MODE_TIMER)
    {
        APP_TimerProcessChar(c);
    }
    else if (g_currentMode == APP_MODE_STOPWATCH)
    {
        APP_StopwatchProcessChar(c);
    }
    else if (g_currentMode == APP_MODE_MONITOR)
    {
        APP_MonitorProcessChar(c);
    }
}

static void APP_ResetCalcBuffer(void)
{
    uint32_t i;

    g_calcIndex = 0U;

    for (i = 0U; i < APP_CALC_BUFFER_SIZE; i++)
    {
        g_calcBuffer[i] = '\0';
    }
}

static void APP_ResetCommandBuffer(void)
{
    uint32_t i;

    g_cmdIndex = 0U;

    for (i = 0U; i < APP_CMD_BUFFER_SIZE; i++)
    {
        g_cmdBuffer[i] = '\0';
    }
}

static void APP_CalculatorProcessChar(char c)
{
    if ((c == '\r') || (c == '\n'))
    {
        return;
    }

    if ((c == 'R') || (c == 'r'))
    {
        APP_ResetCalcBuffer();
        UART0_SendCRLF();
        UART0_SendString("Calculator cleared.");
        UART0_SendCRLF();
        return;
    }

    if (g_calcIndex < (APP_CALC_BUFFER_SIZE - 1U))
    {
        g_calcBuffer[g_calcIndex] = c;
        g_calcIndex++;
        g_calcBuffer[g_calcIndex] = '\0';
    }
    else
    {
        APP_ResetCalcBuffer();
        UART0_SendCRLF();
        UART0_SendString("ERROR: Calculator input too long.");
        UART0_SendCRLF();
        APP_StartAlert();
        return;
    }

    if ((c == 'D') || (c == 'd'))
    {
        UART0_SendCRLF();
        APP_CalculatorEvaluate(g_calcBuffer);
        APP_ResetCalcBuffer();
    }
}

static bool APP_ParseFixedNumber(const char *s, uint32_t *index, int32_t *value)
{
    int32_t sign = 1;
    int32_t integerPart = 0;
    int32_t fractionPart = 0;
    int32_t fractionDigits = 0;
    bool hasDigit = false;

    if (s[*index] == '-')
    {
        sign = -1;
        (*index)++;
    }

    while ((s[*index] >= '0') && (s[*index] <= '9'))
    {
        hasDigit = true;
        integerPart = (integerPart * 10) + (s[*index] - '0');
        (*index)++;
    }

    if (s[*index] == '.')
    {
        (*index)++;

        while ((s[*index] >= '0') && (s[*index] <= '9') && (fractionDigits < 3))
        {
            fractionPart = (fractionPart * 10) + (s[*index] - '0');
            fractionDigits++;
            (*index)++;
        }

        while ((s[*index] >= '0') && (s[*index] <= '9'))
        {
            (*index)++;
        }
    }

    while (fractionDigits < 3)
    {
        fractionPart *= 10;
        fractionDigits++;
    }

    if (!hasDigit)
    {
        return false;
    }

    *value = sign * ((integerPart * APP_FIXED_SCALE) + fractionPart);
    return true;
}

static void APP_CalculatorEvaluate(const char *expr)
{
    uint32_t index = 0U;
    int32_t num1 = 0;
    int32_t num2 = 0;
    int32_t result = 0;
    int64_t temp;
    char op;

    if (!APP_ParseFixedNumber(expr, &index, &num1))
    {
        UART0_SendString("ERROR: Invalid first number.");
        UART0_SendCRLF();
        APP_StartAlert();
        return;
    }

    op = expr[index];

    if ((op != 'A') && (op != 'a') &&
        (op != 'B') && (op != 'b') &&
        (op != 'C') && (op != 'c') &&
        (op != '*'))
    {
        UART0_SendString("ERROR: Invalid operator.");
        UART0_SendCRLF();
        APP_StartAlert();
        return;
    }

    index++;

    if (!APP_ParseFixedNumber(expr, &index, &num2))
    {
        UART0_SendString("ERROR: Invalid second number.");
        UART0_SendCRLF();
        APP_StartAlert();
        return;
    }

    if ((expr[index] != 'D') && (expr[index] != 'd'))
    {
        UART0_SendString("ERROR: Missing D equals key.");
        UART0_SendCRLF();
        APP_StartAlert();
        return;
    }

    if ((op == 'A') || (op == 'a'))
    {
        result = num1 + num2;
    }
    else if ((op == 'B') || (op == 'b'))
    {
        result = num1 - num2;
    }
    else if (op == '*')
    {
        temp = ((int64_t)num1 * (int64_t)num2) / APP_FIXED_SCALE;
        result = (int32_t)temp;
    }
    else if ((op == 'C') || (op == 'c'))
    {
        if (num2 == 0)
        {
            UART0_SendString("ERROR: Divide by zero.");
            UART0_SendCRLF();
            APP_StartAlert();
            return;
        }

        temp = ((int64_t)num1 * APP_FIXED_SCALE) / num2;
        result = (int32_t)temp;
    }

    UART0_SendString("Result = ");
    APP_PrintFixed3(result);
    UART0_SendCRLF();
}

static void APP_PrintFixed3(int32_t value)
{
    int32_t integerPart;
    int32_t fractionPart;

    if (value < 0)
    {
        UART0_SendChar('-');
        value = -value;
    }

    integerPart = value / APP_FIXED_SCALE;
    fractionPart = value % APP_FIXED_SCALE;

    UART0_SendUnsigned((uint32_t)integerPart);
    UART0_SendChar('.');

    if (fractionPart < 100)
    {
        UART0_SendChar('0');
    }

    if (fractionPart < 10)
    {
        UART0_SendChar('0');
    }

    UART0_SendUnsigned((uint32_t)fractionPart);
}

static void APP_TimerProcessChar(char c)
{
    if ((c == '\r') || (c == '\n'))
    {
        UART0_SendCRLF();
        APP_TimerProcessCommandLine(g_cmdBuffer);
        APP_ResetCommandBuffer();
        return;
    }

    if ((c == 'S') || (c == 's'))
    {
        if (g_timerSeconds > 0U)
        {
            g_timerRunning = true;
            g_timerPausedIndicator = false;
            APP_UpdatePauseLedForCurrentMode();
            UART0_SendCRLF();
            UART0_SendString("Timer started.");
            UART0_SendCRLF();
        }
        else
        {
            UART0_SendCRLF();
            UART0_SendString("Enter time first as MM:SS.");
            UART0_SendCRLF();
        }

        return;
    }

    if ((c == 'P') || (c == 'p'))
    {
        g_timerRunning = false;

        if (g_timerSeconds > 0U)
        {
            g_timerPausedIndicator = true;
        }

        APP_UpdatePauseLedForCurrentMode();
        UART0_SendCRLF();
        UART0_SendString("Timer paused at: ");
        APP_PrintTime(g_timerSeconds);
        UART0_SendCRLF();
        return;
    }

    if ((c == 'R') || (c == 'r'))
    {
        g_timerRunning = false;
        g_timerPausedIndicator = false;
        g_timerSeconds = 0U;
        APP_ResetCommandBuffer();
        APP_UpdatePauseLedForCurrentMode();

        UART0_SendCRLF();
        UART0_SendString("Timer reset. Enter new time as MM:SS.");
        UART0_SendCRLF();
        return;
    }

    if (g_cmdIndex < (APP_CMD_BUFFER_SIZE - 1U))
    {
        g_cmdBuffer[g_cmdIndex] = c;
        g_cmdIndex++;
        g_cmdBuffer[g_cmdIndex] = '\0';
    }
}

static void APP_TimerProcessCommandLine(const char *cmd)
{
    uint32_t seconds;

    if (cmd[0] == '\0')
    {
        return;
    }

    if (APP_ParseTimeMMSS(cmd, &seconds))
    {
        g_timerSeconds = seconds;
        g_timerRunning = false;
        g_timerPausedIndicator = false;
        APP_UpdatePauseLedForCurrentMode();

        UART0_SendString("Timer loaded: ");
        APP_PrintTime(g_timerSeconds);
        UART0_SendCRLF();
        UART0_SendString("Press S to start.");
        UART0_SendCRLF();
    }
    else
    {
        UART0_SendString("ERROR: Invalid time. Use MM:SS.");
        UART0_SendCRLF();
        APP_StartAlert();
    }
}

static bool APP_ParseTimeMMSS(const char *cmd, uint32_t *secondsOut)
{
    uint32_t i = 0U;
    uint32_t minutes = 0U;
    uint32_t seconds = 0U;

    if (cmd[0] == '\0')
    {
        return false;
    }

    while ((cmd[i] >= '0') && (cmd[i] <= '9'))
    {
        minutes = (minutes * 10U) + (uint32_t)(cmd[i] - '0');
        i++;
    }

    if (cmd[i] != ':')
    {
        return false;
    }

    i++;

    while ((cmd[i] >= '0') && (cmd[i] <= '9'))
    {
        seconds = (seconds * 10U) + (uint32_t)(cmd[i] - '0');
        i++;
    }

    if (cmd[i] != '\0')
    {
        return false;
    }

    if (seconds >= 60U)
    {
        return false;
    }

    *secondsOut = (minutes * 60U) + seconds;
    return true;
}

static void APP_StopwatchProcessChar(char c)
{
    bool wasRunning;

    if ((c == '\r') || (c == '\n'))
    {
        UART0_SendCRLF();
        return;
    }

    if ((c == 'S') || (c == 's'))
    {
        g_stopwatchRunning = true;
        g_stopwatchPausedIndicator = false;
        APP_UpdatePauseLedForCurrentMode();
        UART0_SendCRLF();
        UART0_SendString("Stopwatch started.");
        UART0_SendCRLF();
    }
    else if ((c == 'P') || (c == 'p'))
    {
        wasRunning = g_stopwatchRunning;
        g_stopwatchRunning = false;

        if (wasRunning || (g_stopwatchSeconds > 0U))
        {
            g_stopwatchPausedIndicator = true;
        }

        APP_UpdatePauseLedForCurrentMode();
        UART0_SendCRLF();
        UART0_SendString("Stopwatch paused at: ");
        APP_PrintTime(g_stopwatchSeconds);
        UART0_SendCRLF();
    }
    else if ((c == 'R') || (c == 'r'))
    {
        g_stopwatchRunning = false;
        g_stopwatchPausedIndicator = false;
        g_stopwatchSeconds = 0U;
        APP_UpdatePauseLedForCurrentMode();
        UART0_SendCRLF();
        UART0_SendString("Stopwatch reset: 00:00");
        UART0_SendCRLF();
    }
}

static void APP_MonitorProcessChar(char c)
{
    if ((c == '\r') || (c == '\n'))
    {
        UART0_SendCRLF();
        APP_MonitorProcessCommandLine(g_cmdBuffer);
        APP_ResetCommandBuffer();
        return;
    }

    if (g_cmdIndex < (APP_CMD_BUFFER_SIZE - 1U))
    {
        g_cmdBuffer[g_cmdIndex] = c;
        g_cmdIndex++;
        g_cmdBuffer[g_cmdIndex] = '\0';
    }
}

static void APP_MonitorProcessCommandLine(const char *cmd)
{
    bool ok;
    uint32_t value;

    if (cmd[0] == '\0')
    {
        UART0_SendString("Current threshold = ");
        UART0_SendUnsigned(g_thresholdPercent);
        UART0_SendString("%");
        UART0_SendCRLF();
        return;
    }

    if (APP_StartsWithSET(cmd))
    {
        value = APP_StringToUnsigned(&cmd[4], &ok);

        if (ok && (value <= 100U))
        {
            g_thresholdPercent = value;
            g_thresholdWarningLatched = false;

            UART0_SendString("Threshold set to ");
            UART0_SendUnsigned(g_thresholdPercent);
            UART0_SendString("%");
            UART0_SendCRLF();
        }
        else
        {
            UART0_SendString("ERROR: Threshold must be 0 to 100.");
            UART0_SendCRLF();
            APP_StartAlert();
        }
    }
    else
    {
        UART0_SendString("ERROR: Use SET xx.");
        UART0_SendCRLF();
        APP_StartAlert();
    }
}

static bool APP_StartsWithSET(const char *s)
{
    return (((s[0] == 'S') || (s[0] == 's')) &&
            ((s[1] == 'E') || (s[1] == 'e')) &&
            ((s[2] == 'T') || (s[2] == 't')) &&
            (s[3] == ' '));
}

static uint32_t APP_StringToUnsigned(const char *s, bool *ok)
{
    uint32_t i = 0U;
    uint32_t value = 0U;
    bool hasDigit = false;

    while (s[i] == ' ')
    {
        i++;
    }

    while ((s[i] >= '0') && (s[i] <= '9'))
    {
        hasDigit = true;
        value = (value * 10U) + (uint32_t)(s[i] - '0');
        i++;
    }

    while (s[i] == ' ')
    {
        i++;
    }

    if (s[i] != '\0')
    {
        *ok = false;
        return 0U;
    }

    *ok = hasDigit;
    return value;
}

static void APP_UpdatePauseLedForCurrentMode(void)
{
    bool pauseLedOn = false;

    if (g_currentMode == APP_MODE_TIMER)
    {
        pauseLedOn = g_timerPausedIndicator;
    }
    else if (g_currentMode == APP_MODE_STOPWATCH)
    {
        pauseLedOn = g_stopwatchPausedIndicator;
    }

    if (pauseLedOn)
    {
        GPIO_PauseLedOn();
    }
    else
    {
        GPIO_PauseLedOff();
    }
}

static void APP_PrintTime(uint32_t totalSeconds)
{
    uint32_t minutes;
    uint32_t seconds;

    minutes = totalSeconds / 60U;
    seconds = totalSeconds % 60U;

    if (minutes < 10U)
    {
        UART0_SendChar('0');
    }

    UART0_SendUnsigned(minutes);
    UART0_SendChar(':');

    if (seconds < 10U)
    {
        UART0_SendChar('0');
    }

    UART0_SendUnsigned(seconds);
}

static void APP_PrintADCReading(void)
{
    UART0_SendString("ADC Raw: ");
    UART0_SendUnsigned(g_adcRaw);

    UART0_SendString(" | Voltage: ");
    UART0_SendUnsigned(g_adcMilliVolt / 1000U);
    UART0_SendChar('.');
    UART0_SendUnsigned((g_adcMilliVolt % 1000U) / 100U);
    UART0_SendUnsigned((g_adcMilliVolt % 100U) / 10U);
    UART0_SendUnsigned(g_adcMilliVolt % 10U);
    UART0_SendString(" V");

    UART0_SendString(" | Level: ");
    UART0_SendUnsigned(g_adcPercent);
    UART0_SendString("%");

    UART0_SendCRLF();
}

static void APP_StartAlert(void)
{
    g_alertRunning = true;
    g_alertTogglesRemaining = APP_ALERT_TOGGLE_COUNT;
    g_alertTickCounter = 0U;
    GPIO_AlertOff();
}

static void APP_ServiceAlert10ms(void)
{
    if (!g_alertRunning)
    {
        return;
    }

    g_alertTickCounter++;

    if (g_alertTickCounter >= APP_ALERT_HALF_PERIOD_TICKS)
    {
        g_alertTickCounter = 0U;

        if (g_alertTogglesRemaining > 0U)
        {
            GPIO_AlertToggle();
            g_alertTogglesRemaining--;
        }

        if (g_alertTogglesRemaining == 0U)
        {
            g_alertRunning = false;
            GPIO_AlertOff();
        }
    }
}

static void APP_ServiceButtonLed10ms(void)
{
    if (g_buttonLedTicks > 0U)
    {
        g_buttonLedTicks--;

        if (g_buttonLedTicks == 0U)
        {
            GPIO_ButtonLedOff();
        }
    }
}
