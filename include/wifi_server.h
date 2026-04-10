#pragma once
#include <WiFi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <ArduinoJson.h>
#include "mosfet.h"
#include "secrets.h"
#include <WiFiMulti.h>

// 웹 서버 포트
#define WEB_SERVER_PORT 80

// 전역 서버/소켓 객체
extern AsyncWebServer server;
extern AsyncWebSocket ws;

// 측정 히스토리 최대 저장 개수
#define HISTORY_MAX 50

// 측정 히스토리 항목
struct HistoryEntry {
    String timestamp;
    String label;
    float peak_current;
    float steady_current;
    float settling_time;
    float std_dev;
};

extern HistoryEntry history[HISTORY_MAX];
extern int history_count;

// WiFi 연결
inline void wifi_connect() {
    WiFiMulti wifiMulti;
    wifiMulti.addAP(WIFI_SSID_1, WIFI_PASSWORD_1);
    wifiMulti.addAP(WIFI_SSID_2, WIFI_PASSWORD_2);

    Serial.print("WiFi 연결 중");
    while (wifiMulti.run() != WL_CONNECTED) {
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
        String msg = String((char *)data).substring(0, len);
        if (msg == "ON") {
            extern bool collect_requested;
            collect_requested = true;
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
        for (int i = 0; i < HISTORY_MAX - 1; i++) {
            history[i] = history[i + 1];
        }
        history_count = HISTORY_MAX - 1;
    }

    unsigned long ms = millis();
    unsigned long sec = ms / 1000;
    unsigned long min = sec / 60;
    unsigned long hr  = min / 60;
    char ts[16];
    snprintf(ts, sizeof(ts), "%02lu:%02lu:%02lu", hr, min % 60, sec % 60);

    history[history_count] = {
        .timestamp      = String(ts),
        .label          = label,
        .peak_current   = peak,
        .steady_current = steady,
        .settling_time  = settling,
        .std_dev        = std_dev
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
    server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
        request->send(SPIFFS, "/index.html", "text/html");
    });

    ws.onEvent(on_ws_event);
    server.addHandler(&ws);

    server.begin();
    Serial.println("웹 서버 시작");
}

inline void ws_cleanupClients() {
    ws.cleanupClients();
}

// 수집 완료 후 전체 파형 데이터 일괄 전송
// 수집 완료 후 전체 파형 데이터 일괄 전송
inline void ws_send_waveform(float *buf, uint32_t *ts, int count) {
    if (ws.count() == 0) return;

    // 최대 300포인트로 다운샘플링
    int step = max(1, count / 300);

    // 힙에 할당 (스택 오버플로우 방지)
    DynamicJsonDocument doc(8192);
    doc["type"] = "waveform";
    JsonArray arr = doc.createNestedArray("data");

    for (int i = 0; i < count; i += step) {
        JsonObject obj = arr.createNestedObject();
        obj["t"] = ts[i];
        obj["v"] = buf[i];
    }

    String json;
    serializeJson(doc, json);
    ws.textAll(json);
}