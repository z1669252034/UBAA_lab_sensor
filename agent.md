# Agent Context — I2C 传感器板调试项目

## 项目概述
5 个传感器全部驱动完成，使用 STM32G474RE（NUCLEO-G474RE 开发板）。

## 硬件

| 传感器 | 芯片 | I2C 7-bit 地址 | 功能 | 状态 |
|--------|------|:---:|------|:---:|
| 气压 | SPL06-001 | **0x77** | 气压 + 温度 | ✅ |
| 温湿度 | AHT20 | **0x38** | 温度 + 湿度 | ✅ |
| 颜色 | BH1745 | **0x39** | RGB 颜色传感器 | ✅ |
| IMU | QMI8658C | **0x6A** | 6 轴（加速度+陀螺） | ✅ |
| ToF 距离 | VL53L0X | **0x29** | 激光测距 | ✅ |
| 未知器件 | ? | **0x7E** | 待确认 | — |

- NUCLEO-G474RE 开发板，板载 ST-Link V3
- 引脚：I2C1 → PB8(SCL/D15), PB9(SDA/D14), 3V3, GND
- USART2 → PA2(TX), PA3(RX) 通过 ST-Link VCP → COM10
- 新板 SN: 001700323756501220303658

## 工程信息
- 路径：`D:\STM\IIC_test`
- 工具：CubeMX 生成 CMake 工程，CLion 编辑
- 编译器：`D:\STMCLT\STM32CubeCLT_1.22.0\GNU-tools-for-STM32\bin\arm-none-eabi-gcc.exe`
- 烧录：STM32CubeProgrammer CLI，端口 SWD
- 串口：COM10，115200 波特率
- 注意：串口助手需要设为 ASCII/文本模式，UTF-8 中文会乱码，全部输出已改为英文

## 关键问题与解决

### 已解决 ✅
1. **CMake 找不到编译器** → 修改 `cmake/gcc-arm-none-eabi.cmake`，把 `TOOLCHAIN_PREFIX` 改为完整路径并加 `.exe` 后缀
2. **第一块板子 Target Voltage 异常 (0.6V)** → 换板
3. **BOOT0 冲突** → PB8 既是 I2C SCL 又是 BOOT0。传感器板上拉电阻把 PB8 拉高 → 芯片从 System Memory 启动。设置 Option Bytes 强制从 Flash 启动
4. **Option Bytes 正确设置**：`STM32_Programmer_CLI -c port=SWD mode=UR -ob "nSWBOOT0=0" "nBOOT0=1" "nBOOT1=1"`
   - `nSWBOOT0=0`：忽略 PB8 引脚电平，由 Option Byte 决定 BOOT0
   - `nBOOT0=1`：BOOT0=0 → 强制从 Flash 启动
5. **I2C 无上拉** → 修改 MSP 文件 PB8/PB9 从 `GPIO_NOPULL` 改为 `GPIO_PULLUP`
6. **按复位键无法启动** → ST-Link V3 干预，断电重启正常
7. **`sprintf` 不支持 `%f`** → CMakeLists.txt 加 `target_link_options(... PRIVATE -u _printf_float)`
8. **串口助手显示乱码/HEX** → 串口助手需设为 ASCII/文本模式

## 常用命令

### 编译
```bash
"D:/STMCLT/STM32CubeCLT_1.22.0/CMake/bin/cmake.exe" --build "/d/STM/IIC_test/build/Debug" --target IIC_test -j
```

### 烧录
```bash
"D:/STMCLT/STM32CubeCLT_1.22.0/STM32CubeProgrammer/bin/STM32_Programmer_CLI.exe" -c port=SWD -w "/d/STM/IIC_test/build/Debug/IIC_test.elf" -rst
```

### 读取串口
```bash
python -c "
import serial, time
ser = serial.Serial('COM10', 115200, timeout=8)
time.sleep(0.5)
data = b''
t0 = time.time()
while time.time() - t0 < 7:
    chunk = ser.read(1024)
    if chunk: data += chunk
    else: time.sleep(0.2)
ser.close()
if data.strip(): print(data.decode('utf-8', errors='replace'))
else: print('NO DATA')
"
```

## 当前进度
- ✅ **全部 5 个传感器驱动完成**
- ✅ 串口输出为人类可读的英文格式

---

## 传感器知识体系总结

用户通过这 5 个传感器学完了嵌入式 I2C 驱动的三种模式：

| 类型 | 传感器 | 通信方式 | 难度 |
|:---|:---|:---|:---:|
| 寄存器式 | SPL06、BH1745、QMI8658C | `HAL_I2C_Mem_Read/Write` | 中等 |
| 裸 I2C 协议式 | AHT20 | `HAL_I2C_Master_Transmit/Receive` | 简单 |
| 固件+API 式 | VL53L0X | ST API + 平台适配层 | 较高 |

