#include "TamagotchiGame.h"
#include <Adafruit_SSD1306.h>
#include <Arduino.h>
#include <math.h>
#include <string.h>

// ─── Layout ───────────────────────────────────────────────────────────────────
// HUD:  y = 0..7   (stat bars + separator at y=7)
// Pet:  y = 8..50  (Pikachu body center at CY=36)
// Menu: y = 51..63
constexpr int16_t TG_CX       = 64;
constexpr int16_t TG_CY       = 36;
constexpr int     TG_SEP      = 51;
constexpr int     TG_MENU_Y   = 54;

// ─── Decay rates (per second) ─────────────────────────────────────────────────
constexpr float TG_HUNGER_DECAY = 0.40f;
constexpr float TG_HAPPY_DECAY  = 0.27f;
constexpr float TG_ENERGY_DECAY = 0.33f;
constexpr float TG_HEALTH_DRAIN = 0.50f;
constexpr float TG_HEALTH_REGEN = 0.08f;
constexpr float TG_SLEEP_ENERGY = 50.0f;
constexpr float TG_SLEEP_HAPPY  = -8.0f;
constexpr float TG_SLEEP_SECS   = 2.5f;
constexpr float TG_ACTION_CDN   = 3.0f;

// ─── Menu ─────────────────────────────────────────────────────────────────────
static const int16_t     ACT_X[4]     = { 2, 33, 66, 100 };
static const char* const ACT_LABEL[4] = { "CIBO", "GIOCA", "DORMI", "CURA" };

// ─── Begin ────────────────────────────────────────────────────────────────────

void TamagotchiGame::begin() {
    _hunger         = 75.f;
    _happy          = 70.f;
    _energy         = 80.f;
    _health         = 100.f;
    _petState       = NORMAL;
    _selAction      = ACT_FEED;
    _actionCooldown = 0.f;
    _sleeping       = false;
    _sleepTimer     = 0.f;
    _lastShake      = false;
    _lastBtn        = false;
    _deadMs         = 0;
    _animFrame      = 0;
}

// ─── State machine ────────────────────────────────────────────────────────────

void TamagotchiGame::computePetState() {
    if (_health <= 0.f)  { _petState = DEAD;   return; }
    if (_health < 35.f)  { _petState = SICK;   return; }
    if (_sleeping)       { _petState = TIRED;  return; }
    if (_hunger  < 22.f) { _petState = HUNGRY; return; }
    if (_energy  < 22.f) { _petState = TIRED;  return; }
    if (_happy   < 22.f) { _petState = SAD;    return; }
    if (_hunger > 65.f && _happy > 65.f && _energy > 50.f && _health > 75.f)
                         { _petState = HAPPY;  return; }
    _petState = NORMAL;
}

void TamagotchiGame::doAction(Action a) {
    if (_petState == DEAD || _sleeping || _actionCooldown > 0.f) return;
    _actionCooldown = TG_ACTION_CDN;
    switch (a) {
        case ACT_FEED:
            if (_hunger < 92.f)
                _hunger = _hunger + 35.f > 100.f ? 100.f : _hunger + 35.f;
            break;
        case ACT_PLAY:
            if (_energy >= 20.f) {
                _happy  = _happy  + 28.f > 100.f ? 100.f : _happy  + 28.f;
                _energy = _energy - 18.f <   0.f ?   0.f : _energy - 18.f;
            }
            break;
        case ACT_SLEEP:
            _sleeping   = true;
            _sleepTimer = TG_SLEEP_SECS;
            break;
        case ACT_CURE:
            _health = _health + 40.f > 100.f ? 100.f : _health + 40.f;
            break;
        default: break;
    }
}

// ─── Main update ──────────────────────────────────────────────────────────────

