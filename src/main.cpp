#include <Arduino.h>
#include <SPIFFS.h>
#include "ina219.h"
#include "mosfet.h"
#include "wifi_server.h"

// 전역 서버/소켓 객체 정의
AsyncWebServer server(WEB_SERVER_PORT);
AsyncWebSocket ws("/ws");

// 측정 히스토리 전역 배열
HistoryEntry history[HISTORY_MAX];
int history_count = 0;

// 데이터 수집 설정
#define SAMPLE_DURATION_MS   3000   // 부하 ON 후 수집 시간 (ms)
#define COOLDOWN_MS          3000   // 부하 OFF 후 대기 시간 (ms)
#define MAX_SAMPLES          2000   // 최대 샘플 수 (ESP32 SRAM 여유분 기준)

// 수집 버퍼
float sample_buf[MAX_SAMPLES];      // 전류 샘플 배열 (mA)
uint32_t timestamp_buf[MAX_SAMPLES]; // 타임스탬프 배열 (ms)
int sample_count = 0;

// 동작 모드
enum class Mode {
    IDLE,       // 대기
    COLLECTING, // 데이터 수집 중
    DUMPING     // 시리얼 전송 중
};
Mode current_mode = Mode::IDLE;

// ------- Feature 추출 (stub) -------
// 수집된 샘플에서 Feature 4종 추출
struct Features {
    float peak_current;    // 최대 돌입 전류 (mA)
    float steady_current;  // 정상 상태 평균 전류 (mA)
    float settling_time;   // 안정화 소요 시간 (ms)
    float std_dev;         // 정상 상태 표준편차 (mA)
};

Features extract_features(float *buf, uint32_t *ts, int count) {
    // TODO: 납땜 후 실제 파형 확인하고 구현
    Features f = {0.0f, 0.0f, 0.0f, 0.0f};
    return f;
}

// ------- 시리얼 Data Dump -------
// 수집된 샘플 전체를 CSV 형식으로 시리얼 전송
void serial_dump() {
    Serial.println("=== DATA_START ===");
    Serial.println("timestamp_ms,current_mA");
    for (int i = 0; i < sample_count; i++) {
        Serial.printf("%lu,%.4f\n", timestamp_buf[i], sample_buf[i]);
    }
    Serial.println("=== DATA_END ===");
}

// ------- 데이터 수집 루프 -------
// MOSFET ON → 샘플링 → MOSFET OFF → Dump → 대기
void run_collection_cycle() {
    sample_count = 0;
    current_mode = Mode::COLLECTING;

    mosfet_on();
    uint32_t start = millis();

    while (millis() - start < SAMPLE_DURATION_MS && sample_count < MAX_SAMPLES) {
        sample_buf[sample_count] = ina219_read_current_mA();
        timestamp_buf[sample_count] = millis() - start;
        sample_count++;

        // WebSocket으로 실시간 전송
        ws_send_current(sample_buf[sample_count - 1]);
    }

    mosfet_off();
    current_mode = Mode::DUMPING;

    // 시리얼 전송
    serial_dump();

    // Feature 추출 (stub)
    Features f = extract_features(sample_buf, timestamp_buf, sample_count);

    // 분류 결과 전송 (stub: 납땜 후 ML 연동 시 교체)
    String label = "Unknown";
    ws_send_result(label, f.peak_current, f.steady_current, f.settling_time, f.std_dev);
    history_add(label, f.peak_current, f.steady_current, f.settling_time, f.std_dev);
    ws_send_history();

    current_mode = Mode::IDLE;
    delay(COOLDOWN_MS);
}

void setup() {
    Serial.begin(115200);

    // SPIFFS 초기화 (웹 파일 서빙용)
    if (!SPIFFS.begin(true)) {
        Serial.println("SPIFFS 초기화 실패");
        return;
    }

    mosfet_init();
    ina219_init();
    wifi_connect();
    server_init();

    Serial.println("초기화 완료");
}

void loop() {
    ws.cleanupClients();

    // 현재는 자동 수집 루프 비활성화 (웹 버튼으로 제어)
    // run_collection_cycle() 은 WebSocket 명령으로 트리거됨
}