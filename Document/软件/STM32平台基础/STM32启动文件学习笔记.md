# STM32 启动文件学习笔记

## 一、启动文件概述

启动文件 `startup_stm32f103xe.s` 是系统上电后最先执行的代码，负责：
- 设置栈空间和堆空间
- 定义向量表（中断入口地址）
- 执行 Reset_Handler（复位处理函数）
- 调用 SystemInit 和 __main，最终进入用户的 main()

---

## 二、栈空间配置

```assembly
Stack_Size      EQU     0x00000400              ; 定义常量，栈大小 = 1KB

                AREA    STACK, NOINIT, READWRITE, ALIGN=3
Stack_Mem       SPACE   Stack_Size              ; 分配栈空间
__initial_sp                                    ; 栈顶指针标签
```

### 关键点：
| 指令 | 含义 |
|------|------|
| `EQU` | 定义常量（类似 #define） |
| `AREA` | 声明内存段，定义属性 |
| `NOINIT` | 不初始化 |
| `READWRITE` | 可读写，放在 RAM |
| `ALIGN=3` | 2³=8 字节对齐 |
| `SPACE` | 分配指定大小的空间 |

### 内存布局：
```
高地址 ┌─────────────────┐ ← __initial_sp (栈顶)
       │   栈空间        │  ↑ 栈向下生长
       │  (1024 字节)    │
低地址 └─────────────────┘ ← Stack_Mem (栈底)
```

---

## 三、堆空间配置

```assembly
Heap_Size       EQU     0x00000200              ; 堆大小 = 512 字节

                AREA    HEAP, NOINIT, READWRITE, ALIGN=3
__heap_base                                     ; 堆起始地址
Heap_Mem        SPACE   Heap_Size
__heap_limit                                    ; 堆结束地址
```

### 堆 vs 栈：
| 对比项 | 栈（Stack） | 堆（Heap） |
|--------|------------|-----------|
| 大小 | 0x400 (1KB) | 0x200 (512字节) |
| 生长方向 | 向下生长 ↓ | 向上生长 ↑ |
| 用途 | 局部变量、函数调用 | malloc() 动态分配 |

---

## 四、编译器指令

```assembly
                PRESERVE8
                THUMB
```

| 指令 | 作用 |
|------|------|
| `PRESERVE8` | 声明本文件保证栈8字节对齐（告诉链接器） |
| `THUMB` | 告诉汇编器使用 Thumb 指令集（Cortex-M3 必须） |

### PRESERVE8 详解：
- 不是让代码对齐，是声明"运行时栈指针SP保持8字节对齐"
- 是给链接器的承诺，链接器会检查
- 不写会有警告，提醒你检查代码是否真的对齐

### THUMB 详解：
- Cortex-M3 只支持 Thumb/Thumb-2 指令集
- 不写可能生成错误的指令格式
- 只影响本汇编文件，不影响 C 代码

---

## 五、向量表

```assembly
                AREA    RESET, DATA, READONLY
                EXPORT  __Vectors
                EXPORT  __Vectors_End
                EXPORT  __Vectors_Size

__Vectors       DCD     __initial_sp            ; 栈顶指针
                DCD     Reset_Handler           ; 复位处理函数
                DCD     NMI_Handler             ; NMI 中断
                DCD     HardFault_Handler       ; 硬件错误
                ...
                DCD     USART1_IRQHandler       ; 外设中断
                ...
__Vectors_End

__Vectors_Size  EQU     __Vectors_End - __Vectors
```

### 关键点：
- `AREA RESET, DATA, READONLY` → 只读数据段，放在 Flash 最前面
- `DCD` = Define Constant Data，定义 32 位数据
- 向量表存的是**函数地址**（指针）
- CPU 复位时从地址 0 读取 SP 和 PC
- `EXPORT` 导出符号，让其他文件可以使用

### 向量表作用：
```
中断发生 → CPU 查向量表 → 找到处理函数地址 → 跳转执行
```