### 第三方库集成通用模式
1. 复制库的源文件和头文件进项目
2. 实现平台层（I2C 读写 + 延时函数）——库通过头文件声明函数，你的 `.c` 文件提供实现
3. 编译时链接：库调用的空壳函数在链接阶段找到你的实现
4. 按库的文档顺序调 API 函数，组合出功能

---

## SPL06-001 气压传感器 ✅

- I2C 地址 0x77，有内部寄存器
- 24-bit 补码数据 + nibble-split 校准系数 + 补偿公式
- 关键寄存器：0x06(PRS_CFG), 0x07(TMP_CFG), 0x08(MEAS_CFG), 0x10-0x21(COEF)
- **踩坑**：TMP_EXT 必须设 1 用外部传感器；single mode 在背景模式不工作；BOOT0 冲突

---

## AHT20 温湿度传感器 ✅

- I2C 地址 0x38，**无内部寄存器**，用裸 I2C 读写
- 流程：发 0xAC 触发 → 等 80ms → 读 6 字节 → 拆 20-bit 解算
- **踩坑**：触发命令写成 0xBE；raw 值声明为 uint8_t 被截断

---

## BH1745 颜色传感器 ✅

- I2C 地址 0x39，ROHM 16-bit RGB 颜色传感器
- 类似 SPL06：有寄存器，用 `Mem_Read/Write`
- 关键寄存器：0x40(SYSTEM_CONTROL), 0x41(MODE_CONTROL1), 0x42(MODE_CONTROL2), 0x44(MODE_CONTROL3)
- 数据寄存器：0x50-0x57(R/G/B/C 各 16-bit，小端序)
- 配置：MODE_CONTROL3=0x02, MODE_CONTROL2=0x10(使能), 测量时间 160ms
- **踩坑**：字节序反了——低地址=低字节，高地址=高字节

---

## QMI8658C IMU ✅

- I2C 地址 0x6A（SA0=1），QST 6 轴 IMU
- 加速度 ±2g（16384 LSB/g），陀螺 ±128dps（256 LSB/dps）
- 关键寄存器：0x00(WHO_AM_I=0x05), 0x02(CTRL1), 0x03(CTRL2), 0x04(CTRL3), 0x07(CTRL7), 0x2E(STATUS0)
- 数据：0x35-0x3A(Accel XYZ), 0x3B-0x40(Gyro XYZ), 0x33-0x34(Temp)
- **陀螺零偏**：静止时陀螺有 ±10dps 固有偏移，需上电时采 100 样本做校准
- **踩坑**：陀螺读数大（6-7dps）误以为出 bug，实为 MEMS 正常零偏

---

## VL53L0X ToF 激光测距 ✅

- I2C 地址 0x29（7-bit，8-bit=0x52），ST 飞行时间传感器
- **与所有其他传感器的本质区别**：内部有 MCU 运行固件，不公开寄存器 map
- 必须通过 ST 官方 API 操作，不能"写寄存器→读数据"
- 驱动模式：`main.c → Simple API → ST API → 平台层(HAL) → I2C 硬件`
- 集成步骤：
  1. 复制 ST API 源文件（`vl53l0x_api.c` 等 5 个 `.c` + 14 个 `.h`）到项目
  2. 实现平台层 `vl53l0x_hal.c`：把 `VL53L0X_WrByte` 等函数接到 `HAL_I2C_Mem_Write`
  3. 封装简化 API：`VL53L0X_SimpleInit()` + `VL53L0X_SimpleGetDistance()`
- ST API 通过头文件声明函数（`vl53l0x_platform.h`），编译时只检查类型，链接时才找到你的实现
- **踩坑**：`PerformRefCalibration` 需要传入 `uint8_t*` 参数不能传 NULL；中间层 `VL53L0X_write_byte` 可以省略直接从 `WrByte` 调 HAL

---

## 文件结构
```
D:\STM\IIC_test\
├── Core/Src/main.c              ← 主程序 + 所有传感器驱动函数
├── Core/Src/vl53l0x_hal.c       ← VL53L0X 平台适配层 + 简化 API
├── Core/Src/vl53l0x_api*.c      ← ST 官方 VL53L0X API（5个文件）
├── Core/Inc/vl53l0x_*.h         ← VL53L0X 头文件（14个）
├── cmake/stm32cubemx/CMakeLists.txt ← 源文件列表 + 头文件路径
├── cmake/gcc-arm-none-eabi.cmake ← 工具链路径
├── CMakeLists.txt               ← -u _printf_float
桌面\
├── agent.md                     ← 本文件（项目记忆）
├── IIC训练/2dbd6-main/           ← VL53L0X 参考例程（WMD 移植版）
├── SPL06.pdf                    ← SPL06 数据手册
├── BH1745数据手册.pdf            ← BH1745 数据手册
├── vl53l0x.pdf                  ← VL53L0X datasheet (ST官方)
├── QMI8658C.pdf                 ← QMI8658C 数据手册 (88页)
└── VL53L0X用户手册.pdf           ← VL53L0X 模块使用手册(YFROBOT)
```
