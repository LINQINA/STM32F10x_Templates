#ifndef _VERSION_H_
#define _VERSION_H_

#define APP_FIRMWARE_VERSION         "00.00.02"

typedef struct {
    char headBuff[16];
    char versionBuff[16];
} productType;

/* 获取 Boot 版本信息 */
char *pcVersionBootHardGet(void);
char *pcVersionBootSoftGet(void);
char *pcVersionBootDateGet(void);
char *pcVersionBootTypeGet(void);

/* 获取 Bootloader 版本信息 */
char *pcVersionBootloaderHardGet(void);
char *pcVersionBootloaderSoftGet(void);
char *pcVersionBootloaderDateGet(void);
char *pcVersionBootloaderTypeGet(void);

/* 获取 APP 版本信息 */
char *pcVersionAPPHardGet(void);
char *pcVersionAPPSoftGet(void);
char *pcVersionAPPDateGet(void);
char *pcVersionAPPTypeGet(void);

/* 获取产品型号 */
productType *ptypeProductGet(void);

/* 更新产品型号 */
int8_t cProductInfoUpdate(void);

#endif
