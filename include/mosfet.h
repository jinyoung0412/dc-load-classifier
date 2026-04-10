#pragma once
#include <Arduino.h>

// MOSFET 제어 핀 (LR7843 모듈 IN 핀)
#define MOSFET_PIN 5

// MOSFET 초기화
inline void mosfet_init() {
    pinMode(MOSFET_PIN, OUTPUT);
    digitalWrite(MOSFET_PIN, LOW);  // 초기 상태: OFF
}

// 부하 전원 ON
inline void mosfet_on() {
    digitalWrite(MOSFET_PIN, HIGH);
}

// 부하 전원 OFF
inline void mosfet_off() {
    digitalWrite(MOSFET_PIN, LOW);
}