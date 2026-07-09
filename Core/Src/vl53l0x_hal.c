/**
 * @file   vl53l0x_hal.c
 * @brief  VL53L0X 平台适配层 —— 桥接 ST API 和 STM32 HAL 库
 *
 * 同时实现 vl53l0x_platform.h（上层接口）和 vl53l0x_i2c_platform.h（底层 I2C）
 * VL53L0X 是大端序（Big-Endian）：多字节传输时 MSB 在前。
 */

#include "main.h"
#include "vl53l0x_api.h"
#include "vl53l0x_platform.h"
#include "vl53l0x_i2c_platform.h"

extern I2C_HandleTypeDef hi2c1;

/* ================================================================
 *  底层 I2C 函数 —— vl53l0x_i2c_platform.h 定义，返回 int32_t
 * ================================================================ */

int32_t VL53L0X_write_multi(uint8_t address, uint8_t index,
                             uint8_t *pdata, int32_t count) {
    if (HAL_I2C_Mem_Write(&hi2c1, (uint16_t)address, index,
                          I2C_MEMADD_SIZE_8BIT, pdata, count, 100) == HAL_OK)
        return 0;
    return -1;
}

int32_t VL53L0X_read_multi(uint8_t address, uint8_t index,
                            uint8_t *pdata, int32_t count) {
    if (HAL_I2C_Mem_Read(&hi2c1, (uint16_t)address, index,
                         I2C_MEMADD_SIZE_8BIT, pdata, count, 100) == HAL_OK)
        return 0;
    return -1;
}

int32_t VL53L0X_write_byte(uint8_t address, uint8_t index, uint8_t data) {
    return VL53L0X_write_multi(address, index, &data, 1);
}

int32_t VL53L0X_read_byte(uint8_t address, uint8_t index, uint8_t *pdata) {
    return VL53L0X_read_multi(address, index, pdata, 1);
}

int32_t VL53L0X_write_word(uint8_t address, uint8_t index, uint16_t data) {
    uint8_t buf[2];
    buf[0] = (uint8_t)(data >> 8);
    buf[1] = (uint8_t)(data & 0xFF);
    return VL53L0X_write_multi(address, index, buf, 2);
}

int32_t VL53L0X_read_word(uint8_t address, uint8_t index, uint16_t *pdata) {
    uint8_t buf[2];
    int32_t ret = VL53L0X_read_multi(address, index, buf, 2);
    *pdata = ((uint16_t)buf[0] << 8) | buf[1];
    return ret;
}

int32_t VL53L0X_write_dword(uint8_t address, uint8_t index, uint32_t data) {
    uint8_t buf[4];
    buf[0] = (uint8_t)(data >> 24);
    buf[1] = (uint8_t)(data >> 16);
    buf[2] = (uint8_t)(data >> 8);
    buf[3] = (uint8_t)(data & 0xFF);
    return VL53L0X_write_multi(address, index, buf, 4);
}

int32_t VL53L0X_read_dword(uint8_t address, uint8_t index, uint32_t *pdata) {
    uint8_t buf[4];
    int32_t ret = VL53L0X_read_multi(address, index, buf, 4);
    *pdata = ((uint32_t)buf[0] << 24) | ((uint32_t)buf[1] << 16) |
             ((uint32_t)buf[2] << 8)  | (uint32_t)buf[3];
    return ret;
}

/* ================================================================
 *  上层平台函数 —— vl53l0x_platform.h 定义，返回 VL53L0X_Error
 *  这些是 ST API 直接调用的接口
 * ================================================================ */

VL53L0X_Error VL53L0X_LockSequenceAccess(VL53L0X_DEV Dev) {
    (void)Dev;
    return VL53L0X_ERROR_NONE;
}

VL53L0X_Error VL53L0X_UnlockSequenceAccess(VL53L0X_DEV Dev) {
    (void)Dev;
    return VL53L0X_ERROR_NONE;
}

VL53L0X_Error VL53L0X_WriteMulti(VL53L0X_DEV Dev, uint8_t index,
                                  uint8_t *pdata, uint32_t count) {
    if (VL53L0X_write_multi(Dev->I2cDevAddr, index, pdata, count) == 0)
        return VL53L0X_ERROR_NONE;
    return VL53L0X_ERROR_CONTROL_INTERFACE;
}

VL53L0X_Error VL53L0X_ReadMulti(VL53L0X_DEV Dev, uint8_t index,
                                 uint8_t *pdata, uint32_t count) {
    if (VL53L0X_read_multi(Dev->I2cDevAddr, index, pdata, count) == 0)
        return VL53L0X_ERROR_NONE;
    return VL53L0X_ERROR_CONTROL_INTERFACE;
}

VL53L0X_Error VL53L0X_WrByte(VL53L0X_DEV Dev, uint8_t index, uint8_t data) {
    if (VL53L0X_write_byte(Dev->I2cDevAddr, index, data) == 0)
        return VL53L0X_ERROR_NONE;
    return VL53L0X_ERROR_CONTROL_INTERFACE;
}

