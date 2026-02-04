# STM32 时钟系统学习笔记

## 一、时钟系统概述

STM32F103 的时钟系统负责为 CPU、外设、总线提供时钟信号，是整个芯片的"心跳"。

时钟系统的核心功能：
- 提供系统主时钟（SYSCLK）
- 为各总线提供时钟（AHB、APB1、APB2）
- 为外设提供独立时钟（RTC、USB、ADC 等）
- 支持时钟源切换和倍频/分频配置

---

## 二、时钟源

STM32F103 有 4 个时钟源：

```
┌─────────────────────────────────────────────────────────────┐
│                       时钟源                                 │
├──────────┬─────────────┬─────────────────────────────────────┤
│ HSI RC   │ 8 MHz       │ 内部高速 RC 振荡器                  │
│ HSE OSC  │ 4-16 MHz    │ 外部高速晶振（通常 8MHz）           │
│ LSE OSC  │ 32.768 kHz  │ 外部低速晶振（RTC 专用）            │
│ LSI RC   │ 40 kHz      │ 内部低速 RC 振荡器                  │
└──────────┴─────────────┴─────────────────────────────────────┘
```

### 时钟源对比：

| 时钟源 | 类型 | 精度 | 需要外部元件 | 主要用途 |
|--------|------|------|-------------|----------|
| HSI | 内部 RC | ±1%~2% | ❌ 不需要 | 系统时钟（备用） |
| HSE | 外部晶振 | ±0.002% | ✅ 需要 | 系统时钟（主用） |
| LSE | 外部晶振 | ±0.002% | ✅ 需要 | RTC 实时时钟 |
| LSI | 内部 RC | ±1%~2% | ❌ 不需要 | 独立看门狗 |

### 晶振 vs RC 振荡器：

| 对比项 | 晶振（Crystal） | RC 振荡器 |
|--------|----------------|-----------|
| 原理 | 石英晶体压电效应 | 电阻电容充放电 |
| 精度 | 高（±20ppm） | 低（±1%~2%） |
| 位置 | 外部 | 芯片内部集成 |
| 成本 | 需要外部元件 | 免费 |

---

## 三、时钟引脚

```
┌─────────────────────────────────────────────────────────────┐
│                    STM32F103 时钟引脚                        │
├──────────────┬────────────┬─────────────────────────────────┤
│ OSC_IN (PD0) │ 高速晶振   │ 8MHz 晶振输入端                 │
│ OSC_OUT(PD1) │ 高速晶振   │ 8MHz 晶振输出端                 │
├──────────────┼────────────┼─────────────────────────────────┤
│ OSC32_IN     │ 低速晶振   │ 32.768kHz 晶振输入端 (PC14)     │
│ OSC32_OUT    │ 低速晶振   │ 32.768kHz 晶振输出端 (PC15)     │
├──────────────┼────────────┼─────────────────────────────────┤
│ MCO (PA8)    │ 时钟输出   │ 可输出内部时钟给外部设备        │
└──────────────┴────────────┴─────────────────────────────────┘
```

### 晶振接法：

```
8MHz晶振接法：                    32.768kHz晶振接法：
                              
  OSC_IN ──┬──||──┬── OSC_OUT     OSC32_IN ──┬──||──┬── OSC32_OUT
           │      │                          │      │
          ═╧═    ═╧═                        ═╧═    ═╧═
          GND    GND                        GND    GND
           ↑      ↑                          ↑      ↑
       负载电容(约20pF)                  负载电容(约6.8pF)
```

### 为什么 RTC 用 32.768kHz？

```
32768 = 2^15
经过 15 次二分频，正好得到 1Hz（1秒）
完美用于实时时钟计时！
```

---

## 四、时钟树结构

```
┌─────────────────────────────────────────────────────────────┐
│                       时钟树                                 │
└─────────────────────────────────────────────────────────────┘

HSI (8MHz) ──┐                    
             ├──► PLLSRC ──► PLL(x2~x16) ──► PLLCLK ──┐
HSE (8MHz) ──┘    (选择器)     (倍频器)               │
    │                                                 │
    └─────────────────────────────────────────────► SW ──► SYSCLK
                                                  (选择器) (最高72MHz)
                                                           │
                                                           ▼
                                                     AHB预分频器
                                                     (/1,2...512)
                                                           │
                                                           ▼
                                                        HCLK
                                                      (最高72MHz)
                                                           │
                      ┌────────────────────────────────────┼────────┐
                      ▼                                    ▼        ▼
                APB1预分频器                          APB2预分频器  /8
                (/1,2,4,8,16)                        (/1,2,4,8,16)  ↓
                      │                                    │      FCLK
                      ▼                                    ▼
                   PCLK1                               PCLK2
                 (最高36MHz)                         (最高72MHz)
```

### 关键概念：

