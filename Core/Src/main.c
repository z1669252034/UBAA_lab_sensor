/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include <stdio.h>
#include <string.h>
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */
typedef struct {
  int16_t c0,c1;
  int32_t c00,c10;
  int16_t c01,c11,c20,c21,c30;
}SPL06_Calib;
/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
I2C_HandleTypeDef hi2c1;

UART_HandleTypeDef huart2;

/* USER CODE BEGIN PV */
float gx_offset = 0, gy_offset = 0, gz_offset = 0;
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_I2C1_Init(void);
static void MX_USART2_UART_Init(void);
/* USER CODE BEGIN PFP */
uint8_t SPL06_ReadID(void);
uint8_t SPL06_ReadReg(uint8_t reg);
void SPL06_ReadCoefs (uint8_t *buf);
void SPL06_WriteReg(uint8_t reg, uint8_t value);
int32_t SPL06_ReadTemp(void);
int32_t SPL06_ReadPressure(void);
SPL06_Calib SPL06_DecodeCoefs(uint8_t *coef);
float SPL06_CompTemp(int32_t T_raw, SPL06_Calib *c);
float SPL06_CompPressure(int32_t P_raw, int32_t T_raw, SPL06_Calib *c);
void AHT20_SendCmd(uint8_t cmd,uint8_t cmd1,uint8_t cmd2);
void AHT20_Init(void);
void AHT20_Read(float* temp,float* humi);
void BH1745_Read(uint16_t *r,uint16_t *g,uint16_t *b,uint16_t *c);
void BH1745_Init(void);
void BH1745_WriteReg(uint8_t reg, uint8_t value);
void BH1745_ReadBlock (uint8_t reg,uint8_t *buf);
void VL53L0X_SimpleInit(void);
uint16_t VL53L0X_SimpleGetDistance(void);
uint8_t QMI8658_ReadID(void);
void QMI8658_WriteReg(uint8_t reg, uint8_t value);
uint8_t QMI8658_ReadReg(uint8_t reg);
void QMI8658_Init(void);
void QMI8658_ReadAccel(int16_t *ax, int16_t *ay, int16_t *az);
void QMI8658_ReadGyro(int16_t *gx, int16_t *gy, int16_t *gz);
void QMI8658_CalibrateGyro(void);
/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

// ─── UART 输出辅助函数 ──────────────────────────────────
// 发送一个字符串到串口助手（USART2 → ST-Link VCP）
void UART_Print(const char *str)
{
    HAL_UART_Transmit(&huart2, (uint8_t *)str, strlen(str), 100);
}

// 发送一个换行
void UART_Println(const char *str)
{
    UART_Print(str);
    UART_Print("\r\n");
}

uint8_t SPL06_ReadID() {
  if (HAL_I2C_IsDeviceReady(&hi2c1, (uint16_t)(0x77 << 1), 1, 10) == HAL_OK) {
    uint8_t ID;
    if (HAL_I2C_Mem_Read(&hi2c1,(uint16_t)(0x77<<1),0x0D,I2C_MEMADD_SIZE_8BIT,&ID,1,100)==HAL_OK)
    return ID;
    else return 0x00;
  }
  return 0x00;
}

uint8_t SPL06_ReadReg(uint8_t reg) {

    uint8_t data;
    if (HAL_I2C_Mem_Read(&hi2c1,(uint16_t)(0x77<<1),reg,I2C_MEMADD_SIZE_8BIT,&data,1,100)==HAL_OK)
      return data;
    else return 0x00;
}

void SPL06_ReadCoefs (uint8_t *buf) {
  HAL_I2C_Mem_Read(&hi2c1,(uint16_t)(0x77<<1),0X10,I2C_MEMADD_SIZE_8BIT,buf,18,100);
}