### 向量表在 Flash 中的位置：
由链接脚本决定，`*.o (RESET, +First)` 表示放在最前面

---

## 六、Reset_Handler

```assembly
Reset_Handler   PROC
                EXPORT  Reset_Handler             [WEAK]
                IMPORT  __main
                IMPORT  SystemInit
                LDR     R0, =SystemInit
                BLX     R0                        ; 调用 SystemInit()
                LDR     R0, =__main
                BX      R0                        ; 跳转到 __main（不返回）
                ENDP
```

### 执行流程：
```
CPU 复位 → Reset_Handler → SystemInit() → __main → main()
```

### 指令含义：
| 指令 | 含义 |
|------|------|
| `PROC/ENDP` | 函数开始/结束标记 |
| `EXPORT [WEAK]` | 导出符号，弱定义可被覆盖 |
| `IMPORT` | 导入外部符号 |
| `LDR R0, =xxx` | 把 xxx 的地址放到 R0 |
| `BLX R0` | 调用函数（会返回） |
| `BX R0` | 跳转（不返回） |

### BLX vs BX：
| 指令 | 含义 | 会返回吗？ |
|------|------|-----------|
| `BLX` | 调用函数，保存返回地址到 LR | ✅ 会返回 |
| `BX` | 跳转，不保存返回地址 | ❌ 不返回 |

---

## 七、默认中断处理函数

```assembly
NMI_Handler     PROC
                EXPORT  NMI_Handler                [WEAK]
                B       .                          ; 死循环
                ENDP
```

### 关键点：
- `B .` = 跳转到当前位置 = 死循环
- `[WEAK]` = 弱定义，可以被用户的同名函数覆盖
- 如果开启中断但没写处理函数 → 执行死循环 → 程序卡住

### Default_Handler（共享默认实现）：
```assembly
Default_Handler PROC
                EXPORT  USART1_IRQHandler   [WEAK]
                EXPORT  TIM2_IRQHandler     [WEAK]
                ...
USART1_IRQHandler                           ; 多个标签
TIM2_IRQHandler                             ; 指向同一地址
...
                B       .                   ; 共用一个死循环
                ENDP
```

### 设计目的：
- 节省空间（几十个标签共用一条指令）
- 提供默认实现（死循环，方便发现问题）
- 允许用户覆盖（[WEAK] 弱定义）

---

## 八、标签的概念

### 什么是标签？
**标签 = 给内存地址起名字**

```assembly
NMI_Handler     PROC      ; 标签，标记这个位置的地址
                B       .
                ENDP
```

### 关键理解：
- 写标签时不知道具体地址
- 链接器后面分配具体地址
- 向量表通过标签名引用函数地址
- 标签本身不占空间，只是位置标记

### 标签 vs PROC：
| 东西 | 作用 | 标记地址吗？ |
|------|------|-------------|
| **标签** | 给地址起名字 | ✅ 是 |
| **PROC** | 告诉汇编器"函数开始" | ❌ 不是 |

---

## 九、栈/堆初始化

```assembly
                 IF      :DEF:__MICROLIB
                 EXPORT  __initial_sp
                 EXPORT  __heap_base
                 EXPORT  __heap_limit
                 ELSE
                 IMPORT  __use_two_region_memory
                 EXPORT  __user_initial_stackheap
__user_initial_stackheap
                 LDR     R0, = Heap_Mem           ; 堆起始
                 LDR     R1, =(Stack_Mem + Stack_Size) ; 栈顶
                 LDR     R2, = (Heap_Mem + Heap_Size)  ; 堆结束
                 LDR     R3, = Stack_Mem          ; 栈底
                 BX      LR
                 ENDIF
                 END
```

### 作用：
告诉 C 库栈和堆在哪里，让 malloc() 等函数知道可用空间

