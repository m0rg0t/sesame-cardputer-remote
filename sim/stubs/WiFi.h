#pragma once

#include "Arduino.h"

enum wl_status_t {
    WL_IDLE_STATUS = 0,
    WL_NO_SSID_AVAIL = 1,
    WL_SCAN_COMPLETED = 2,
    WL_CONNECTED = 3,
    WL_CONNECT_FAILED = 4,
    WL_CONNECTION_LOST = 5,
    WL_DISCONNECTED = 6,
};

constexpr int WIFI_STA = 1;
constexpr int WIFI_SCAN_FAILED = -2;
constexpr int WIFI_SCAN_RUNNING = -1;
constexpr int WIFI_AUTH_OPEN = 0;

class WiFiClient {};

class SimWiFi {
public:
    wl_status_t status() const { return connected ? WL_CONNECTED : WL_DISCONNECTED; }
    String SSID() const { return ssid; }
    String SSID(int) const { return {}; }
    int RSSI(int) const { return -65; }
    int encryptionType(int) const { return 1; }
    IPAddress gatewayIP() const { return {192, 168, 4, 1}; }
    IPAddress localIP() const { return {192, 168, 4, 28}; }
    void setAutoReconnect(bool) {}
    void disconnect(bool, bool) { connected = false; }
    void begin(const char* value, const char*) { ssid = value; }
    void mode(int) {}
    void setSleep(bool) {}
    void setHostname(const char*) {}
    void persistent(bool) {}
    void scanDelete() {}
    int scanNetworks(bool, bool) { return WIFI_SCAN_RUNNING; }
    int scanComplete() const { return WIFI_SCAN_RUNNING; }

    bool connected = true;
    String ssid = "Studio WiFi";
};

inline SimWiFi WiFi;
