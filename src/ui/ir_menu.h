// IR Tools Menu - capture view, session list, replay, NEC send form
#pragma once

#include <Arduino.h>
#include <M5Unified.h>

class IrMenu {
public:
    static void show();
    static void hide();
    static void update();
    static bool isActive() { return active; }
    static void draw(M5Canvas& canvas);

    // NEC form buffer size (8 hex chars + NUL)
    static constexpr uint8_t NEC_ENTRY_MAX = 9;

private:
    enum class View : uint8_t { Tools = 0, Live, List, Nec };
    static bool active;
    static View view;
    static uint8_t toolSel;
    static bool keyWasPressed;
    static uint8_t listSel;
    static char necEntry[NEC_ENTRY_MAX];
    static uint8_t necLen;
};
