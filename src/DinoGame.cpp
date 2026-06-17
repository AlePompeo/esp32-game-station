#include "DinoGame.h"
#include <Arduino.h>
#include <string.h>

// ─── Sprite data (Arduboy/SSD1306 page format) ────────────────────────────────
// Format: column-major within each page-row (8 vertical pixels per byte).
// Byte layout: bmp[page * width + col], bit-0 = top pixel of the page.
// These arrays were adapted from github.com/flaki/arduboy-rund-ino

// cactus_1  w:12  h:24  (3 pages × 12 cols = 36 bytes)
static const uint8_t PROGMEM spr_cactus[] = {
    0x00, 0x00, 0x00, 0x00, 0xfe, 0xff, 0xff, 0xfe, 0x00, 0xc0, 0xc0, 0x80,
    0xfe, 0xff, 0xfe, 0x00, 0xff, 0xff, 0xff, 0xff, 0xc0, 0xff, 0xff, 0x7f,
    0x01, 0x03, 0x03, 0x83, 0xff, 0xff, 0xff, 0xff, 0x80, 0x00, 0x00, 0x00
};
static constexpr uint8_t CACTUS_W = 12, CACTUS_H = 24;

// cloud_1  w:17  h:7  (1 page × 17 cols = 17 bytes)
static const uint8_t PROGMEM spr_cloud[] = {
    0x1c, 0x22, 0x22, 0x22, 0x24, 0x10, 0x12, 0x2a, 0x21,
    0x41, 0x41, 0x41, 0x42, 0x4a, 0x24, 0x24, 0x18
};
static constexpr uint8_t CLOUD_W = 17, CLOUD_H = 7;

// dino_top  w:20  h:18  (3 pages × 20 cols = 60 bytes)
static const uint8_t PROGMEM spr_dino_top[] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0xfe, 0xff, 0xfb, 0xff, 0xff, 0xbf, 0xbf, 0xbf, 0x3f, 0x3e,
    0x7e, 0xf8, 0xf0, 0xe0, 0xe0, 0xf0, 0xf8, 0xfc, 0xfe, 0xff,
    0xff, 0xff, 0xff, 0x7f, 0x04, 0x0c, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x01, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03,
    0x03, 0x03, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};
static constexpr uint8_t DINO_W = 20, DINO_H = 18;

// dino_leg_0  w:20  h:5  (standing still)
static const uint8_t PROGMEM spr_leg0[] = {
    0x00, 0x00, 0x00, 0x00, 0x01, 0x1f, 0x17, 0x03, 0x01, 0x03,
    0x1f, 0x10, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};
// dino_leg_1  w:20  h:5  (run frame A)
static const uint8_t PROGMEM spr_leg1[] = {
    0x00, 0x00, 0x00, 0x00, 0x01, 0x0f, 0x0b, 0x01, 0x01, 0x03,
    0x1f, 0x10, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};
// dino_leg_2  w:20  h:5  (run frame B)
static const uint8_t PROGMEM spr_leg2[] = {
    0x00, 0x00, 0x00, 0x00, 0x01, 0x1f, 0x17, 0x03, 0x01, 0x03,
    0x0f, 0x08, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};
static constexpr uint8_t LEG_W = 20, LEG_H = 5;

// dino_tumble  w:30  h:18  (death animation)
static const uint8_t PROGMEM spr_tumble[] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x7e, 0xf8, 0xf0, 0xe0, 0xe0, 0xf0, 0xf0, 0xf8, 0xf8, 0xf8,
    0xf8, 0xf0, 0xf0, 0xf0, 0xe0, 0xe0, 0xc0, 0xc0, 0x80, 0xc0,
    0xf0, 0xa8, 0xd8, 0xa8, 0xf8, 0xf8, 0xf8, 0xf8, 0xf8, 0xf0,
    0x00, 0x00, 0x01, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03,
    0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x01,
    0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x01
};
static constexpr uint8_t TUMBLE_W = 30, TUMBLE_H = 18;

// ─── Page-format bitmap renderer ─────────────────────────────────────────────
// Draws Arduboy/SSD1306-style bitmaps directly into the display buffer.
// Handles any (x, y) including negative and unaligned values.

