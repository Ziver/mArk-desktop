/**
 * WiFi Manager — connect to WiFi + NTP time sync
 */

#ifndef WIFI_MANAGER_H
#define WIFI_MANAGER_H

#include <WiFi.h>
#include <time.h>

#include "credentials.h"

// ── NTP ──
#define NTP_SERVER "pool.ntp.org"
#define GMT_OFFSET_SEC 3600      // CET (UTC+1)
#define DST_OFFSET_SEC 0         // Adjust for summer time if needed

static bool wifi_connected = false;
static bool time_synced = false;

bool setupWiFi(void (*cb)(int attempts, const char* status) = nullptr) {
    Serial.printf("[WiFi] Connecting to %s", WIFI_SSID);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 40) {
        if (cb) {
            char buf[64];
            snprintf(buf, sizeof(buf), "SSID: %s (Attempt %d/40)...", WIFI_SSID, attempts + 1);
            cb(attempts, buf);
        }
        delay(500);
        Serial.print(".");
        attempts++;
    }
    Serial.println();

    if (WiFi.status() == WL_CONNECTED) {
        wifi_connected = true;
        Serial.printf("[WiFi] Connected! IP: %s\n", WiFi.localIP().toString().c_str());

        if (cb) {
            cb(-1, "Syncing time (NTP)...");
            delay(800);
        }

        // Sync NTP
        configTime(GMT_OFFSET_SEC, DST_OFFSET_SEC, NTP_SERVER);
        struct tm timeinfo;
        if (getLocalTime(&timeinfo, 5000)) {
            time_synced = true;
            Serial.printf("[NTP] Time: %04d-%02d-%02d %02d:%02d:%02d\n",
                timeinfo.tm_year + 1900, timeinfo.tm_mon + 1, timeinfo.tm_mday,
                timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);
        } else {
            Serial.println("[NTP] Failed to sync time");
        }
        return true;
    } else {
        Serial.println("[WiFi] Connection failed!");
        if (cb) {
            cb(-2, "Failed to connect to WiFi.");
        }
        return false;
    }
}

// Reconnects WiFi if the connection has dropped. Returns true if connected.
bool ensureWiFiConnected() {
    if (WiFi.status() == WL_CONNECTED) return true;

    Serial.println("[WiFi] Connection lost, reconnecting...");
    wifi_connected = false;
    WiFi.disconnect();
    delay(500);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 20) {
        delay(500);
        Serial.print(".");
        attempts++;
    }
    Serial.println();

    if (WiFi.status() == WL_CONNECTED) {
        wifi_connected = true;
        Serial.printf("[WiFi] Reconnected! IP: %s\n", WiFi.localIP().toString().c_str());
        return true;
    }
    Serial.println("[WiFi] Reconnect failed");
    return false;
}

void getGreeting(char *buf, size_t len) {
    struct tm t;
    if (getLocalTime(&t)) {
        if (t.tm_hour < 12) snprintf(buf, len, "GOOD MORNING");
        else if (t.tm_hour < 17) snprintf(buf, len, "GOOD AFTERNOON");
        else snprintf(buf, len, "GOOD EVENING");
    } else {
        snprintf(buf, len, "HELLO");
    }
}

void getDateStr(char *buf, size_t len) {
    struct tm t;
    if (getLocalTime(&t)) {
        const char* days[] = {"Sunday","Monday","Tuesday","Wednesday","Thursday","Friday","Saturday"};
        const char* months[] = {"Jan","Feb","Mar","Apr","May","Jun","Jul","Aug","Sep","Oct","Nov","Dec"};
        snprintf(buf, len, "%s, %s %d", days[t.tm_wday], months[t.tm_mon], t.tm_mday);
    } else {
        snprintf(buf, len, "No time sync");
    }
}

#endif
