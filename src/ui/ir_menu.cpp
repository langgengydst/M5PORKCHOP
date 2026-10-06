// IR Tools Menu - UI over IrTools hardware lifecycle
#include "ir_menu.h"
#include "display.h"
#include "../modes/ir_tools.h"
#include "../core/config.h"
#include <M5Cardputer.h>
#include <string.h>

bool IrMenu::active = false;
IrMenu::View IrMenu::view = IrMenu::View::Tools;
uint8_t IrMenu::toolSel = 0;
bool IrMenu::keyWasPressed = false;
uint8_t IrMenu::listSel = 0;
char IrMenu::necEntry[IrMenu::NEC_ENTRY_MAX] = "";
uint8_t IrMenu::necLen = 0;

static const char* const IR_TOOL_NAMES[] = {"LIVE CAPTURE", "REPLAY LAST", "CAPTURES", "NEC SEND"};
static const uint8_t IR_TOOL_COUNT = sizeof(IR_TOOL_NAMES) / sizeof(IR_TOOL_NAMES[0]);

void IrMenu::show() {
    active = true;
    view = View::Tools;
    toolSel = 0;
    listSel = 0;
    necLen = 0;
    necEntry[0] = '\0';
    keyWasPressed = false;
    IrTools::start();
}

void IrMenu::hide() {
    IrTools::stop();
    active = false;
}

static bool navPressed() {
    return M5Cardputer.Keyboard.isKeyPressed(';') ||
           M5Cardputer.Keyboard.isKeyPressed('.') ||
           M5Cardputer.Keyboard.isKeyPressed(KEY_ENTER) ||
           M5Cardputer.Keyboard.isKeyPressed(KEY_BACKSPACE);
}