### MicroLIB vs 标准库：
| 对比 | MicroLIB | 标准库 |
|------|----------|--------|
| 来源 | Keil（ARM） | ARM 官方 |
| 大小 | 小（精简） | 大（完整） |
| 初始化方式 | 直接用符号 | 调用函数 |
| 常用场景 | 嵌入式（推荐） | 需要完整功能 |

### Keil 设置：
```
Project → Options → Target → ☑ Use MicroLIB
```

---

## 十、启动流程总结

```
┌─────────────────────────────────────────────────────────────┐
│                        上电/复位                            │
└─────────────────────────┬───────────────────────────────────┘
                          ▼
┌─────────────────────────────────────────────────────────────┐
│  硬件从地址 0x00000000 读取 __initial_sp → 加载到 SP        │
│  硬件从地址 0x00000004 读取 Reset_Handler 地址 → 跳转执行   │
└─────────────────────────┬───────────────────────────────────┘
                          ▼
┌─────────────────────────────────────────────────────────────┐
│  Reset_Handler:                                             │
│    调用 SystemInit() → 初始化系统时钟                       │
│    跳转 __main → C 库初始化 .data/.bss                      │
└─────────────────────────┬───────────────────────────────────┘
                          ▼
┌─────────────────────────────────────────────────────────────┐
│  main() → 你的代码开始执行                                  │
└─────────────────────────────────────────────────────────────┘
```

---

## 十一、内存布局总览

```
Flash (0x08000000)                 RAM (0x20000000)
┌─────────────────┐               ┌─────────────────┐
│  RESET (向量表)  │               │  .data (已初始化)│
├─────────────────┤               ├─────────────────┤
│  .text (代码)   │               │  .bss (未初始化) │
│                 │               ├─────────────────┤
│  只读数据       │               │  HEAP (堆)      │
│                 │               ├─────────────────┤
│                 │               │  STACK (栈)     │
└─────────────────┘               └─────────────────┘
     READONLY                         READWRITE
```

---

## 十二、实用价值

| 场景 | 需要的知识 |
|------|-----------|
| HardFault 调试 | 向量表、异常处理、栈回溯 |
| OTA / Bootloader | 向量表重定向、跳转机制 |
| 调整栈/堆大小 | 修改 Stack_Size、Heap_Size |
| 自定义中断函数 | 覆盖 [WEAK] 弱定义 |
| RTOS 移植 | PendSV、SVC、栈管理 |

---

## 十三、常用汇编指令速查

| 指令 | 含义 |
|------|------|
| `EQU` | 定义常量 |
| `AREA` | 声明内存段 |
| `SPACE` | 分配空间 |
| `DCD` | 定义 32 位数据 |
| `PROC/ENDP` | 函数开始/结束 |
| `EXPORT` | 导出符号 |
| `IMPORT` | 导入符号 |
| `[WEAK]` | 弱定义 |
| `LDR` | 加载数据到寄存器 |
| `BLX` | 调用函数（会返回） |
| `BX` | 跳转（不返回） |
| `B .` | 死循环 |
| `ALIGN` | 对齐 |
| `PRESERVE8` | 声明栈 8 字节对齐 |
| `THUMB` | 使用 Thumb 指令集 |
| `IF/ELSE/ENDIF` | 条件编译 |
| `END` | 文件结束 |

---

## 十四、关键概念总结

### 1. 启动文件的作用
启动文件是"地基"，帮你把系统环境搭好，然后把控制权交给你的 main()

### 2. 向量表
向量表 = 函数入口地址的目录，告诉 CPU 中断发生时去哪里执行代码

### 3. [WEAK] 弱定义
弱定义 = 可被覆盖的默认实现，你写了同名函数就用你的，没写就用默认的（死循环）

### 4. 标签
标签 = 地址的名字，具体地址由链接器分配

### 5. AREA 和 SPACE
AREA = 定义段的属性（名字、读写、对齐等）
SPACE = 在段内实际分配空间

---

*学习日期：2026年1月12日*
