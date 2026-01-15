#include "stm32f1xx.h"
#include "stdio.h"
#include "stdint.h"
#include "string.h"
#include "stdarg.h"

#include "DevicesUart.h"

#include "DriverLogPrintf.h"


/* 允许的打印等级、通道 */
LogPrintfSwitchEnum st_enumLogSwitchs = (LogPrintfNormal | LogPrintfError | LogPrintfSystem) | LogPrintfUartWifi | LogPrintfRS485Log;

/* printf一次性最大的输出长度 */
char st_cLogPrintfBuff[256];
volatile int32_t st_cLogPrintfLength = 0;


void vLogPrintf(const char *format, ...)
{
    va_list args;

    va_start(args, format);
    st_cLogPrintfLength += vsnprintf(&st_cLogPrintfBuff[st_cLogPrintfLength], sizeof(st_cLogPrintfBuff) - st_cLogPrintfLength, format, args);
    va_end(args);
}

int8_t cLogPrintfStop(LogPrintfSwitchEnum enumSwitchs)
{
    /* USART Log */
    if(enumSwitchs & LogPrintfRS485Log)
    {
        vUartDMASendDatas((uint32_t)UART_LOG, st_cLogPrintfBuff, st_cLogPrintfLength);
    }

    st_cLogPrintfLength = 0;

    return 0;
}

/* 设置打印等级、通道 */
int8_t cLogPrintfSwitchsSet(LogPrintfSwitchEnum enumSwitchs)
{
    st_enumLogSwitchs = enumSwitchs & 0xFFFF;

    return 0;
}

LogPrintfSwitchEnum enumLogPrintfSwitchsGet(void)
{
    return st_enumLogSwitchs;
}
