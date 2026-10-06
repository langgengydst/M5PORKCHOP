// NRF Tools Menu - scanner / sniffer / ping / mousejack UI
#pragma once

#include <Arduino.h>
#include <M5Unified.h>

class NrfMenu {
public:
    static void show();
    static void hide();
    static void update();
    static bool isActive() { return active; }
    static void draw(M5Canvas& canvas);

private:
    enum class View : uint8_t { Tools = 0, Scan, Sniff, Ping, Mousejack };
    static bool active;
    static View view;
    static uint8_t toolSel;
    static bool keyWasPressed;
    static uint8_t sniffListSel;
};