| 名称 | 含义 | 作用 |
|------|------|------|
| SYSCLK | 系统时钟 | 整个系统的主时钟 |
| HCLK | AHB 总线时钟 | CPU、DMA、存储器的时钟 |
| PCLK1 | APB1 总线时钟 | 低速外设时钟（最高36MHz） |
| PCLK2 | APB2 总线时钟 | 高速外设时钟（最高72MHz） |

---

## 五、PLL 倍频器

### 作用：

将低频时钟（8MHz）倍频到高频（72MHz）

```
HSE (8MHz) ──► PLL (×9) ──► PLLCLK (72MHz)
```

### 倍频系数：

| 宏定义 | 倍频 | 输出（HSE=8MHz时） |
|--------|------|-------------------|
| RCC_PLL_MUL2 | ×2 | 16 MHz |
| RCC_PLL_MUL3 | ×3 | 24 MHz |
| ... | ... | ... |
| RCC_PLL_MUL9 | ×9 | **72 MHz** ← 常用 |
| RCC_PLL_MUL16 | ×16 | 128 MHz（超频） |

---

## 六、总线与外设

### AHB 总线（高性能总线）

```
HCLK (72MHz) ──► CPU 内核
             ──► DMA 控制器
             ──► Flash 存储器
             ──► SRAM 内存
```

### APB1 外设（低速总线，最高36MHz）

| 外设类型 | 包含 |
|----------|------|
| 定时器 | TIM2、TIM3、TIM4、TIM5、TIM6、TIM7 |
| 串口 | USART2、USART3、UART4、UART5 |
| I2C | I2C1、I2C2 |
| SPI | SPI2、SPI3 |
| 其他 | CAN、DAC、PWR、BKP、WWDG |

### APB2 外设（高速总线，最高72MHz）

| 外设类型 | 包含 |
|----------|------|
| 定时器 | TIM1、TIM8（高级定时器） |
| 串口 | USART1 |
| SPI | SPI1 |
| ADC | ADC1、ADC2、ADC3 |
| GPIO | GPIOA ~ GPIOG（所有端口） |
| 其他 | AFIO、EXTI |

### 记忆技巧：

> 编号为"1"的往往在 APB2（更快）：USART1、SPI1、TIM1

---

## 七、定时器时钟的特殊规则

### 问题背景：

APB1 总线最高只能 36MHz，但定时器需要高精度。

### 解决方案：

```
如果 APB1 预分频系数 = 1：定时器时钟 = PCLK1（不变）
如果 APB1 预分频系数 ≠ 1：定时器时钟 = PCLK1 × 2（自动翻倍）
```

### 实际效果：

| APB1预分频 | PCLK1 | 定时器时钟(TIM2~7) |
|-----------|-------|-------------------|
| /1 | 72 MHz | 72 MHz（不变） |
| /2 | 36 MHz | 36 × 2 = **72 MHz** |

### 设计目的：

```
外设受限于 APB 总线（36MHz）
定时器需要高精度，所以自动翻倍（72MHz）
让定时器"绕过"APB总线的频率限制
```

---

## 八、CPU 与外设的关系

### CPU（中央处理器）

- 执行程序代码
- 做运算、判断、循环
- 指挥外设工作
- 芯片只有 1 个 CPU

### 外设（Peripheral）

- 芯片内部的功能模块
- 完成特定任务（串口、定时、采集等）
- 被 CPU 配置和控制
- 芯片有多个外设

### 引脚（Pin）

- 芯片与外部世界连接的物理接口
- 外设通过引脚与外界交互

```
┌─────────────────────────────────────────────────────┐
│                    STM32 芯片                        │
│  ┌─────┐      ┌────────┐      ┌────────┐           │
│  │ CPU │◄────►│  外设  │◄────►│  引脚  │◄────► 外部
│  │     │      │ (干活) │      │(数据口)│           │
│  └─────┘      └────────┘      └────────┘           │
│    指挥         处理数据        物理连接            │
└─────────────────────────────────────────────────────┘
```

### 类比理解：

```
CPU（老板）：做决策、发指令、协调各部门
外设（部门）：执行具体任务，各有专长
引脚（门口）：与外界交流的通道
```

---

## 九、时钟配置代码

### HAL 库配置示例：

```c
int8_t cSystemClockInit(void)
{
    RCC_OscInitTypeDef RCC_OscInitStruct = {0};
    RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

    /* 配置 HSE + PLL */
    RCC_OscInitStruct.OscillatorType  = RCC_OSCILLATORTYPE_HSE;
    RCC_OscInitStruct.HSEState        = RCC_HSE_ON;
    RCC_OscInitStruct.HSEPredivValue  = RCC_HSE_PREDIV_DIV1;
    RCC_OscInitStruct.PLL.PLLState    = RCC_PLL_ON;
    RCC_OscInitStruct.PLL.PLLSource   = RCC_PLLSOURCE_HSE;
    RCC_OscInitStruct.PLL.PLLMUL      = RCC_PLL_MUL9;

    if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
    {
        return -1;
    }

    /* 配置 SYSCLK, HCLK, PCLK1, PCLK2 */
    RCC_ClkInitStruct.ClockType      = RCC_CLOCKTYPE_SYSCLK | RCC_CLOCKTYPE_HCLK |
                                       RCC_CLOCKTYPE_PCLK1  | RCC_CLOCKTYPE_PCLK2;
    RCC_ClkInitStruct.SYSCLKSource   = RCC_SYSCLKSOURCE_PLLCLK;
    RCC_ClkInitStruct.AHBCLKDivider  = RCC_SYSCLK_DIV1;
    RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
    RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

    if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2) != HAL_OK)
    {
        return -2;
    }

    return 0;
}
```