void DinoGame::drawPageBitmap(Adafruit_SSD1306& d, int16_t x, int16_t y,
                               const uint8_t* bmp, uint8_t w, uint8_t h) const {
    if (x + w <= 0 || x >= 128 || y + h <= 0 || y >= 64) return;

    uint8_t* buf = d.getBuffer();
    const uint8_t srcPages = (h + 7) >> 3;
    // yShift: how many bits to shift within the destination page.
    // y & 7 is always in [0,7] for both positive and negative y in 2's complement.
    const uint8_t yShift = (uint8_t)(y & 7);

    for (uint8_t sp = 0; sp < srcPages; sp++) {
        // Destination page indices (can be negative, handled by bounds check)
        const int16_t dp0 = (y >> 3) + (int16_t)sp;
        const int16_t dp1 = dp0 + 1;

        for (uint8_t col = 0; col < w; col++) {
            const int16_t dc = x + (int16_t)col;
            if (dc < 0 || dc >= 128) continue;

            const uint8_t bits = pgm_read_byte(&bmp[sp * w + col]);
            if (!bits) continue;

            if (dp0 >= 0 && dp0 < 8)
                buf[dp0 * 128 + dc] |= (uint8_t)(bits << yShift);
            if (yShift > 0 && dp1 >= 0 && dp1 < 8)
                buf[dp1 * 128 + dc] |= (uint8_t)(bits >> (8 - yShift));
        }
    }
}

// ─── Jump physics (exact port of the Arduboy original at 60 fps) ──────────────

void DinoGame::advanceJump() {
    if (_jumpT == 0) return;
    _jumpT++;
    if      (_jumpT < 6)  _jumpH += 6;
    else if (_jumpT < 9)  _jumpH += 2;
    else if (_jumpT < 13) _jumpH += 1;
    else if (_jumpT == 16 || _jumpT == 18) _jumpH += 1;
    else if (_jumpT == 20 || _jumpT == 22) _jumpH -= 1;
    else if (_jumpT > 38) { _jumpH = 0; _jumpT = 0; _jumpAcc = 0.f; }
    else if (_jumpT > 32) _jumpH -= 6;
    else if (_jumpT > 29) _jumpH -= 2;
    else if (_jumpT > 25) _jumpH -= 1;
}

// ─── Scene rendering ──────────────────────────────────────────────────────────

void DinoGame::renderScene(Adafruit_SSD1306& d, bool tumble) const {
    const int16_t dy = DINO_BASE - _jumpH;

    // Cloud (parallax — draw before terrain)
    drawPageBitmap(d, (int16_t)_cloudX, _cloudY, spr_cloud, CLOUD_W, CLOUD_H);

    // Ground line (gap under dino while airborne)
    if (_jumpH > 4) {
        d.drawFastHLine(0, GROUND_Y, 128, SSD1306_WHITE);
    } else {
        d.drawFastHLine(0,         GROUND_Y, DINO_X,       SSD1306_WHITE);
        d.drawFastHLine(DINO_X + DINO_W, GROUND_Y, 128 - DINO_X - DINO_W, SSD1306_WHITE);
    }

    // Cactus(es)
    const int16_t cx = (int16_t)_cactX;
    drawPageBitmap(d, cx, DINO_BASE, spr_cactus, CACTUS_W, CACTUS_H);
    if (_cactType == 1)
        drawPageBitmap(d, cx + CACTUS_W + 2, DINO_BASE, spr_cactus, CACTUS_W, CACTUS_H);

    // Dino
    if (tumble) {
        drawPageBitmap(d, DINO_X, dy, spr_tumble, TUMBLE_W, TUMBLE_H);
    } else {
        drawPageBitmap(d, DINO_X, dy, spr_dino_top, DINO_W, DINO_H);
        const uint8_t* leg = (_jumpH > 0)      ? spr_leg0 :
                             (_legFrame == 1)   ? spr_leg1 : spr_leg2;
        drawPageBitmap(d, DINO_X, dy + DINO_H, leg, LEG_W, LEG_H);
    }
}

// ─── begin ────────────────────────────────────────────────────────────────────

void DinoGame::begin() {
    _state      = WAITING;
    _score      = 0;
    _speed      = 120.f;
    _scoreAcc   = 0.f;
    _jumpH      = 0;
    _jumpT      = 0;
    _jumpAcc    = 0.f;
    _legFrame   = 0;
    _legAcc     = 0.f;
    _cactX      = 160.f;
    _cactType   = 0;
    _cloudX     = 80.f;
    _cloudY     = 3;
    _deathTimer = 0.f;
    _lastBtn    = false;
    _lastShake  = false;
    // _bestScore preserved intentionally
}

// ─── update ───────────────────────────────────────────────────────────────────