bool TamagotchiGame::update(float dt, bool btnPressed, bool shake, Adafruit_SSD1306& d) {
    // ── Dead ────────────────────────────────────────────────────────────────
    if (_petState == DEAD) {
        if (!_deadMs) _deadMs = millis();
        const bool edge = btnPressed && !_lastBtn;
        _lastBtn = btnPressed;
        if (edge || millis() - _deadMs > 5000UL) return false;
        d.clearDisplay();
        d.setTextSize(2); d.setTextColor(SSD1306_WHITE);
        d.setCursor(22,  8); d.print("GAME");
        d.setCursor(22, 26); d.print("OVER");
        d.setTextSize(1);
        d.setCursor(10, 50); d.print("BTN: riprova");
        d.display();
        return true;
    }

    // ── Input ────────────────────────────────────────────────────────────────
    const bool btnEdge   = btnPressed && !_lastBtn;
    _lastBtn = btnPressed;
    if (btnEdge) _selAction = (Action)(((int)_selAction + 1) % ACT_COUNT);

    const bool shakeEdge = shake && !_lastShake;
    _lastShake = shake;
    if (shakeEdge) doAction(_selAction);

    // ── Sleep ────────────────────────────────────────────────────────────────
    if (_sleeping) {
        _sleepTimer -= dt;
        const float f = dt / TG_SLEEP_SECS;
        _energy = _energy + TG_SLEEP_ENERGY * f > 100.f ? 100.f : _energy + TG_SLEEP_ENERGY * f;
        const float dh = TG_SLEEP_HAPPY * f;
        _happy  = _happy + dh < 0.f ? 0.f : _happy + dh;
        if (_sleepTimer <= 0.f) _sleeping = false;
    }

    // ── Decay ────────────────────────────────────────────────────────────────
    if (!_sleeping) {
        _hunger -= TG_HUNGER_DECAY * dt;
        _happy  -= TG_HAPPY_DECAY  * dt;
        _energy -= TG_ENERGY_DECAY * dt;
    }
    if (_hunger < 0.f) _hunger = 0.f;
    if (_happy  < 0.f) _happy  = 0.f;
    if (_energy < 0.f) _energy = 0.f;

    // ── Health ───────────────────────────────────────────────────────────────
    if (_hunger < 5.f || _petState == SICK) {
        _health -= TG_HEALTH_DRAIN * dt;
        if (_health < 0.f) _health = 0.f;
    } else if (_health < 100.f && _hunger > 40.f && _happy > 40.f) {
        _health += TG_HEALTH_REGEN * dt;
        if (_health > 100.f) _health = 100.f;
    }

    if (_actionCooldown > 0.f) _actionCooldown -= dt;

    computePetState();

    _animFrame = (_animFrame + 1) & 63;

    // ── Render ───────────────────────────────────────────────────────────────
    d.clearDisplay();
    drawStatBars(d);
    drawPikachu(d, TG_CX, TG_CY);
    drawMenu(d);
    d.display();
    return true;
}

// ─── Stat bars ────────────────────────────────────────────────────────────────

void TamagotchiGame::drawStatBars(Adafruit_SSD1306& d) {
    d.setTextSize(1); d.setTextColor(SSD1306_WHITE);
    const struct { int16_t lx, bx; float val; char lbl; } b[4] = {
        {  0,  7, _hunger, 'H' },
        { 35, 42, _happy,  'P' },
        { 70, 77, _energy, 'E' },
        {105,112, _health, 'V' },
    };
    for (int i = 0; i < 4; i++) {
        d.setCursor(b[i].lx, 0); d.print(b[i].lbl);
        d.drawRect(b[i].bx, 1, 14, 5, SSD1306_WHITE);
        const int fill = (int)(b[i].val * 12.f / 100.f);
        if (fill > 0) d.fillRect(b[i].bx + 1, 2, fill, 3, SSD1306_WHITE);
    }
    d.drawFastHLine(0, 7, 128, SSD1306_WHITE);
}

// ─── Pikachu pixel-art ───────────────────────────────────────────────────────
//
// All coordinates are relative to (cx, cy) = center of Pikachu's body.
//
// Key anatomy (at cy=36):
//   Ears     y = cy-24..cy-14   (tips at cy-24, base at cy-14)
//   Head     fillCircle(cx, cy-7, 11)   [y: 18..46]
//   Body     fillCircle(cx, cy+5,  9)   [y: 31..50]
//   Eyes     fillCircle(cx±4, cy-11, 3) [centers at y=25]
//   Cheeks   fillCircle(cx±7, cy-3,  3) [centers at y=33]
//   Mouth    at cy-4..cy-3              [y=32..33]
//   Arms     fillRect at cy             [y=36..39]
//   Feet     fillRect at cy+13          [y=49..51]
//   Tail     zigzag from (cx+10, cy+2)  [y=32..44]

