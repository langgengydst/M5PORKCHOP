// NRF Tools Menu - UI over NrfTools hardware lifecycle
#include "nrf_menu.h"
#include "display.h"
#include "../modes/nrf_tools.h"
#include <M5Cardputer.h>
#include <string.h>

bool NrfMenu::active = false;
NrfMenu::View NrfMenu::view = NrfMenu::View::Tools;
uint8_t NrfMenu::toolSel = 0;
bool NrfMenu::keyWasPressed = false;

static const char* const NRF_TOOL_NAMES[] = {"SCANNER", "SNIFFER", "PING", "MOUSEJACK"};
static const uint8_t NRF_TOOL_COUNT = sizeof(NRF_TOOL_NAMES) / sizeof(NRF_TOOL_NAMES[0]);

void NrfMenu::show() {
    active = true;
    view = View::Tools;
    toolSel = 0;
    keyWasPressed = false;
    NrfTools::start();
}

void NrfMenu::hide() {
    NrfTools::stop();
    active = false;
}

void NrfMenu::update() {
    if (!active) return;

    // Radio tick: scanner dwell step or sniffer candidate, never both.
    NrfTools::update();

    auto& kb = M5Cardputer.Keyboard;

    if (view == View::Tools) {
        if (kb.isKeyPressed(';')) {
            if (!keyWasPressed && toolSel > 0) toolSel--;
            keyWasPressed = true;
        } else if (kb.isKeyPressed('.')) {
            if (!keyWasPressed && toolSel < NRF_TOOL_COUNT - 1) toolSel++;
            keyWasPressed = true;
        } else if (kb.isKeyPressed(KEY_ENTER)) {
            if (!keyWasPressed) {
                if (toolSel == 0) { NrfTools::beginScanner(); view = View::Scan; }
                else if (toolSel == 1) { NrfTools::beginSniffer(); view = View::Sniff; }
                else if (toolSel == 2) { NrfTools::beginPing(); view = View::Ping; }
                else { NrfTools::beginMousejack(); view = View::Mousejack; }
            }
            keyWasPressed = true;
        } else {
            keyWasPressed = false;
        }
        return;
    }

    if (view == View::Scan) {
        if (kb.isKeyPressed(KEY_BACKSPACE) || kb.isKeyPressed(KEY_ENTER)) {
            if (!keyWasPressed) {
                NrfTools::endScanner();
                view = View::Tools;
            }
            keyWasPressed = true;
        } else {
            keyWasPressed = false;
        }
        return;
    }

    if (view == View::Sniff) {
        if (kb.isKeyPressed(';')) {
            if (!keyWasPressed) {
                NrfTools::sniffChannelStep(-1);
            }
            keyWasPressed = true;
        } else if (kb.isKeyPressed('.')) {
            if (!keyWasPressed) {
                NrfTools::sniffChannelStep(+1);
            }
            keyWasPressed = true;
        } else if (kb.isKeyPressed(KEY_BACKSPACE)) {
            if (!keyWasPressed) {
                NrfTools::endSniffer();
                view = View::Tools;
            }
            keyWasPressed = true;
        } else {
            keyWasPressed = false;
        }
        return;
    }

    // Ping view
    if (view == View::Ping) {
        if (kb.isKeyPressed(KEY_ENTER)) {
            if (!keyWasPressed) NrfTools::pingFireOnce();
            keyWasPressed = true;
        } else if (kb.isKeyPressed('.')) {
            if (!keyWasPressed) NrfTools::pingToggleAuto();
            keyWasPressed = true;
        } else if (kb.isKeyPressed(KEY_BACKSPACE)) {
            if (!keyWasPressed) {
                NrfTools::endPing();
                view = View::Tools;
            }
            keyWasPressed = true;
        } else {
            keyWasPressed = false;
        }
        return;
    }

    // Mousejack: any key during injection aborts; enter confirms GO when Ready.
    if (view == View::Mousejack) {
        if (NrfTools::mousejackState() == NrfTools::MjState::Injecting) {
            // ANY key aborts (the hint line promises it): check every press
            // edge, printable chars plus both erase keys.
            if (kb.isChange()) {
                auto st = kb.keysState();
                bool anyKey = st.backspace || st.del;
                for (char c : st.word) {
                    if (c >= 0x20 && c <= 0x7E) anyKey = true;
                }
                if (anyKey) {
                    NrfTools::mousejackAbort();
                    keyWasPressed = true;
                }
            }
            return;
        }
        if (kb.isKeyPressed(KEY_ENTER)) {
            if (!keyWasPressed) {
                if (NrfTools::mousejackState() == NrfTools::MjState::Ready) {
                    NrfTools::mousejackGo();
                }
            }
            keyWasPressed = true;
        } else if (kb.isKeyPressed(';')) {
            if (!keyWasPressed) NrfTools::mousejackChannelStep(-1);
            keyWasPressed = true;
        } else if (kb.isKeyPressed('.')) {
            if (!keyWasPressed) NrfTools::mousejackChannelStep(+1);
            keyWasPressed = true;
        } else if (kb.isKeyPressed(KEY_BACKSPACE)) {
            if (!keyWasPressed) {
                NrfTools::endMousejack();
                view = View::Tools;
            }
            keyWasPressed = true;
        } else {
            keyWasPressed = false;
        }
        return;
    }
}

