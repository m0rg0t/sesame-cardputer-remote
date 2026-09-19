#pragma once

#include "Arduino.h"

struct SimMdns {
    bool begin(const char*) { return true; }
    void end() {}
    IPAddress queryHost(const char*, std::uint32_t) { return {}; }
};

inline SimMdns MDNS;
