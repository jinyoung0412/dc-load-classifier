#pragma once
#include <Wire.h>

// INA219 I2C 기본 주소
#define INA219_ADDRESS          0x40

// 레지스터 주소
#define INA219_REG_CONFIG       0x00
#define INA219_REG_SHUNTVOLTAGE 0x01
#define INA219_REG_BUSVOLTAGE   0x02
#define INA219_REG_POWER        0x03
#define INA219_REG_CURRENT      0x04
#define INA219_REG_CALIBRATION  0x05

// Configuration Register 설정값
// BRNG=0(16V), PGA=00(±40mV), BADC=0000(9-bit), SADC=0000(9-bit), MODE=111(continuous)
#define INA219_CONFIG_9BIT_NO_AVG 0x0007

// 캘리브레이션 설정값
// 전류 LSB = 0.1mA, Cal = trunc(0.04096 / (0.1mA * 0.1Ω)) = 4096
#define INA219_CALIBRATION_VALUE  4096

// 전류 변환 계수 (mA/LSB)
#define INA219_CURRENT_LSB        0.1f

// INA219 레지스터에 16비트 값 write
inline void ina219_write_register(uint8_t reg, uint16_t value) {
    Wire.beginTransmission(INA219_ADDRESS);
    Wire.write(reg);
    Wire.write((value >> 8) & 0xFF);  // 상위 바이트
    Wire.write(value & 0xFF);          // 하위 바이트
    Wire.endTransmission();
}

// INA219 레지스터에서 16비트 값 read
inline int16_t ina219_read_register(uint8_t reg) {
    Wire.beginTransmission(INA219_ADDRESS);
    Wire.write(reg);
    Wire.endTransmission();

    Wire.requestFrom(INA219_ADDRESS, (uint8_t)2);
    int16_t value = (Wire.read() << 8) | Wire.read();
    return value;
}

// INA219 초기화 (9-bit 해상도, Averaging OFF)
inline void ina219_init() {
    Wire.begin();
    ina219_write_register(INA219_REG_CONFIG, INA219_CONFIG_9BIT_NO_AVG);
    ina219_write_register(INA219_REG_CALIBRATION, INA219_CALIBRATION_VALUE);
}

// 전류 읽기 (mA 단위)
inline float ina219_read_current_mA() {
    int16_t raw = ina219_read_register(INA219_REG_CURRENT);
    return raw * INA219_CURRENT_LSB;
}

// 버스 전압 읽기 (V 단위)
inline float ina219_read_voltage_V() {
    int16_t raw = ina219_read_register(INA219_REG_BUSVOLTAGE);
    return (raw >> 3) * 0.004f;  // LSB = 4mV, 하위 3비트 제거
}