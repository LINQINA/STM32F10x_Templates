#ifndef _VERSION_H_
#define _VERSION_H_


#define VERSION_BOOT_HARDWARE       "00.00.01"
#define VERSION_BOOT_SOFTWARE       "00.00.01"


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


#endif