void SPL06_WriteReg(uint8_t reg, uint8_t value) {
  HAL_I2C_Mem_Write(&hi2c1,(uint16_t)(0x77<<1),reg,I2C_MEMADD_SIZE_8BIT,&value,1,100);
}

int32_t SPL06_ReadTemp(void) {
    // 命令模式触发一次温度测量
    SPL06_WriteReg(0x08, 0x02);

    // 等待 TMP_RDY (bit 5)
    uint8_t timeout = 0;
    while (((SPL06_ReadReg(0x08) & (1 << 5)) == 0) && timeout < 100) {
        HAL_Delay(2);
        timeout++;
    }

    uint8_t buf[3];
    HAL_I2C_Mem_Read(&hi2c1, (uint16_t)(0x77 << 1), 0x03,
                     I2C_MEMADD_SIZE_8BIT, buf, 3, 100);

    int32_t raw = ((int32_t)buf[0] << 16) | ((int32_t)buf[1] << 8) | buf[2];
    if (raw & (1 << 23)) {
        raw = raw - (1 << 24);
    }
    return raw;
}

int32_t SPL06_ReadPressure(void) {
    // 命令模式触发一次气压测量
    SPL06_WriteReg(0x08, 0x01);

    // 等待 PRS_RDY (bit 4)
    uint8_t timeout = 0;
    while (((SPL06_ReadReg(0x08) & (1 << 4)) == 0) && timeout < 100) {
        HAL_Delay(2);
        timeout++;
    }

    uint8_t buf[3];
    HAL_I2C_Mem_Read(&hi2c1, (uint16_t)(0x77 << 1), 0x00,
                     I2C_MEMADD_SIZE_8BIT, buf, 3, 100);

    int32_t raw = ((int32_t)buf[0] << 16) | ((int32_t)buf[1] << 8) | buf[2];
    if (raw & (1 << 23)) {
        raw = raw - (1 << 24);
    }
    return raw;
}

SPL06_Calib SPL06_DecodeCoefs(uint8_t *coef) {
  SPL06_Calib c;

  // c0: 12-bit signed → coef[0]<<4 | coef[1]>>4
  c.c0 = (int16_t)((coef[0] << 4) | (coef[1] >> 4));
  if (c.c0 & (1 << 11)) c.c0 -= (1 << 12);

  // c1: 12-bit signed → (coef[1]&0x0F)<<8 | coef[2]
  c.c1 = (int16_t)(((coef[1] & 0x0F) << 8) | coef[2]);
  if (c.c1 & (1 << 11)) c.c1 -= (1 << 12);

  // c00: 20-bit signed → coef[3]<<12 | coef[4]<<4 | coef[5]>>4
  c.c00 = (int32_t)((coef[3] << 12) | (coef[4] << 4) | (coef[5] >> 4));
  if (c.c00 & (1 << 19)) c.c00 -= (1 << 20);

  // c10: 20-bit signed → (coef[5]&0x0F)<<16 | coef[6]<<8 | coef[7]
  c.c10 = (int32_t)(((coef[5] & 0x0F) << 16) | (coef[6] << 8) | coef[7]);
  if (c.c10 & (1 << 19)) c.c10 -= (1 << 20);

  // c01: 16-bit signed → coef[8]<<8 | coef[9]
  c.c01 = (int16_t)((coef[8] << 8) | coef[9]);

  // c11: 16-bit signed → coef[10]<<8 | coef[11]
  c.c11 = (int16_t)((coef[10] << 8) | coef[11]);

  // c20: 16-bit signed → coef[12]<<8 | coef[13]
  c.c20 = (int16_t)((coef[12] << 8) | coef[13]);

  // c21: 16-bit signed → coef[14]<<8 | coef[15]
  c.c21 = (int16_t)((coef[14] << 8) | coef[15]);

  // c30: 16-bit signed → coef[16]<<8 | coef[17]
  c.c30 = (int16_t)((coef[16] << 8) | coef[17]);

  return c;
}