void TamagotchiGame::drawPikachu(Adafruit_SSD1306& d, int16_t cx, int16_t cy) {
    const uint8_t f = _animFrame;

    // ── Animation vertical/horizontal offsets ──────────────────────────────
    int8_t dy = 0, adx = 0;
    switch (_petState) {
        case HAPPY:
            // Bouncy jump (±2px, 8-frame period)
            dy = ((f >> 2) & 1) ? -2 : 0;
            break;
        case SICK:
            // Side wobble (±1px, 6-frame period)
            adx = (f % 6 < 3) ? -1 : 1;
            break;
        case DEAD:
        case SAD:
            break;
        default:
            // Gentle idle bob (±1px, 16-frame period)
            dy = ((f >> 3) & 1) ? -1 : 0;
            break;
    }
    cx += adx;
    cy += dy;

    // ── Ear tip positions (SAD ears droop outward) ─────────────────────────
    int16_t lTX = cx - 9,  lTY = cy - 24;
    int16_t rTX = cx + 9,  rTY = cy - 24;
    if (_petState == SAD) {
        lTX = cx - 15; lTY = cy - 18;
        rTX = cx + 15; rTY = cy - 18;
    }

    // ── 1. Ears (white, behind head) ───────────────────────────────────────
    d.fillTriangle(cx - 4, cy - 14, cx - 11, cy - 14, lTX, lTY, SSD1306_WHITE);
    d.fillTriangle(cx + 4, cy - 14, cx + 11, cy - 14, rTX, rTY, SSD1306_WHITE);

    // ── 2. Body + head + neck ──────────────────────────────────────────────
    d.fillCircle(cx, cy + 5, 9,  SSD1306_WHITE);
    d.fillCircle(cx, cy - 7, 11, SSD1306_WHITE);
    d.fillRect  (cx - 6, cy - 7, 12, 12, SSD1306_WHITE);

    // ── 3. Black ear tips ──────────────────────────────────────────────────
    if (_petState == SAD) {
        d.fillTriangle(lTX, lTY, lTX + 4, lTY + 5, lTX - 2, lTY + 5, SSD1306_BLACK);
        d.fillTriangle(rTX, rTY, rTX - 4, rTY + 5, rTX + 2, rTY + 5, SSD1306_BLACK);
    } else {
        d.fillTriangle(cx - 9, cy - 24, cx - 7, cy - 18, cx - 12, cy - 18, SSD1306_BLACK);
        d.fillTriangle(cx + 9, cy - 24, cx + 7, cy - 18, cx + 12, cy - 18, SSD1306_BLACK);
    }

    // ── 4. Lightning-bolt tail (right side, white on black bg) ────────────
    {
        const int16_t tx = cx + 10, ty = cy + 2;
        d.drawLine(tx,     ty,     tx + 4, ty - 5, SSD1306_WHITE);
        d.drawLine(tx + 4, ty - 5, tx + 2, ty + 2, SSD1306_WHITE);
        d.drawLine(tx + 2, ty + 2, tx + 6, ty - 2, SSD1306_WHITE);
        d.drawLine(tx + 6, ty - 2, tx + 4, ty + 6, SSD1306_WHITE);
        // Second pass to give 2px width
        d.drawLine(tx + 1, ty,     tx + 5, ty - 5, SSD1306_WHITE);
        d.drawLine(tx + 5, ty - 5, tx + 3, ty + 2, SSD1306_WHITE);
        d.drawLine(tx + 3, ty + 2, tx + 7, ty - 2, SSD1306_WHITE);
        d.drawLine(tx + 7, ty - 2, tx + 5, ty + 6, SSD1306_WHITE);
    }

    // ── 5. Cheeks (Pikachu's signature round spots) ────────────────────────
    d.fillCircle(cx - 7, cy - 3, 3, SSD1306_BLACK);
    d.fillCircle(cx + 7, cy - 3, 3, SSD1306_BLACK);
    // Central highlight dot to soften them
    d.drawPixel(cx - 7, cy - 3, SSD1306_WHITE);
    d.drawPixel(cx + 7, cy - 3, SSD1306_WHITE);

    // ── 6. Nose ────────────────────────────────────────────────────────────
    d.drawFastHLine(cx - 1, cy - 9, 2, SSD1306_BLACK);

    // ── 7. Eyes ────────────────────────────────────────────────────────────
    const bool blink = (_petState == NORMAL || _petState == HAPPY) && (f > 58);

    switch (_petState) {
        case TIRED:
            // Closed: thick downward arc (sleeping / exhausted)
            d.drawFastHLine(cx - 6, cy - 12, 4, SSD1306_BLACK);
            d.drawFastHLine(cx - 5, cy - 11, 2, SSD1306_BLACK);
            d.drawFastHLine(cx + 2, cy - 12, 4, SSD1306_BLACK);
            d.drawFastHLine(cx + 3, cy - 11, 2, SSD1306_BLACK);
            break;

        case SICK:
        case DEAD:
            // X X eyes
            d.drawLine(cx - 7, cy - 14, cx - 1, cy - 8, SSD1306_BLACK);
            d.drawLine(cx - 1, cy - 14, cx - 7, cy - 8, SSD1306_BLACK);
            d.drawLine(cx + 1, cy - 14, cx + 7, cy - 8, SSD1306_BLACK);
            d.drawLine(cx + 7, cy - 14, cx + 1, cy - 8, SSD1306_BLACK);
            break;

        case SAD:
            // Normal eyes + angled eyebrows (inner corners raised = sad look)
            d.fillCircle(cx - 4, cy - 11, 3, SSD1306_BLACK);
            d.drawPixel (cx - 3, cy - 12, SSD1306_WHITE);
            d.fillCircle(cx + 4, cy - 11, 3, SSD1306_BLACK);
            d.drawPixel (cx + 5, cy - 12, SSD1306_WHITE);
            // Sad eyebrows: tilt toward nose
            d.drawLine(cx - 6, cy - 15, cx - 2, cy - 14, SSD1306_BLACK);
            d.drawLine(cx + 2, cy - 14, cx + 6, cy - 15, SSD1306_BLACK);
            break;

        default:
            if (blink) {
                // Blink: closed lines
                d.drawFastHLine(cx - 6, cy - 10, 4, SSD1306_BLACK);
                d.drawFastHLine(cx + 2, cy - 10, 4, SSD1306_BLACK);
            } else {
                // Open eyes: large black circles + white highlight pixel
                d.fillCircle(cx - 4, cy - 11, 3, SSD1306_BLACK);
                d.drawPixel (cx - 3, cy - 12, SSD1306_WHITE);
                d.fillCircle(cx + 4, cy - 11, 3, SSD1306_BLACK);
                d.drawPixel (cx + 5, cy - 12, SSD1306_WHITE);
            }
            break;
    }

    // ── 8. Mouth ───────────────────────────────────────────────────────────
    switch (_petState) {
        case HAPPY:
            // Wide open smile with white teeth strip
            d.drawPixel    (cx - 4, cy - 5,  SSD1306_BLACK);
            d.drawFastHLine(cx - 3, cy - 4,  6, SSD1306_BLACK);
            d.drawPixel    (cx + 3, cy - 5,  SSD1306_BLACK);
            d.fillRect     (cx - 2, cy - 4,  4, 2, SSD1306_BLACK);
            d.drawFastHLine(cx - 2, cy - 3,  4, SSD1306_WHITE);  // teeth line
            break;

        case HUNGRY: {
            // Chomping: open/close every 4 frames
            const bool open = (f & 4) != 0;
            if (open) {
                d.fillRect     (cx - 3, cy - 5, 6, 5, SSD1306_BLACK);
                d.drawFastHLine(cx - 2, cy - 3, 4, SSD1306_WHITE);  // teeth
            } else {
                d.drawFastHLine(cx - 3, cy - 4, 6, SSD1306_BLACK);
            }
            break;
        }

        case SAD:
            // Frown (inverted arc)
            d.drawPixel    (cx - 3, cy - 3, SSD1306_BLACK);
            d.drawFastHLine(cx - 2, cy - 4, 4, SSD1306_BLACK);
            d.drawPixel    (cx + 3, cy - 3, SSD1306_BLACK);
            break;

        case TIRED:
            d.drawFastHLine(cx - 2, cy - 4, 4, SSD1306_BLACK);
            break;

        case SICK:
            // Wavy nauseous line
            d.drawPixel(cx - 4, cy - 4, SSD1306_BLACK);
            d.drawPixel(cx - 2, cy - 5, SSD1306_BLACK);
            d.drawPixel(cx,     cy - 4, SSD1306_BLACK);
            d.drawPixel(cx + 2, cy - 5, SSD1306_BLACK);
            d.drawPixel(cx + 4, cy - 4, SSD1306_BLACK);
            break;

        case DEAD:
            d.drawFastHLine(cx - 4, cy - 4, 8, SSD1306_BLACK);
            break;

        default: // NORMAL
            // Gentle smile arc
            d.drawPixel    (cx - 3, cy - 5, SSD1306_BLACK);
            d.drawFastHLine(cx - 2, cy - 4, 4, SSD1306_BLACK);
            d.drawPixel    (cx + 3, cy - 5, SSD1306_BLACK);
            break;
    }

    // ── 9. Arms ────────────────────────────────────────────────────────────
    switch (_petState) {
        case HAPPY: {
            // Alternating raised arms
            const bool raisedL = ((f >> 2) & 1) != 0;
            const int8_t lOff = raisedL ? -3 : 0;
            const int8_t rOff = raisedL ?  0 : -3;
            d.fillRect(cx - 13, cy + lOff, 3, 5, SSD1306_WHITE);
            d.fillRect(cx + 10, cy + rOff, 3, 5, SSD1306_WHITE);
            break;
        }
        case SAD:
        case SICK:
            // Drooped arms
            d.fillRect(cx - 13, cy + 3, 3, 5, SSD1306_WHITE);
            d.fillRect(cx + 10, cy + 3, 3, 5, SSD1306_WHITE);
            break;
        case DEAD:
            // Arms flopped outward (wider)
            d.fillRect(cx - 14, cy + 4, 4, 3, SSD1306_WHITE);
            d.fillRect(cx + 10, cy + 4, 4, 3, SSD1306_WHITE);
            break;
        default:
            d.fillRect(cx - 13, cy,     3, 5, SSD1306_WHITE);
            d.fillRect(cx + 10, cy,     3, 5, SSD1306_WHITE);
            break;
    }

    // ── 10. Feet ────────────────────────────────────────────────────────────
    if (_petState != DEAD) {
        if (_petState == HAPPY) {
            // Walking: alternate feet up
            const bool stepL = ((f >> 2) & 1) != 0;
            d.fillRect(cx - 6, cy + 13 + (stepL ? -2 : 0), 5, 3, SSD1306_WHITE);
            d.fillRect(cx + 1, cy + 13 + (stepL ?  0 : -2), 5, 3, SSD1306_WHITE);
        } else {
            d.fillRect(cx - 6, cy + 13, 5, 3, SSD1306_WHITE);
            d.fillRect(cx + 1, cy + 13, 5, 3, SSD1306_WHITE);
        }
    }

    // ── 11. Sleep bubbles ───────────────────────────────────────────────────
    if (_sleeping) {
        d.setTextSize(1); d.setTextColor(SSD1306_WHITE);
        const int zc = ((f >> 4) % 3) + 1;   // 1..3 z's, cycles every ~1.5 s
        for (int i = 0; i < zc; i++) {
            d.setCursor(cx + 11 + i * 6, cy - 14 - i * 4);
            d.print('z');
        }
    }

    // ── 12. Sick sparks ─────────────────────────────────────────────────────
    if (_petState == SICK) {
        // Small rotating dots around head to show dizziness
        const uint8_t angle = f & 7;
        const int16_t sparks[8][2] = {
            {cx,            (int16_t)(cy-20)}, {(int16_t)(cx+7),  (int16_t)(cy-19)},
            {(int16_t)(cx+11),(int16_t)(cy-14)},{(int16_t)(cx+11),(int16_t)(cy-8)},
            {(int16_t)(cx+7),(int16_t)(cy-4)}, {cx,               (int16_t)(cy-4)},
            {(int16_t)(cx-7),(int16_t)(cy-4)}, {(int16_t)(cx-11), (int16_t)(cy-8)}
        };
        d.drawPixel(sparks[angle][0],       sparks[angle][1],       SSD1306_WHITE);
        d.drawPixel(sparks[(angle+4) & 7][0], sparks[(angle+4) & 7][1], SSD1306_WHITE);
    }
}

// ─── Action menu ──────────────────────────────────────────────────────────────

void TamagotchiGame::drawMenu(Adafruit_SSD1306& d) {
    d.drawFastHLine(0, TG_SEP, 128, SSD1306_WHITE);
    d.setTextSize(1);
    const bool active = !_sleeping && _petState != DEAD;
    for (int i = 0; i < (int)ACT_COUNT; i++) {
        const bool sel = active && (i == (int)_selAction);
        const int16_t x = ACT_X[i];
        const int16_t w = (int16_t)(strlen(ACT_LABEL[i]) * 6);
        if (sel) {
            d.fillRect(x - 1, TG_MENU_Y - 1, w + 2, 9, SSD1306_WHITE);
            d.setTextColor(SSD1306_BLACK);
        } else {
            d.setTextColor(SSD1306_WHITE);
        }
        d.setCursor(x, TG_MENU_Y); d.print(ACT_LABEL[i]);
    }
    d.setTextColor(SSD1306_WHITE);
}
