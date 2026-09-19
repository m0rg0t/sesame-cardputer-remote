#pragma once

#include "Arduino.h"
#include "WiFi.h"

constexpr int HTTP_CODE_OK = 200;

class HTTPClient {
public:
    void setConnectTimeout(int) {}
    void setTimeout(int) {}
    bool begin(WiFiClient&, const String&, int, const char*) { return true; }
    int GET() { return 503; }
    int POST(const String&) { return 503; }
    String getString() const { return {}; }
    void addHeader(const char*, const char*) {}
    void end() {}
};