float SPL06_CompTemp(int32_t T_raw,SPL06_Calib *c) {
  float kT = 524288.0f;
  float T_sc =(float)T_raw/kT;
  return (float)c->c0*0.5f+ (float)c->c1 * T_sc;
}

float SPL06_CompPressure(int32_t P_raw, int32_t T_raw, SPL06_Calib *c) {
    float kP = 1572864.0f;  // 2x oversampling
    float kT = 524288.0f;   // 1x oversampling
    float P_sc = (float)P_raw / kP;
    float T_sc = (float)T_raw / kT;

    // Pcomp(Pa) = c00 + P_sc*(c10 + P_sc*(c20 + P_sc*c30))
    //           + T_sc*c01 + T_sc*P_sc*(c11 + P_sc*c21)
    float p = (float)c->c00;
    p += P_sc * ((float)c->c10 + P_sc * ((float)c->c20 + P_sc * (float)c->c30));
    p += T_sc * (float)c->c01;
    p += T_sc * P_sc * ((float)c->c11 + P_sc * (float)c->c21);
    return p;  // Pa
}

void AHT20_SendCmd(uint8_t cmd,uint8_t cmd1,uint8_t cmd2) {
  uint8_t buf[3]={cmd,cmd1,cmd2};
  HAL_I2C_Master_Transmit(&hi2c1, (uint16_t)(0x38 << 1), buf,3,100);
}

void AHT20_Init(void) {
  AHT20_SendCmd(0xBE,0x08,0x00);
  HAL_Delay(10);
}

void AHT20_Read(float* temp,float* humi) {
  AHT20_SendCmd(0xAC, 0x33, 0x00);  // 触发测量（不是 0xBE！）
  HAL_Delay(80);
  uint8_t buf[6];
  HAL_I2C_Master_Receive(&hi2c1, (uint16_t)(0x38 << 1), buf, 6, 100);
  uint32_t raw_h = ((uint32_t)buf[1] << 12) | ((uint32_t)buf[2] << 4) | (buf[3] >> 4);
  uint32_t raw_t = ((uint32_t)(buf[3] & 0x0F) << 16) | ((uint32_t)buf[4] << 8) | buf[5];
  *humi = (float)raw_h * 100.0f / 1048576.0f;
  *temp = (float)raw_t * 200.0f / 1048576.0f - 50.0f;
}

void BH1745_ReadBlock (uint8_t reg,uint8_t *buf) {
  HAL_I2C_Mem_Read(&hi2c1,(uint16_t)(0x39<<1),reg,I2C_MEMADD_SIZE_8BIT,buf,8,100);
}

void BH1745_WriteReg(uint8_t reg, uint8_t value) {
  HAL_I2C_Mem_Write(&hi2c1,(uint16_t)(0x39<<1),reg,I2C_MEMADD_SIZE_8BIT,&value,1,100);
}

void BH1745_Init(void) {
  BH1745_WriteReg(0x44,0x02);
  BH1745_WriteReg(0x41,0x00);
  BH1745_WriteReg(0x42,0x10);
}

void BH1745_Read(uint16_t *r,uint16_t *g,uint16_t *b,uint16_t *c) {
  HAL_Delay(160);
  uint8_t buf[8];
  BH1745_ReadBlock(0x50,buf);
  // 小端序：buf[n]=低字节(LSB), buf[n+1]=高字节(MSB)
  *r = ((uint16_t)buf[1] << 8) | buf[0];
  *g = ((uint16_t)buf[3] << 8) | buf[2];
  *b = ((uint16_t)buf[5] << 8) | buf[4];
  *c = ((uint16_t)buf[7] << 8) | buf[6];
}
// ─── QMI8658C IMU ─────────────────────────────────────
// 6 轴惯性测量单元（加速度计 + 陀螺仪），I2C 地址 0x6A
// 和 SPL06/BH1745 一样，有内部寄存器，用 HAL_I2C_Mem_Read/Write

