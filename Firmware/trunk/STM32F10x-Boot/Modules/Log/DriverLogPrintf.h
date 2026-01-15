#ifndef _DriverLogPrintf_H_
#define _DriverLogPrintf_H_

#include "DevicesTime.h"

typedef enum {
    LogPrintfDate       = 0x01000000,   /* 打印日期 */
    LogPrintfTime       = 0x02000000,   /* 打印时间 */
    LogPrintfFile       = 0x04000000,   /* 打印文件名 */
    LogPrintfLine       = 0x08000000,   /* 打印代码行号 */
    LogPrintfFunc       = 0x10000000,   /* 打印函数名 */

    LogPrintfNormal     = 0x00000100,   /* 打印普通log */
    LogPrintfError      = 0x00000200,   /* 打印错误log */
    LogPrintfSystem     = 0x00000400,   /* 打印系统log */

    LogPrintfRS485Log   = 0x00000001,   /* 通过 RS485 Log 通道输出 */
    LogPrintfRS485Bus   = 0x00000002,   /* 通过 RS485 Bus 通道输出 */
    LogPrintfUartWifi   = 0x00000004,   /* 通过 Uart Wifi 通道输出 */
    LogPrintfCan        = 0x00000008,   /* 通过 CAN 通道输出 */
    LogPrintfUsbCdc1    = 0x00000010,   /* 通过 USB CDC 1 通道输出 */
    LogPrintfUsbCdc2    = 0x00000020,   /* 通过 USB CDC 2 通道输出 */
} LogPrintfSwitchEnum;


void vLogPrintf(const char *format, ...);
int8_t cLogPrintfStop(LogPrintfSwitchEnum enumSwitchs);
int8_t cLogPrintfSwitchsSet(LogPrintfSwitchEnum enumSwitchs);
LogPrintfSwitchEnum enumLogPrintfSwitchsGet(void);


/* 核心打印函数 */
#define _cLogPrintf_(enumSwitchs, format, ...)                  \
do{                                                             \
    /* 过滤打印通道和打印类型 */                                \
    if((enumSwitchs & 0xFF00 & enumLogPrintfSwitchsGet()) == 0  \
    || (enumSwitchs & 0x00FF & enumLogPrintfSwitchsGet()) == 0) \
    {                                                           \
        break;                                                  \
    }                                                           \
                                                                \
    vLogPrintf(":");                                            \
                                                                \
    if(enumSwitchs & LogPrintfTime)                             \
    {                                                           \
        vLogPrintf("[Time:%lld] ", lTimeGetStamp());             \
    }                                                           \
                                                                \
    if(enumSwitchs & LogPrintfFile)                             \
    {                                                           \
        vLogPrintf("[File:%s] ", __FILE__);                     \
    }                                                           \
                                                                \
    if(enumSwitchs & LogPrintfFunc)                             \
    {                                                           \
        vLogPrintf("[Func:%s] ", __func__);                     \
    }                                                           \
                                                                \
    if(enumSwitchs & LogPrintfLine)                             \
    {                                                           \
        vLogPrintf("[Line:%d] ", __LINE__);                     \
    }                                                           \
                                                                \
    vLogPrintf(format, ##__VA_ARGS__);                          \
                                                                \
    vLogPrintf("\r");                                           \
                                                                \
    cLogPrintfStop(enumSwitchs);                                \
}while(0)

/* 打印（基础） */
#define cLogPrintfNormal(format, ...)           _cLogPrintf_((LogPrintfRS485Log | LogPrintfNormal), (format), ##__VA_ARGS__)
#define cLogPrintfError(format, ...)            _cLogPrintf_((LogPrintfRS485Log | LogPrintfError ), (format), ##__VA_ARGS__)
#define cLogPrintfSystem(format, ...)           _cLogPrintf_((LogPrintfRS485Log | LogPrintfSystem), (format), ##__VA_ARGS__)

/* 打印（时间） */
#define cLogPrintfNormalTime(format, ...)       _cLogPrintf_((LogPrintfRS485Log | LogPrintfNormal | LogPrintfTime), (format), ##__VA_ARGS__)
#define cLogPrintfErrorTime(format, ...)        _cLogPrintf_((LogPrintfRS485Log | LogPrintfError  | LogPrintfTime), (format), ##__VA_ARGS__)
#define cLogPrintfSystemTime(format, ...)       _cLogPrintf_((LogPrintfRS485Log | LogPrintfSystem | LogPrintfTime), (format), ##__VA_ARGS__)

/* 打印（时间 + 文件行号） */
#define cLogPrintfNormalTimeFile(format, ...)   _cLogPrintf_((LogPrintfRS485Log | LogPrintfNormal | LogPrintfTime | LogPrintfFile | LogPrintfLine), (format), ##__VA_ARGS__)
#define cLogPrintfErrorTimeFile(format, ...)    _cLogPrintf_((LogPrintfRS485Log | LogPrintfError  | LogPrintfTime | LogPrintfFile | LogPrintfLine), (format), ##__VA_ARGS__)
#define cLogPrintfSystemTimeFile(format, ...)   _cLogPrintf_((LogPrintfRS485Log | LogPrintfSystem | LogPrintfTime | LogPrintfFile | LogPrintfLine), (format), ##__VA_ARGS__)

/* 打印（所有） */
#define cLogPrintfNormalAll(format, ...)        _cLogPrintf_((LogPrintfRS485Log | LogPrintfNormal | LogPrintfTime | LogPrintfFunc| LogPrintfFile | LogPrintfLine), (format), ##__VA_ARGS__)
#define cLogPrintfErrorAll(format, ...)         _cLogPrintf_((LogPrintfRS485Log | LogPrintfError  | LogPrintfTime | LogPrintfFunc| LogPrintfFile | LogPrintfLine), (format), ##__VA_ARGS__)
#define cLogPrintfSystemAll(format, ...)        _cLogPrintf_((LogPrintfRS485Log | LogPrintfSystem | LogPrintfTime | LogPrintfFunc| LogPrintfFile | LogPrintfLine), (format), ##__VA_ARGS__)

/* Iot打印（基础） */
#define cLogPrintfIotNormal(format, ...)        _cLogPrintf_((LogPrintfUartWifi | LogPrintfNormal), (format), ##__VA_ARGS__)


#endif // _DriverLogPrintf_H_
