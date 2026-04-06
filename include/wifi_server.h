#pragma once
#include <WiFi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <ArduinoJson.h>
#include "mosfet.h"

// WiFi 인증 정보 (실제 값으로 교체)
#define WIFI_SSID "your_ssid"
#define WIFI_PASSWORD "your_password"

// 웹 서버 포트
#define WEB_SERVER_PORT 80

// 전역 서버/소켓 객체
extern AsyncWebServer server;
extern AsyncWebSocket ws;

// 측정 히스토리 최대 저장 개수
#define HISTORY_MAX 50

// 측정 히스토리 항목
struct HistoryEntry {
    String timestamp;       // 측정 시각
    String label;           // 분류 결과 (Resistive / Motor / Electronic)
    float peak_current;     // 최대 돌입 전류 (mA)
    float steady_current;   // 정상 상태 평균 전류 (mA)
    float settling_time;    // 안정화 소요 시간 (ms)
    float std_dev;          // 정상 상태 표준편차 (mA)
};

extern HistoryEntry history[HISTORY_MAX];
extern int history_count;

// WiFi 연결
inline void wifi_connect() {
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    Serial.print("WiFi 연결 중");
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }
    Serial.println();
    Serial.print("IP 주소: ");
    Serial.println(WiFi.localIP());
}

// WebSocket 이벤트 핸들러
inline void on_ws_event(AsyncWebSocket *server, AsyncWebSocketClient *client,
                        AwsEventType type, void *arg, uint8_t *data, size_t len) {
    if (type == WS_EVT_CONNECT) {
        Serial.printf("WebSocket 클라이언트 연결: %u\n", client->id());
    } else if (type == WS_EVT_DISCONNECT) {
        Serial.printf("WebSocket 클라이언트 해제: %u\n", client->id());
    } else if (type == WS_EVT_DATA) {
        // 클라이언트로부터 MOSFET 제어 명령 수신
        String msg = String((char *)data).substring(0, len);
        if (msg == "ON") {
            mosfet_on();
            Serial.println("MOSFET ON (웹 명령)");
        } else if (msg == "OFF") {
            mosfet_off();
            Serial.println("MOSFET OFF (웹 명령)");
        }
    }
}

// 실시간 전류값 WebSocket 전송
inline void ws_send_current(float current_mA) {
    if (ws.count() == 0) return;
    StaticJsonDocument<64> doc;
    doc["type"] = "current";
    doc["value"] = current_mA;
    String json;
    serializeJson(doc, json);
    ws.textAll(json);
}

// 분류 결과 WebSocket 전송
inline void ws_send_result(const String &label, float peak, float steady,
                           float settling, float std_dev) {
    if (ws.count() == 0) return;
    StaticJsonDocument<128> doc;
    doc["type"]     = "result";
    doc["label"]    = label;
    doc["peak"]     = peak;
    doc["steady"]   = steady;
    doc["settling"] = settling;
    doc["std_dev"]  = std_dev;
    String json;
    serializeJson(doc, json);
    ws.textAll(json);
}

// 히스토리 항목 추가
inline void history_add(const String &label, float peak, float steady,
                        float settling, float std_dev) {
    if (history_count >= HISTORY_MAX) {
        // 가장 오래된 항목 제거 (앞으로 당기기)
        for (int i = 0; i < HISTORY_MAX - 1; i++) {
            history[i] = history[i + 1];
        }
        history_count = HISTORY_MAX - 1;
    }

    // 타임스탬프 생성 (ESP32 가동 시간 기준)
    unsigned long ms = millis();
    unsigned long sec = ms / 1000;
    unsigned long min = sec / 60;
    unsigned long hr  = min / 60;
    char ts[16];
    snprintf(ts, sizeof(ts), "%02lu:%02lu:%02lu", hr, min % 60, sec % 60);

    history[history_count] = {
        .timestamp     = String(ts),
        .label         = label,
        .peak_current  = peak,
        .steady_current= steady,
        .settling_time = settling,
        .std_dev       = std_dev
    };
    history_count++;
}

// 히스토리 전체를 JSON으로 WebSocket 전송
inline void ws_send_history() {
    if (ws.count() == 0) return;
    StaticJsonDocument<4096> doc;
    doc["type"] = "history";
    JsonArray arr = doc.createNestedArray("entries");
    for (int i = 0; i < history_count; i++) {
        JsonObject obj = arr.createNestedObject();
        obj["timestamp"] = history[i].timestamp;
        obj["label"]     = history[i].label;
        obj["peak"]      = history[i].peak_current;
        obj["steady"]    = history[i].steady_current;
        obj["settling"]  = history[i].settling_time;
        obj["std_dev"]   = history[i].std_dev;
    }
    String json;
    serializeJson(doc, json);
    ws.textAll(json);
}

// 웹 서버 라우팅 및 시작
inline void server_init() {
    // 대시보드 HTML 서빙
    server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
        request->send(SPIFFS, "/index.html", "text/html");
    });

    // WebSocket 등록
    ws.onEvent(on_ws_event);
    server.addHandler(&ws);

    server.begin();
    Serial.println("웹 서버 시작");
}