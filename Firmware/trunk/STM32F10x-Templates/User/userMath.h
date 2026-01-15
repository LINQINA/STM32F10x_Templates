#ifndef _userMath_H_
#define _userMath_H_


#define MAX(a, b)   ({                          \
                        typeof(a) _a = (a);     \
                        typeof(b) _b = (b);     \
                        (void) (&_a == &_b);    \
                        (_a > _b) ? _a : _b; })

#define MIN(a, b)   ({                          \
                        typeof(a) _a = (a);     \
                        typeof(b) _b = (b);     \
                        (void) (&_a == &_b);    \
                        (_a < _b) ? _a : _b; })

#define ABS(a, b)   ({                          \
                        typeof(a) _a = (a);     \
                        typeof(b) _b = (b);     \
                        (void) (&_a == &_b);    \
                        (_a > _b) ? (_a - _b) : (_b - _a); })

#define FABS(a, b)   ((a > b) ? (a - b) : (b - a))

#define AVERAGE(a, b)   ({                      \
                        typeof(a) _a = (a);     \
                        typeof(b) _b = (b);     \
                        (void) (&_a == &_b);    \
                        (_a & _b) + ((_a ^ _b) >> 1); })

/* 使x对n字节对齐 */
#define roundUp(x, n) (((x) + (n) - 1) & (~((n) - 1)))
/* 使 iValue 对 iBase 对齐（任意值） */
int32_t iRoundUp(int32_t iValue, int32_t iBase);
int32_t iRoundDown(int32_t iValue, int32_t iBase);

/* 大小端转换 */
uint32_t uiSwapUint32(uint32_t uiValue);
uint16_t usSwapUint16(uint16_t usValue);
void vSwapUint32s(uint32_t *puiDatas, int32_t iLength);
/* 选择排序 */
void vSortChoice(uint16_t *pusBuff, int32_t iLength);


#endif