uint8_t QMI8658_ReadID(void) {
    if (HAL_I2C_IsDeviceReady(&hi2c1, (uint16_t)(0x6A << 1), 1, 10) != HAL_OK)
        return 0x00;
    uint8_t id;
    if (HAL_I2C_Mem_Read(&hi2c1, (uint16_t)(0x6A << 1), 0x00,
                         I2C_MEMADD_SIZE_8BIT, &id, 1, 100) == HAL_OK)
        return id;
    return 0x00;
}

void QMI8658_WriteReg(uint8_t reg, uint8_t value) {
    HAL_I2C_Mem_Write(&hi2c1, (uint16_t)(0x6A << 1), reg,
                      I2C_MEMADD_SIZE_8BIT, &value, 1, 100);
}

uint8_t QMI8658_ReadReg(uint8_t reg) {
    uint8_t data = 0x00;
    HAL_I2C_Mem_Read(&hi2c1, (uint16_t)(0x6A << 1), reg,
                     I2C_MEMADD_SIZE_8BIT, &data, 1, 100);
    return data;
}

void QMI8658_Init(void) {
    // 1. CTRL1 (0x02) = 0x60
    //    bit6=1: 地址自动递增（方便 burst read）
    //    bit5=0: 小端序（低地址=低字节，和 BH1745/SPL06 一致）
    QMI8658_WriteReg(0x02, 0x60);

    // 2. CTRL2 (0x03) = 0x03 — 加速度计
    //    [3:0]=0011 → 6DOF 模式 ODR = 896.8Hz
    //    [6:4]=000  → 量程 ±2g（16384 LSB/g）
    QMI8658_WriteReg(0x03, 0x03);

    // 3. CTRL3 (0x04) = 0x33 — 陀螺仪
    //    [3:0]=0011 → ODR = 896.8Hz（和加速度同步）
    //    [6:4]=011  → 量程 ±128dps（256 LSB/dps）
    QMI8658_WriteReg(0x04, 0x33);

    // 4. CTRL7 (0x08) = 0x03
    //    bit0=1 → 加速度计开
    //    bit1=1 → 陀螺仪开（6 轴 IMU 模式）
    QMI8658_WriteReg(0x08, 0x03);

    // 陀螺启动时间 t1 + 3/ODR ≈ 150ms + 3ms
    HAL_Delay(160);
}

void QMI8658_ReadAccel(int16_t *ax, int16_t *ay, int16_t *az) {
    // 轮询 STATUS0 (0x2E) 的 bit0(aDA)，直到新数据就绪
    uint8_t timeout = 0;
    while (((QMI8658_ReadReg(0x2E) & 1) == 0) && timeout < 200) {
        HAL_Delay(1);
        timeout++;
    }

    // 从 0x35 连续读 6 字节：AX_L,AX_H, AY_L,AY_H, AZ_L,AZ_H
    uint8_t buf[6];
    HAL_I2C_Mem_Read(&hi2c1, (uint16_t)(0x6A << 1), 0x35,
                     I2C_MEMADD_SIZE_8BIT, buf, 6, 100);
    // 小端序：低字节在前
    *ax = (int16_t)(((uint16_t)buf[1] << 8) | buf[0]);
    *ay = (int16_t)(((uint16_t)buf[3] << 8) | buf[2]);
    *az = (int16_t)(((uint16_t)buf[5] << 8) | buf[4]);
}