VL53L0X_Error VL53L0X_WrWord(VL53L0X_DEV Dev, uint8_t index, uint16_t data) {
    if (VL53L0X_write_word(Dev->I2cDevAddr, index, data) == 0)
        return VL53L0X_ERROR_NONE;
    return VL53L0X_ERROR_CONTROL_INTERFACE;
}

VL53L0X_Error VL53L0X_WrDWord(VL53L0X_DEV Dev, uint8_t index, uint32_t data) {
    if (VL53L0X_write_dword(Dev->I2cDevAddr, index, data) == 0)
        return VL53L0X_ERROR_NONE;
    return VL53L0X_ERROR_CONTROL_INTERFACE;
}

VL53L0X_Error VL53L0X_RdByte(VL53L0X_DEV Dev, uint8_t index, uint8_t *data) {
    if (VL53L0X_read_byte(Dev->I2cDevAddr, index, data) == 0)
        return VL53L0X_ERROR_NONE;
    return VL53L0X_ERROR_CONTROL_INTERFACE;
}

VL53L0X_Error VL53L0X_RdWord(VL53L0X_DEV Dev, uint8_t index, uint16_t *data) {
    if (VL53L0X_read_word(Dev->I2cDevAddr, index, data) == 0)
        return VL53L0X_ERROR_NONE;
    return VL53L0X_ERROR_CONTROL_INTERFACE;
}

VL53L0X_Error VL53L0X_RdDWord(VL53L0X_DEV Dev, uint8_t index, uint32_t *data) {
    if (VL53L0X_read_dword(Dev->I2cDevAddr, index, data) == 0)
        return VL53L0X_ERROR_NONE;
    return VL53L0X_ERROR_CONTROL_INTERFACE;
}

VL53L0X_Error VL53L0X_UpdateByte(VL53L0X_DEV Dev, uint8_t index,
                                  uint8_t AndData, uint8_t OrData) {
    uint8_t data;
    int32_t ret = VL53L0X_read_byte(Dev->I2cDevAddr, index, &data);
    if (ret != 0) return VL53L0X_ERROR_CONTROL_INTERFACE;
    data = (data & AndData) | OrData;
    ret = VL53L0X_write_byte(Dev->I2cDevAddr, index, data);
    if (ret != 0) return VL53L0X_ERROR_CONTROL_INTERFACE;
    return VL53L0X_ERROR_NONE;
}

VL53L0X_Error VL53L0X_PollingDelay(VL53L0X_DEV Dev) {
    (void)Dev;
    HAL_Delay(5);
    return VL53L0X_ERROR_NONE;
}

/* ================================================================
 *  其他平台辅助函数
 * ================================================================ */

int32_t VL53L0X_comms_initialise(uint8_t comms_type, uint16_t comms_speed_khz) {
    (void)comms_type; (void)comms_speed_khz;
    return 0;
}

int32_t VL53L0X_comms_close(void) { return 0; }
int32_t VL53L0X_cycle_power(void) { return 0; }
int32_t VL53L0X_set_gpio(uint8_t level) { (void)level; return 0; }
int32_t VL53L0X_get_gpio(uint8_t *plevel) { *plevel = 0; return 0; }
int32_t VL53L0X_release_gpio(void) { return 0; }

int32_t VL53L0X_platform_wait_us(int32_t wait_us) {
    for (volatile int32_t i = 0; i < wait_us * 20; i++);
    return 0;
}

int32_t VL53L0X_wait_ms(int32_t wait_ms) {
    HAL_Delay(wait_ms);
    return 0;
}

int32_t VL53L0X_get_timer_frequency(int32_t *ptimer_freq_hz) {
    *ptimer_freq_hz = 1000;
    return 0;
}

int32_t VL53L0X_get_timer_value(int32_t *ptimer_count) {
    *ptimer_count = (int32_t)HAL_GetTick();
    return 0;
}

/* ================================================================
 *  简化 API —— 对用户友好的封装
 * ================================================================ */

static VL53L0X_Dev_t       vl53_dev_data;
static VL53L0X_DEV         vl53_dev = &vl53_dev_data;
static VL53L0X_RangingMeasurementData_t vl53_range;

/**
 * @brief  VL53L0X 初始化（单次测距模式）
 *         执行完整校准流程，耗时约 1~2 秒
 */
void VL53L0X_SimpleInit(void) {
    uint8_t vhv, phase;
    vl53_dev->I2cDevAddr = 0x52;  // VL53L0X 8-bit I2C 地址
    VL53L0X_DataInit(vl53_dev);
    VL53L0X_StaticInit(vl53_dev);
    VL53L0X_PerformRefCalibration(vl53_dev, &vhv, &phase);
    VL53L0X_SetDeviceMode(vl53_dev, VL53L0X_DEVICEMODE_SINGLE_RANGING);
}

/**
 * @brief  单次测距，返回距离（毫米）
 * @retval 0~2000 mm，0xFFFF 表示测量失败或超出量程
 */
uint16_t VL53L0X_SimpleGetDistance(void) {
    VL53L0X_Error status;
    status = VL53L0X_PerformSingleRangingMeasurement(vl53_dev, &vl53_range);
    if (status == VL53L0X_ERROR_NONE &&
        vl53_range.RangeStatus == 0) {
        return vl53_range.RangeMilliMeter;
    }
    return 0xFFFF;
}