void IrMenu::update() {
    if (!active) return;

    // Hardware poll every tick (decode + at most one scheduled TX action).
    IrTools::update();

    auto& kb = M5Cardputer.Keyboard;
    bool nav = navPressed();

    if (view == View::Tools) {
        if (kb.isKeyPressed(';')) {
            if (!keyWasPressed && toolSel > 0) toolSel--;
            keyWasPressed = true;
        } else if (kb.isKeyPressed('.')) {
            if (!keyWasPressed && toolSel < IR_TOOL_COUNT - 1) toolSel++;
            keyWasPressed = true;
        } else if (kb.isKeyPressed(KEY_ENTER)) {
            if (!keyWasPressed) {
                if (toolSel == 0) view = View::Live;
                else if (toolSel == 1) { IrTools::replayLast(); view = View::Live; }
                else if (toolSel == 2) { view = View::List; listSel = 0; }
                else { view = View::Nec; necLen = 0; necEntry[0] = '\0'; }
            }
            keyWasPressed = true;
        } else {
            keyWasPressed = false;
        }
        return;
    }

    if (view == View::Live) {
        // REPLAY re-fires on enter; backspace returns to tool list.
        if (kb.isKeyPressed(KEY_ENTER)) {
            if (!keyWasPressed) IrTools::replayLast();
            keyWasPressed = true;
        } else if (kb.isKeyPressed(KEY_BACKSPACE)) {
            if (!keyWasPressed) view = View::Tools;
            keyWasPressed = true;
        } else {
            keyWasPressed = false;
        }
        return;
    }

    if (view == View::List) {
        if (kb.isKeyPressed(';')) {
            if (!keyWasPressed && listSel > 0) listSel--;
            keyWasPressed = true;
        } else if (kb.isKeyPressed('.')) {
            if (!keyWasPressed && listSel + 1 < IrTools::sessionListCount()) listSel++;
            keyWasPressed = true;
        } else if (kb.isKeyPressed(KEY_BACKSPACE)) {
            if (!keyWasPressed) view = View::Tools;
            keyWasPressed = true;
        } else {
            keyWasPressed = false;
        }
        return;
    }

    // NEC form: hex entry. keys.backspace || keys.del both erase (Fn-layer split).
    // The whole input block is edge-gated on isChange(): without it a held
    // key lands in keys.word on every loop pass and one tap types a dozen
    // digits. OINK typing gets the same gate from handleInput's isChange.
    if (view == View::Nec && kb.isChange()) {
        auto keys = kb.keysState();
        bool changed = false;
        for (char c : keys.word) {
            if ((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') ||
                (c >= 'A' && c <= 'F')) {
                if (necLen < NEC_ENTRY_MAX - 1) {
                    necEntry[necLen++] = c;
                    necEntry[necLen] = '\0';
                    changed = true;
                }
            }
        }
        if (keys.backspace || keys.del) {
            if (!keyWasPressed && necLen > 0) {
                necEntry[--necLen] = '\0';
            }
            keyWasPressed = true;
        } else if (kb.isKeyPressed(KEY_ENTER)) {
            if (!keyWasPressed && necLen > 0) {
                uint32_t v = 0;
                if (IrTools::parseHex32(necEntry, v)) {
                    IrTools::sendNecValue(v);
                    necLen = 0;
                    necEntry[0] = '\0';
                    view = View::Live;
                }
            }
            keyWasPressed = true;
        } else if (kb.isKeyPressed(KEY_BACKSPACE)) {
            if (!keyWasPressed) {
                if (necLen > 0) {
                    necEntry[--necLen] = '\0';
                } else {
                    view = View::Tools;
                }
            }
            keyWasPressed = true;
        } else if (!changed) {
            keyWasPressed = false;
        }
        return;
    }

    (void)nav;
}

static const char* irStatusLine(IrTools::Status s) {
    switch (s) {
        case IrTools::Status::HeapBlocked:   return "HEAP GATE SHUT. NO CAP TODAY.";
        case IrTools::Status::RadioAbsent:   return "EAR PRICKED. NO LIGHT YET.";
        case IrTools::Status::CaptureReady:  return "GOT ONE. SNORTED + SAVED.";
        case IrTools::Status::Overflow:      return "PROTOCOL NOT CAPTURED. TOO FAT.";
        case IrTools::Status::Saved:         return "SAVED TO SD. OINK.";
        case IrTools::Status::Saving:        return "SAVING...";
        case IrTools::Status::SdAbsent:      return "NO SD. HELD IN RAM ONLY.";
        case IrTools::Status::PressureHeld:  return "HEAP SQUEAL. SAVE HELD.";
        case IrTools::Status::Sending:       return "BLASTING LIGHT. DON'T LOOK.";
        default:                             return "";
    }
}

void IrMenu::draw(M5Canvas& canvas) {
    if (!active) return;

    canvas.fillSprite(COLOR_BG);
    canvas.setTextColor(COLOR_FG);
    canvas.setTextDatum(TL_DATUM);
    canvas.setFont(&fonts::Font0);

    if (view == View::Tools) {
        canvas.setTextSize(2);
        canvas.drawString("IR TOOLS", 4, 4);
        canvas.setTextSize(1);
        int y = 26;
        for (uint8_t i = 0; i < IR_TOOL_COUNT; i++) {
            bool sel = (i == toolSel);
            if (sel) canvas.fillRect(0, y - 2, 110, 12, COLOR_FG);
            canvas.setTextColor(sel ? COLOR_BG : COLOR_FG);
            canvas.drawString(IR_TOOL_NAMES[i], 4, y);
            canvas.setTextColor(COLOR_FG);
            y += 14;
        }
        // Session capture count on the side
        char buf[40];
        snprintf(buf, sizeof(buf), "SAVED: %u", (unsigned)IrTools::sessionSavedCount());
        canvas.drawString(buf, 120, 26);
        canvas.drawString("; . MOVE ENT:GO", 4, 96);
        return;
    }

    if (view == View::Live) {
        canvas.setTextSize(2);
        canvas.drawString("LIVE CAP", 4, 4);
        canvas.setTextSize(1);
        const IrTools::Capture* c = IrTools::lastCapture();
        if (!c) {
            canvas.drawString("POINT A REMOTE. PRESS.", 4, 26);
            canvas.drawString("ANY REMOTE. PIG ISN'T FUSSY.", 4, 38);
        } else {
            char buf[48];
            snprintf(buf, sizeof(buf), "PROTO: %s", c->protocolName);
            canvas.drawString(buf, 4, 26);
            if (c->stateful) {
                snprintf(buf, sizeof(buf), "STATE: %uB BITS:%u", c->stateLen, c->bits);
            } else {
                snprintf(buf, sizeof(buf), "VAL: %016llX", (unsigned long long)c->value);
            }
            canvas.drawString(buf, 4, 38);
            snprintf(buf, sizeof(buf), "BITS:%u RAW:%u %s%s",
                     c->bits, c->rawlen, c->repeat ? "RPT " : "",
                     c->overflow ? "OVR" : "");
            canvas.drawString(buf, 4, 50);
            snprintf(buf, sizeof(buf), "SAVED THIS RUN: %u",
                     (unsigned)IrTools::sessionSavedCount());
            canvas.drawString(buf, 4, 62);
        }
        canvas.drawString(irStatusLine(IrTools::getStatus()), 4, 76);
        canvas.drawString("ENTER:REPLAY  BS:BACK", 4, 96);
        return;
    }

    if (view == View::List) {
        canvas.setTextSize(2);
        canvas.drawString("CAPTURES", 4, 4);
        canvas.setTextSize(1);
        uint8_t n = IrTools::sessionListCount();
        if (!Config::isSDAvailable()) {
            canvas.drawString("NO SD CARD. NOTHING TO LIST.", 4, 26);
        } else if (n == 0) {
            canvas.drawString("EMPTY STY. GO CAPTURE.", 4, 26);
        } else {
            // Window of 6 rows around the cursor: listSel can reach 16
            // entries, and rendering from index 0 would walk it off-screen.
            uint8_t start = (listSel > 5) ? (uint8_t)(listSel - 5) : 0;
            int y = 26;
            for (uint8_t i = start; i < n && i < start + 6; i++) {
                bool sel = (i == listSel);
                if (sel) canvas.fillRect(0, y - 2, 100, 12, COLOR_FG);
                canvas.setTextColor(sel ? COLOR_BG : COLOR_FG);
                canvas.drawString(IrTools::sessionFilename(i), 4, y);
                canvas.setTextColor(COLOR_FG);
                y += 12;
            }
        }
        canvas.drawString("; . MOVE  BS:BACK", 4, 96);
        return;
    }

    // NEC form
    canvas.setTextSize(2);
    canvas.drawString("NEC SEND", 4, 4);
    canvas.setTextSize(1);
    canvas.drawString("8 HEX CHARS. 32 BITS OF POWER.", 4, 26);
    char line[NEC_ENTRY_MAX + 4];
    snprintf(line, sizeof(line), "[%s]", necEntry);
    canvas.setTextSize(2);
    canvas.drawString(line, 4, 44);
    canvas.setTextSize(1);
    canvas.drawString(irStatusLine(IrTools::getStatus()), 4, 68);
    canvas.drawString("ENTER:SEND BS:ERASE/BACK", 4, 96);
}