void QMI8658_ReadGyro(int16_t *gx, int16_t *gy, int16_t *gz) {
    // 轮询 STATUS0 (0x2E) 的 bit1(gDA)，直到陀螺新数据就绪
    uint8_t timeout = 0;
    while (((QMI8658_ReadReg(0x2E) & (1 << 1)) == 0) && timeout < 200) {
        HAL_Delay(1);
        timeout++;
    }

    // 从 0x3B 连续读 6 字节：GX_L,GX_H, GY_L,GY_H, GZ_L,GZ_H
    uint8_t buf[6];
    HAL_I2C_Mem_Read(&hi2c1, (uint16_t)(0x6A << 1), 0x3B,
                     I2C_MEMADD_SIZE_8BIT, buf, 6, 100);
    // 小端序：低字节在前
    *gx = (int16_t)(((uint16_t)buf[1] << 8) | buf[0]);
    *gy = (int16_t)(((uint16_t)buf[3] << 8) | buf[2]);
    *gz = (int16_t)(((uint16_t)buf[5] << 8) | buf[4]);
}

// 静止校准：采 100 个样本取平均作为陀螺零偏
// 校准时必须保持传感器静止！
void QMI8658_CalibrateGyro(void) {
    float sum_x = 0, sum_y = 0, sum_z = 0;
    const int N = 100;
    for (int i = 0; i < N; i++) {
        int16_t gx, gy, gz;
        QMI8658_ReadGyro(&gx, &gy, &gz);
        sum_x += gx;
        sum_y += gy;
        sum_z += gz;
        HAL_Delay(10);  // 100 次 × 10ms = 1 秒
    }
    gx_offset = sum_x / N;
    gy_offset = sum_y / N;
    gz_offset = sum_z / N;
}

