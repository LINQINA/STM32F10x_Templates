#ifndef _DevicesDelay_H_
#define _DevicesDelay_H_

/* 非阻塞性延时 */
void vDelayUs(int64_t lTime);
void vDelayMs(int64_t lTime);
void vDelayS(int64_t lTime);

/* 非阻塞似延时 */
void vRtosDelayS(float fTime);
void vRtosDelayMs(float fTime);





#endif
