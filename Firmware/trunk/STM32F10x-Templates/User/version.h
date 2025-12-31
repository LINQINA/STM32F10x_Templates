#ifndef _VERSION_H_
#define _VERSION_H_


#define PRODUCT_TYPE_DEFAULT            "P12SLHSDBS-0"


#define PD_VERSION_APP_HARDWARE_HEAD    "HPD-V"
#define PD_VERSION_APP_SOFTWARE_HEAD    "SPD-V"
#define PD_VERSION_APP_SOFTWARE         "00.00.02"

#define Host_FIRMWARE_VERSION           "00.00.01"


typedef struct{
    char headBuff[16];
    char versionBuff[16];

    uint16_t hardUid;           /* 硬件型号 */
    char series;                /* 系列：P(便携)、S(家储) */
    char modules;               /* 系列：M(主机)、B(加电包)、S(太阳能)、A(配件)、C(充电桩转接盒BOX)、D(配电箱)、K（电芯模组pack系列） */
    int8_t batteryCapacity;     /* 电池容量：13（1331.2Wh）、17（1715.2Wh）、23（2329.6Wh）、30（3001.6Wh）、35（3430.4Wh）、53（5324.8Wh） */
    int8_t power;               /* 功率：03(300W)、05  (500W)、12(1200W)、22(2200W)、36(3600W)、50(5000W)、60(6000W)、72(7200W)、100(10KW) */
    int8_t powerAC;             /* AC功率：03(300W)、05  (500W)、12(1200W)、22(2200W)、36(3600W)、50(5000W)、60(6000W)、72(7200W)、100(10KW) */
    char grade;                 /* 级别：S（标准版本）、C（降成本版本）、P（升级版本） */
    char voltageLevel;          /* 电压等级 */
}productType;


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