// ─── I2C 总线扫描 ──────────────────────────────────────
// 遍历 128 个 7 位地址，检查哪些地址有器件应答
// 你的传感器地址：
//   AHT20   = 0x38  (温湿度)
//   BH1745  = 0x39  (颜色)
//   VL53L0X = 0x52  (距离/ToF)
//   SPL06   = 0x77  (气压)
//   QMI8568C = 0x6A 或 0x6B (惯性/地磁, 推测值)
void I2C_BusScan(void)
{
    char msg[64];
    uint8_t found = 0;

    UART_Println("┌──────────────────────────────────┐");
    UART_Println("│     I2C Bus Scanner 开始扫描     │");
    UART_Println("├──────────────────────────────────┤");
    UART_Println("│ Addr │ 状态       │ 传感器       │");
    UART_Println("├──────┼────────────┼──────────────┤");

    for (uint8_t addr = 1; addr < 128; addr++)
    {
        // HAL_I2C_IsDeviceReady: 向地址发一个空写操作，看是否收到 ACK
        // 参数: (句柄, 7位地址左移1, 重试次数, 超时ms)
        if (HAL_I2C_IsDeviceReady(&hi2c1, (uint16_t)(addr << 1), 1, 10) == HAL_OK)
        {
            found++;
            const char *name = "未知";
            switch (addr)
            {
                case 0x38: name = "AHT20 温湿度";  break;
                case 0x39: name = "BH1745 颜色";   break;
                case 0x52: name = "VL53L0X 距离";  break;
                case 0x6A: name = "QMI8658 IMU";   break;
                case 0x6B: name = "QMI8658 IMU";   break;
                case 0x77: name = "SPL06 气压";    break;
            }
            sprintf(msg, "│ 0x%02X │ ✅ 已找到  │ %-12s │", addr, name);
            UART_Println(msg);
        }
    }

    UART_Println("├──────┴────────────┴──────────────┤");
    if (found == 0)
    {
        UART_Println("│ ⚠️  未找到任何器件！检查接线  │");
    }
    else
    {
        sprintf(msg, "│ 扫描完成，共发现 %d 个器件               │", found);
        UART_Println(msg);
    }
    UART_Println("└──────────────────────────────────┘");
}
/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{

  /* USER CODE BEGIN 1 */

  // 在 HAL 初始化之前就点亮 LED，确认程序进入了 main
  // 如果 LED 亮了 = main() 被调用了
  RCC->AHB2ENR |= RCC_AHB2ENR_GPIOAEN;
  GPIOA->MODER &= ~GPIO_MODER_MODE5_Msk;
  GPIOA->MODER |= (1 << GPIO_MODER_MODE5_Pos);
  GPIOA->BSRR = GPIO_BSRR_BS5;   // LD2 ON
  for (volatile int i = 0; i < 1000000; i++);  // 延时等待
  GPIOA->BSRR = GPIO_BSRR_BR5;   // LD2 OFF

  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/

  HAL_Init();
  SystemClock_Config();
  MX_GPIO_Init();
  MX_I2C1_Init();
  MX_USART2_UART_Init();
  AHT20_Init();
  BH1745_Init();
  QMI8658_Init();
  UART_Println("Gyro calibrating... keep sensor still");
  QMI8658_CalibrateGyro();
  UART_Println("Gyro calibration done!");
  UART_Println("VL53L0X initializing...");
  VL53L0X_SimpleInit();
  UART_Println("VL53L0X ready!");
  /* USER CODE BEGIN 2 */

  // ─── SPL06 校准系数 ────────────────────────────
  uint8_t coef[18];
  SPL06_ReadCoefs(coef);
  SPL06_Calib cal = SPL06_DecodeCoefs(coef);

  // 配置传感器：温度+气压
  SPL06_WriteReg(0x07, 0x80);  // TMP_CFG: 外部传感器(MEMS), 1x
  SPL06_WriteReg(0x06, 0x01);  // PRS_CFG: 2x oversampling
  HAL_Delay(10);

  UART_Println("========== Sensor Data ==========");
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_5, GPIO_PIN_SET);
    HAL_Delay(20);

    int32_t T_raw = SPL06_ReadTemp();
    int32_t P_raw = SPL06_ReadPressure();
    float temp_c = SPL06_CompTemp(T_raw, &cal);
    float press_pa = SPL06_CompPressure(P_raw, T_raw, &cal);

    char msg[128];
    sprintf(msg, "SPL06   | Temp: %.1f C  |  Press: %.1f hPa",
            (double)temp_c, (double)(press_pa / 100.0f));
    UART_Println(msg);

    char msg1[128];
    float t, h;
    AHT20_Read(&t, &h);
    sprintf(msg1, "AHT20   | Temp: %.1f C  |  Humi: %.1f %%", t, h);
    UART_Println(msg1);

    char msg2[128];
    uint16_t rgb[4];
    BH1745_Read(&rgb[0], &rgb[1], &rgb[2], &rgb[3]);
    sprintf(msg2, "BH1745  | R:%5u  G:%5u  B:%5u  C:%5u",
            rgb[0], rgb[1], rgb[2], rgb[3]);
    UART_Println(msg2);

    char msg3[64];
    uint16_t dist = VL53L0X_SimpleGetDistance();
    if (dist != 0xFFFF)
        sprintf(msg3, "VL53L0X | 距离: %u mm", dist);
    else
        sprintf(msg3, "VL53L0X | out of range");
    UART_Println(msg3);

    char msg4[128];
    int16_t ax, ay, az;
    QMI8658_ReadAccel(&ax, &ay, &az);
    sprintf(msg4, "IMU Acc | X:%+.3fg  Y:%+.3fg  Z:%+.3fg",
            ax / 16384.0f, ay / 16384.0f, az / 16384.0f);
    UART_Println(msg4);

    char msg5[128];
    int16_t gx, gy, gz;
    QMI8658_ReadGyro(&gx, &gy, &gz);
    float gx_dps = (gx - gx_offset) / 256.0f;
    float gy_dps = (gy - gy_offset) / 256.0f;
    float gz_dps = (gz - gz_offset) / 256.0f;
    sprintf(msg5, "IMU Gyr | X:%+.2fdps  Y:%+.2fdps  Z:%+.2fdps",
            gx_dps, gy_dps, gz_dps);
    UART_Println(msg5);

    UART_Println("-----------------------------------");

    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_5, GPIO_PIN_RESET);
    HAL_Delay(2000);
  }
  /* USER CODE END 3 */
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  /** Configure the main internal regulator output voltage
  */
  HAL_PWREx_ControlVoltageScaling(PWR_REGULATOR_VOLTAGE_SCALE1);

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_NONE;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_HSI;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_0) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief I2C1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_I2C1_Init(void)
{

  /* USER CODE BEGIN I2C1_Init 0 */

  /* USER CODE END I2C1_Init 0 */

  /* USER CODE BEGIN I2C1_Init 1 */

  /* USER CODE END I2C1_Init 1 */
  hi2c1.Instance = I2C1;
  hi2c1.Init.Timing = 0x00503D58;
  hi2c1.Init.OwnAddress1 = 0;
  hi2c1.Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
  hi2c1.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
  hi2c1.Init.OwnAddress2 = 0;
  hi2c1.Init.OwnAddress2Masks = I2C_OA2_NOMASK;
  hi2c1.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
  hi2c1.Init.NoStretchMode = I2C_NOSTRETCH_DISABLE;
  if (HAL_I2C_Init(&hi2c1) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure Analogue filter
  */
  if (HAL_I2CEx_ConfigAnalogFilter(&hi2c1, I2C_ANALOGFILTER_ENABLE) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure Digital filter
  */
  if (HAL_I2CEx_ConfigDigitalFilter(&hi2c1, 0) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN I2C1_Init 2 */

  /* USER CODE END I2C1_Init 2 */

}

/**
  * @brief USART2 Initialization Function
  * @param None
  * @retval None
  */
static void MX_USART2_UART_Init(void)
{

  /* USER CODE BEGIN USART2_Init 0 */

  /* USER CODE END USART2_Init 0 */

  /* USER CODE BEGIN USART2_Init 1 */

  /* USER CODE END USART2_Init 1 */
  huart2.Instance = USART2;
  huart2.Init.BaudRate = 115200;
  huart2.Init.WordLength = UART_WORDLENGTH_8B;
  huart2.Init.StopBits = UART_STOPBITS_1;
  huart2.Init.Parity = UART_PARITY_NONE;
  huart2.Init.Mode = UART_MODE_TX_RX;
  huart2.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart2.Init.OverSampling = UART_OVERSAMPLING_16;
  huart2.Init.OneBitSampling = UART_ONE_BIT_SAMPLE_DISABLE;
  huart2.Init.ClockPrescaler = UART_PRESCALER_DIV1;
  huart2.AdvancedInit.AdvFeatureInit = UART_ADVFEATURE_NO_INIT;
  if (HAL_UART_Init(&huart2) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_UARTEx_SetTxFifoThreshold(&huart2, UART_TXFIFO_THRESHOLD_1_8) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_UARTEx_SetRxFifoThreshold(&huart2, UART_RXFIFO_THRESHOLD_1_8) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_UARTEx_DisableFifoMode(&huart2) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN USART2_Init 2 */

  /* USER CODE END USART2_Init 2 */

}

/**
  * @brief GPIO Initialization Function
  * @param None
  * @retval None
  */
static void MX_GPIO_Init(void)
{
  /* USER CODE BEGIN MX_GPIO_Init_1 */

  /* USER CODE END MX_GPIO_Init_1 */

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();

  /* USER CODE BEGIN MX_GPIO_Init_2 */

  // 初始化 LD2 (PA5) 作为输出，用来指示程序状态
  GPIO_InitTypeDef GPIO_InitStruct = {0};
  GPIO_InitStruct.Pin = GPIO_PIN_5;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  /* USER CODE END MX_GPIO_Init_2 */
}

/* USER CODE BEGIN 4 */

/* USER CODE END 4 */

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
  __disable_irq();
  while (1)
  {
  }
  /* USER CODE END Error_Handler_Debug */
}
#ifdef USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
