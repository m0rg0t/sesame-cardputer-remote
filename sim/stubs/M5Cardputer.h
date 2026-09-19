#pragma once

#include <M5GFX.h>

#include <vector>

namespace m5 {
enum class board_t {
    board_unknown,
    board_M5CardputerADV,
    board_M5Cardputer,
};
}

struct SimM5Config {
    bool internal_mic = false;
    bool internal_spk = false;
};

struct SimM5 {
    SimM5Config config() const { return {}; }
    m5::board_t getBoard() const { return m5::board_t::board_M5CardputerADV; }
};

struct SimButton {
    bool wasPressed() const { return false; }
};

struct SimKeysState {
    bool enter = false;
    bool del = false;
    bool space = false;
    bool fn = false;
    std::vector<char> word;
};

struct SimKeyboard {
    bool isChange() const { return false; }
    bool isPressed() const { return false; }
    const SimKeysState& keysState() const { return keys; }
    SimKeysState keys;
};

struct SimCardputer {
    M5GFX Display;
    SimButton BtnA;
    SimKeyboard Keyboard;

    void begin(const SimM5Config&, bool) {}
    void update() {}
};

inline SimM5 M5;
inline SimCardputer M5Cardputer;