### 配置结果：

| 时钟 | 频率 | 计算 |
|------|------|------|
| HSE | 8 MHz | 外部晶振 |
| PLLCLK | 72 MHz | 8 × 9 |
| SYSCLK | 72 MHz | 选择 PLLCLK |
| HCLK | 72 MHz | SYSCLK / 1 |
| PCLK1 | 36 MHz | HCLK / 2 |
| PCLK2 | 72 MHz | HCLK / 1 |

---

## 十、时钟初始化 vs SystemCoreClockUpdate

| 函数 | 作用 | 谁提供 |
|------|------|--------|
| `cSystemClockInit()` | **配置时钟**（设置寄存器） | 你写的 |
| `SystemCoreClockUpdate()` | **读取时钟**（更新全局变量） | CMSIS 提供 |

### 两者关系：

```
cSystemClockInit()       ← 配置硬件，改变时钟
         ↓
SystemCoreClockUpdate()  ← 读取配置，更新变量
         ↓
SystemCoreClock = 72000000  ← 其他代码可用
```

**一个是"设置时钟"，一个是"同步变量"，两者缺一不可！**

---

## 十一、典型时钟配置总览

```
┌─────────────────────────────────────────────────────────────┐
│                    72MHz 典型配置                            │
├─────────────────────────────────────────────────────────────┤
│  HSE(8M) → PLL(×9) → SYSCLK(72M) → HCLK(72M)               │
│                                       │                     │
│                          ┌────────────┼────────────┐        │
│                          ↓            ↓            ↓        │
│                       APB1(/2)    APB2(/1)      CPU/DMA     │
│                       36 MHz      72 MHz       72 MHz       │
│                          │            │                     │
│                     ┌────┴────┐  ┌────┴────┐                │
│                     ↓    ↓    ↓  ↓    ↓    ↓                │
│                   TIM  UART  SPI TIM UART  SPI              │
│                   2~7  2~5  2/3 1/8   1    1                │
│                   72M  36M  36M 72M  72M  72M               │
│                   (×2)                                      │
└─────────────────────────────────────────────────────────────┘
```

---

## 十二、关键概念总结

### 1. 时钟源

4 个时钟源：HSI（内部高速）、HSE（外部高速）、LSI（内部低速）、LSE（外部低速）

### 2. 晶振

晶振 = 产生稳定、精确、固定频率的元器件。比 RC 振荡器精度高。

### 3. SYSCLK

系统时钟，整个芯片的"心跳"，最高 72MHz

### 4. HCLK

AHB 总线时钟，供 CPU、DMA、存储器使用

### 5. PCLK1/PCLK2

APB1/APB2 总线时钟，供外设使用。APB1 最高 36MHz，APB2 最高 72MHz

### 6. 定时器×2规则

当 APB 分频系数≠1 时，定时器时钟自动翻倍，保证精度

### 7. CPU 与外设

CPU 是"指挥官"下命令，外设是"士兵"执行任务，引脚是"数据通道"

---

## 十三、时钟配置速查表

| 参数 | 常用值 | 说明 |
|------|--------|------|
| HSE | 8 MHz | 外部晶振 |
| PLL 倍频 | ×9 | 8 × 9 = 72MHz |
| SYSCLK | 72 MHz | 系统时钟 |
| AHB 分频 | /1 | HCLK = 72MHz |
| APB1 分频 | /2 | PCLK1 = 36MHz |
| APB2 分频 | /1 | PCLK2 = 72MHz |
| FLASH 延迟 | 2WS | 72MHz 需要 2 个等待周期 |

---

## 十四、常用函数/宏速查

| 函数/宏 | 作用 |
|--------|------|
| `HAL_RCC_OscConfig()` | 配置振荡器和 PLL |
| `HAL_RCC_ClockConfig()` | 配置系统时钟和总线分频 |
| `SystemCoreClockUpdate()` | 更新全局变量 SystemCoreClock |
| `__HAL_RCC_xxx_CLK_ENABLE()` | 使能外设时钟 |
| `HAL_RCC_GetSysClockFreq()` | 获取 SYSCLK 频率 |
| `HAL_RCC_GetHCLKFreq()` | 获取 HCLK 频率 |
| `HAL_RCC_GetPCLK1Freq()` | 获取 PCLK1 频率 |
| `HAL_RCC_GetPCLK2Freq()` | 获取 PCLK2 频率 |

---

*学习日期：2026年1月13日*