static void drawGuardScreen(M5Canvas& canvas, NrfTools::RadioState s) {
    canvas.setTextSize(2);
    canvas.drawString("NRF TOOLS", 4, 8);
    canvas.setTextSize(1);
    const char* l2 = "";
    const char* l3 = "";
    switch (s) {
        case NrfTools::RadioState::CapMissing:
            l2 = "CAP NOT DETECTED.";
            l3 = "BUILD THE CAP. OR DONT. IM A UI.";
            break;
        case NrfTools::RadioState::Unresponsive:
            l2 = "RADIO UNRESPONSIVE.";
            l3 = "WIRING OR SOCKET SUSPECT.";
            break;
        case NrfTools::RadioState::LoRaConflict:
            l2 = "LORA CAP OWNS G3/G13/G15.";
            l3 = "SET GPS SOURCE TO GROVE.";
            break;
        default:
            l2 = "";
            l3 = "";
            break;
    }
    canvas.drawString(l2, 4, 36);
    canvas.drawString(l3, 4, 48);
    canvas.drawString("BACKTICK EXITS ANY TIME.", 4, 96);
}

void NrfMenu::draw(M5Canvas& canvas) {
    if (!active) return;

    canvas.fillSprite(COLOR_BG);
    canvas.setTextColor(COLOR_FG);
    canvas.setTextDatum(TL_DATUM);
    canvas.setFont(&fonts::Font0);

    NrfTools::RadioState rs = NrfTools::radioState();

    // Guard screens first: no radio, no tools.
    if (rs != NrfTools::RadioState::Ready) {
        drawGuardScreen(canvas, rs);
        return;
    }

    if (view == View::Tools) {
        canvas.setTextSize(2);
        canvas.drawString("NRF TOOLS", 4, 4);
        canvas.setTextSize(1);
        int y = 26;
        for (uint8_t i = 0; i < NRF_TOOL_COUNT; i++) {
            bool sel = (i == toolSel);
            if (sel) canvas.fillRect(0, y - 2, 100, 12, COLOR_FG);
            canvas.setTextColor(sel ? COLOR_BG : COLOR_FG);
            canvas.drawString(NRF_TOOL_NAMES[i], 4, y);
            canvas.setTextColor(COLOR_FG);
            y += 14;
        }
        canvas.drawString(NrfTools::radioStateLine(), 4, y + 4);
        canvas.drawString("; . MOVE ENT:GO", 4, 96);
        return;
    }

    if (view == View::Scan) {
        canvas.setTextSize(2);
        canvas.drawString("SCAN", 4, 2);
        canvas.setTextSize(1);
        // 84 channels across 232px: 2px bars + 4px margin, peak-normalized.
        uint16_t peak = 1;
        for (uint8_t ch = 0; ch < NRF_CHANNEL_COUNT; ch++) {
            uint16_t h = NrfTools::channelHits(ch);
            if (h > peak) peak = h;
        }
        int baseY = 62;
        for (uint8_t ch = 0; ch < NRF_CHANNEL_COUNT; ch++) {
            uint16_t h = NrfTools::channelHits(ch);
            uint8_t barH = (uint8_t)((h * 28UL) / peak);
            if (barH == 0 && h > 0) barH = 1;
            int x = 4 + ch * 2;
            if (ch == NrfTools::scanChannel()) {
                canvas.drawFastVLine(x - 1, baseY - 30, 32, COLOR_FG);
            }
            if (barH) canvas.drawFastVLine(x, baseY - barH, barH, COLOR_FG);
        }
        // wifi channel landmarks (ch1/6/11/14 are wifi, not nrf)
        canvas.drawFastHLine(4, baseY + 1, NRF_CHANNEL_COUNT * 2, COLOR_FG);
        char buf[48];
        snprintf(buf, sizeof(buf), "CH %u PEAK %u  RPD DWELL",
                 (unsigned)NrfTools::scanChannel(), (unsigned)peak);
        canvas.drawString(buf, 4, 68);
        canvas.drawString("LIVE 2.4G SNIFF. CH 0-83 BY LAW.", 4, 80);
        canvas.drawString("ENT/BS:BACK", 4, 96);
        return;
    }

    if (view == View::Sniff) {
        canvas.setTextSize(2);
        canvas.drawString("SNIFF", 4, 2);
        canvas.setTextSize(1);
        char buf[64];
        snprintf(buf, sizeof(buf), "CH %u  PRE %u/%u  SUF %u/255",
                 (unsigned)NrfTools::sniffChannel(),
                 (unsigned)NrfTools::sniffPrefixIndex() + 1,
                 (unsigned)NRF_KNOWN_PREFIX_COUNT,
                 (unsigned)NrfTools::sniffProgress());
        canvas.drawString(buf, 4, 22);
        uint8_t n = NrfTools::foundCount();
        if (n == 0) {
            canvas.drawString("PREFIX SCAN. NO HITS YET.", 4, 36);
            canvas.drawString("POWER YOUR OWN DONGLE UP.", 4, 48);
        } else {
            // 4 visible rows, newest first: the found ring can hold 6, and
            // rendering from index 0 would push later hits off-screen.
            int y = 34;
            for (uint8_t i = 0; i < n && i < 4; i++) {
                const NrfTools::Found* f = NrfTools::found(i);
                snprintf(buf, sizeof(buf), "%02X%02X%02X%02X%02X CH%u L%u",
                         f->addr[0], f->addr[1], f->addr[2], f->addr[3],
                         f->addr[4], f->channel, f->payloadLen);
                canvas.drawString(buf, 4, y);
                y += 12;
            }
        }
        snprintf(buf, sizeof(buf), "CH HITS: %u", NrfTools::channelHits(NrfTools::sniffChannel()));
        canvas.drawString(buf, 4, 84);
        canvas.drawString("; . CH  BS:BACK", 4, 96);
        return;
    }

    if (view == View::Ping) {
        canvas.setTextSize(2);
        canvas.drawString("PING", 4, 2);
        canvas.setTextSize(1);
        const char* st = "ARMED";
        if (NrfTools::pingState() == NrfTools::PingState::Pong) st = "PONG!";
        else if (NrfTools::pingState() == NrfTools::PingState::Miss) st = "MISS";
        char buf[56];
        snprintf(buf, sizeof(buf), "%s   SENT %u  PONG %u", st,
                 (unsigned)NrfTools::pingSent(), (unsigned)NrfTools::pingPongs());
        canvas.drawString(buf, 4, 24);
        snprintf(buf, sizeof(buf), "STREAK %u   AUTO %s", (unsigned)NrfTools::pingStreak(),
                 NrfTools::pingAuto() ? "ON" : "OFF");
        canvas.drawString(buf, 4, 38);
        canvas.drawString("PAIRS WITH RF24 GETTINGSTARTED", 4, 56);
        canvas.drawString("NODE ON CH76. 1MBPS ESB.", 4, 68);
        canvas.drawString("ENT:FIRE .:AUTO BS:BACK", 4, 96);
        return;
    }

    // Mousejack view
    {
        canvas.setTextSize(2);
        canvas.drawString("MOUSEJACK", 4, 2);
        canvas.setTextSize(1);
        canvas.drawString("OWN LAB ONLY. NO NEIGHBORS.", 4, 20);
        char buf[64];
        const uint8_t* t = NrfTools::mousejackTarget();
        NrfTools::MjState s = NrfTools::mousejackState();
        switch (s) {
            case NrfTools::MjState::Scanning:
                snprintf(buf, sizeof(buf), "SCAN CH%u... POWER YOUR DONGLE",
                         (unsigned)NrfTools::mousejackChannel());
                break;
            case NrfTools::MjState::Ready:
                snprintf(buf, sizeof(buf), "LOCK %02X%02X%02X%02X%02X CH%u",
                         t[4], t[3], t[2], t[1], t[0], (unsigned)NrfTools::mousejackChannel());
                break;
            case NrfTools::MjState::Injecting:
                snprintf(buf, sizeof(buf), "TYPING OINK %u/%u", (unsigned)NrfTools::mousejackProgress(),
                         (unsigned)NRF_MJ_MAX_KEYS);
                break;
            case NrfTools::MjState::Finished:
                snprintf(buf, sizeof(buf), "DONE. %u KEYSTROKES SENT.", (unsigned)NrfTools::mousejackProgress());
                break;
            case NrfTools::MjState::Aborted:
                snprintf(buf, sizeof(buf), "ABORTED AT %u.", (unsigned)NrfTools::mousejackProgress());
                break;
            default:
                buf[0] = '\0';
                break;
        }
        canvas.drawString(buf, 4, 36);
        canvas.drawString("PLAIN SHOCKBURST. NO ACK. PA_MIN.", 4, 52);
        canvas.drawString("ENT:GO ANYKEY:ABORT BS:BACK", 4, 96);
        return;
    }
}