bool DinoGame::update(float dt, bool btnPressed, bool shake, Adafruit_SSD1306& d) {
    const bool btnDown   = (btnPressed || shake) && !(_lastBtn || _lastShake);
    _lastBtn   = btnPressed;
    _lastShake = shake;

    // ── WAITING ───────────────────────────────────────────────────────────────
    if (_state == WAITING) {
        if (btnDown) _state = RUNNING;

        d.clearDisplay();
        renderScene(d, false);

        // "Dinosaur" title
        d.setTextSize(1); d.setTextColor(SSD1306_WHITE);
        d.setCursor(38, 0); d.print("DINOSAUR");

        // Blinking "press" prompt
        if ((millis() / 500) % 2 == 0) {
            d.setCursor(20, 10);
            d.print("BTN/SCUOTI: salta");
        }
        d.display();
        return true;
    }

    // ── DEAD ──────────────────────────────────────────────────────────────────
    if (_state == DEAD) {
        _deathTimer += dt;

        d.clearDisplay();
        renderScene(d, true);

        if (_deathTimer >= 1.2f) {
            // Game-over overlay
            d.fillRect(14, 14, 100, 36, SSD1306_BLACK);
            d.drawRect(14, 14, 100, 36, SSD1306_WHITE);
            d.drawRect(15, 15,  98, 34, SSD1306_WHITE);

            d.setTextSize(2); d.setTextColor(SSD1306_WHITE);
            d.setCursor(18, 17); d.print("GAME OVR");

            d.setTextSize(1);
            d.setCursor(18, 34);
            d.print("Punti:"); d.print(_score);
            if (_bestScore > 0) {
                d.setCursor(77, 34);
                d.print("R:"); d.print(_bestScore);
            }

            if (btnDown) return false;  // restart
        }
        d.display();
        return true;
    }

    // ── RUNNING ───────────────────────────────────────────────────────────────

    // Jump input
    if (btnDown && _jumpT == 0) {
        _jumpT   = 1;
        _jumpH   = 5;
        _jumpAcc = 0.f;
    }

    // Advance jump physics at 60 fps
    _jumpAcc += dt;
    while (_jumpAcc >= JUMP_INTERVAL && _jumpT > 0) {
        _jumpAcc -= JUMP_INTERVAL;
        advanceJump();
    }
    if (_jumpT == 0) _jumpAcc = 0.f;

    // Score and speed
    _scoreAcc += dt * 12.f;
    while (_scoreAcc >= 1.f) { _scoreAcc -= 1.f; _score++; }
    _speed = 120.f + _score * 0.4f;

    // Cactus movement and respawn
    _cactX -= _speed * dt;
    if (_cactX < -20.f) {
        _cactX    = 128.f + (float)random(0, 60);
        _cactType = (uint8_t)random(0, 2);
    }

    // Cloud parallax (much slower)
    _cloudX -= 20.f * dt;
    if (_cloudX < -CLOUD_W) {
        _cloudX = 128.f;
        _cloudY = (int8_t)random(1, 10);
    }

    // Leg animation (only while on ground)
    if (_jumpT == 0) {
        _legAcc += dt;
        if (_legAcc >= 0.12f) {
            _legAcc   = 0.f;
            _legFrame = (_legFrame == 1) ? 2 : 1;
        }
    } else {
        _legFrame = 0;
    }

    // Collision detection
    // Dino occupies: x=[DINO_X+2, DINO_X+16], body safe above SAFE_HEIGHT
    // Cactus x=[_cactX+1, _cactX+10]; double cactus extends by CACTUS_W+2
    const int16_t cactRight = (int16_t)_cactX + (_cactType == 1 ? CACTUS_W + 2 + CACTUS_W : CACTUS_W);
    const bool xOverlap = ((int16_t)_cactX + 1 < DINO_X + 16) &&
                          (cactRight          > DINO_X + 2);
    if (xOverlap && _jumpH < SAFE_HEIGHT) {
        _state      = DEAD;
        _deathTimer = 0.f;
        if (_score > _bestScore) _bestScore = _score;
    }

    // ── Render ────────────────────────────────────────────────────────────────
    d.clearDisplay();
    renderScene(d, false);

    // HUD: score
    d.setTextSize(1); d.setTextColor(SSD1306_WHITE);
    char buf[12];
    snprintf(buf, sizeof(buf), "%u", _score);
    d.setCursor(128 - (int16_t)(strlen(buf) * 6), 0);
    d.print(buf);

    if (_bestScore > 0 && _bestScore != _score) {
        snprintf(buf, sizeof(buf), "R:%u", _bestScore);
        d.setCursor(0, 0);
        d.print(buf);
    }

    d.display();
    return true;
}
