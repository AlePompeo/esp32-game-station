#include "ArduRacerGame.h"
#include <Arduino.h>
#include <math.h>
#include <string.h>
#include <stdio.h>

// ─── Tile codes (from ArduRacerFx/fxdata/fxdata-build.py) ─────────────────────
//  0  = H rettilineo          1  = V rettilineo
//  2  = angolo W↔S            3  = angolo W↔N
//  4  = angolo E↔N            5  = angolo E↔S
//  6..11 = diagonali/curve road   12..15 = erba (off-road)
//  16..19 = diagonali road    20..23 = erba (off-road)
//  24 = partenza H            25 = partenza V
//  26 = checkpoint H          27 = checkpoint V

// ─── Level maps (10×10) estratti da fxdata-data.bin ───────────────────────────
static const uint8_t LEVELS[10][100] = {
  { // Level 1
    21, 5,16,12,21,21,21,21,21,21,
    21,27,15, 6,12,21,21,21,21,21,
    21, 1,21,15, 6,12,21,21,21,21,
    21, 1,21,21,15, 6,12,21,21,21,
    21,27,21,21,21,15, 6,12,21,21,
    20, 1,20,20,20,20,15, 6,12,20,
    20, 1,20,20,20,20,20,15,11,20,
    20,27,20,20,20,20,20,13,19,20,
    20, 4, 0,24, 0, 0,26, 8,14,20,
    20,20,20,20,20,20,20,20,20,20,
  },
  { // Level 2
    21,21,21,21,20,21,21,21,21,21,
    21,13,10, 0,24, 0, 0,16,12,21,
    20,17,14,13, 7, 6,12,15,11,20,
    21, 1,13, 7,14,15, 6,12, 1,21,
    21, 1, 7,14,23,22,15, 6, 1,21,
    20,27,20,23,22,23,22,20,27,20,
    21, 1,23,22,23,22,23,22, 1,21,
    21, 9,12,20,22,23,20,13,19,21,
    21,15,18,26, 0, 0,26, 8,14,21,
    21,21,21,20,21,21,20,21,21,21,
  },
  { // Level 3
    22,22,22,22,22,22,22,22,22,22,
     5,26, 0, 0, 0, 0, 0,26, 2,23,
     4,26, 0, 0, 2,22,22,13,19,22,
    23,23,23,23, 1,23,23,17,14,23,
    22,22,22,22,27,22,22, 9,12,22,
    23,23,23,23, 1,23,23,15,11,23,
    22, 5, 0, 0, 3,22,22,22,25,22,
    23, 1,23,15, 6,12,23,13,19,23,
    22, 4, 0, 0, 0,26, 0, 8,14,22,
    23,23,23,23,23,23,23,23,23,23,
  },
  { // Level 4
    22,22,13,10,26, 0, 0, 0,26, 2,
    22,13, 7,14,22,22,22,22,22, 1,
    13, 7,14,22,22, 5, 2,22,22,27,
    17,14,22,22,22, 4, 3,22, 5, 3,
    27,22,22,22,22,22,22, 5, 3,22,
     1, 5,26, 0, 0, 0,26, 3,22,22,
     1, 4, 0,24, 0, 0, 0,26, 2,22,
    27,22,22,22,22,22,22,22, 1,22,
     9,12,22,22,22,22,22,22, 1,22,
    15,18,26, 0, 0, 0, 0,26, 3,22,
  },
  { // Level 5
     5,26, 0, 0, 0, 0, 0, 0,26, 2,
     4, 0, 2,23,23,21,23,23,21, 1,
    20,20, 1,23,23,21,23,21,23,25,
    20,20, 1,12,23,21,21,23,23, 1,
    20,20,27, 6,12,21,23,21,23, 1,
    20,20, 1,15, 6,21,23,23,21, 1,
    20,20, 1,23,15, 6,12,23,23, 1,
    20,20, 1,23,23,15, 6,12,23, 1,
     5, 0, 3,23,23,23,15, 6,12,27,
     4, 0, 0, 0, 0, 0, 0, 0, 0, 3,
  },
  { // Level 6
    22,22,22,22,22,22,22,22,22,22,
     5,26, 0, 0, 0, 0, 0,26, 2,22,
     1,23,23,23,23,23,23,23, 1,22,
     1,23,23, 5, 0, 2, 5,26, 3,22,
    27,23,23,25,21, 1, 4, 0, 0, 2,
     9,12,23, 1,21, 1,20,20,20, 1,
    15, 6,12, 1,21, 1,20,20,20, 1,
    22,15,11, 1,21, 1,20,20,20, 1,
    22,22,27,27,21, 1,20,20,20,27,
    22,22, 9,19,21, 4,26, 0, 0, 3,
  },
  { // Level 7
     5, 0, 0, 0, 0, 0, 2,23,23,23,
    25,21,22,22,22,21,27,23,23,23,
     1,21,21,22,22,21, 9,10, 0, 2,
     1,21,22,21,22,21,15,18, 2, 1,
     1,21,22,22,21,21,22,13,19, 1,
     1,22,22,22,22,13,10, 8,14, 1,
     1,22,22,22,13, 7,18, 0, 0, 3,
     1,22,22,13, 7,14,23,23,23,23,
     9,12,13, 7,14,23,23,23,23,23,
    15,18, 8,14,23,23,23,23,23,23,
  },
  { // Level 8
    13,10,24,16,12,23,23,23,23,23,
    15,11,23,15, 6,12,23,23,23,23,
    13,19,23,23,15, 6,12,23,23,23,
    17,14,22,23,23,15, 6,12,23,23,
     9,12, 5, 0,16,12,15, 6,12,23,
    15,11, 9,12,15, 6,12,15, 6,12,
    23,27,15, 6,12,15, 6,12,15,11,
    23, 1,23,15,11,23,15,11,23, 1,
    23, 1,23,23,27,20,20,27,23,27,
    23, 4,26, 0, 3,23,23, 4, 0, 3,
  },
  { // Level 9
    13,10,16,12,22,22,22,22,22,22,
    17,18, 8,11,22,22, 5,26, 2,22,
     1,22,22, 9,12,13,19,22, 9,12,
    27,22,22,15,18, 8,14,22,15,11,
     9,12,22,22,20,20,20,20,20,27,
    17,18, 0, 2,20,22,22,22,22, 1,
     1,22,22, 1,20,20,20,22,22, 1,
     1,22,22, 1,20,22,22,22,22, 1,
     1,22, 5, 3,20,20,20,20,20, 1,
     4, 0, 4, 0,24, 0, 0, 0,26, 3,
  },
  { // Level 10
    22,13,10, 0, 0, 2,20, 5, 2,22,
    13, 7,14,22,22, 1,20,27, 4, 2,
    17,14,22,22,22,27,20, 1,22, 1,
    25,22,22, 5, 0, 3,20, 1,22, 1,
     1,22,22, 9,10, 0,26, 3,22, 1,
     1,22,22,17, 6,12,20,22,22, 1,
     1,22,22, 1,15, 6,12,20,22, 1,
     1,22,13,19,21,15, 6,12,20, 1,
    27,13, 7,14,21,20,15, 6,12,27,
     4, 8,14,20,21,20,21,15,18, 3,
  },
};

// ─── Public API ───────────────────────────────────────────────────────────────

void ArduRacerGame::begin() {
    memset(_bestLaps, 0, sizeof(_bestLaps));
    _mode      = Mode::TITLE;
    _modeMs    = 0;
    _level     = 1;
    // Evita edge spurio dal btn/shake usato per lanciare il gioco dal menu
    _prevBtn   = true;
    _prevShake = true;
    resetRace();
}

bool ArduRacerGame::update(float dt, float gravX, float gravY,
                            bool shake, bool btnPressed, bool btnHeld,
                            Adafruit_SSD1306& d) {
    (void)gravY;
    _modeMs += (uint32_t)(dt * 1000.0f);

    const bool shakeEdge = shake && !_prevShake;
    const bool btnEdge   = btnPressed && !_prevBtn;
    _prevShake = shake;
    _prevBtn   = btnPressed;

    switch (_mode) {

        case Mode::TITLE:
            if (_modeMs > 2500 || btnEdge || shakeEdge) {
                _mode   = Mode::SELECT;
                _modeMs = 0;
            }
            renderTitle(d);
            break;

        case Mode::SELECT:
            if (btnEdge)    _level = (_level % NUM_LEVELS) + 1;
            if (shakeEdge) {
                resetRace();
                _mode   = Mode::RACING;
                _modeMs = 0;
            }
            renderSelect(d);
            break;

        case Mode::RACING:
            doPhysics(dt, gravX, btnHeld);
            checkTileEvents();
            if (_lapRunning) _lapCurrent = millis() - _lapStart;
            if (shakeEdge) {
                resetRace();
                _mode   = Mode::RACING;
                _modeMs = 0;
            }
            renderRace(d);
            break;
    }
    return true;
}

// ─── Internal helpers ─────────────────────────────────────────────────────────

void ArduRacerGame::resetRace() {
    _levelSize = 10;
    _chkTotal  = 0;
    _lastTile  = 24;

    // Find start tile and count checkpoints
    for (int y = 0; y < _levelSize; y++) {
        for (int x = 0; x < _levelSize; x++) {
            uint8_t t = getTile((uint8_t)x, (uint8_t)y);
            if (t == 24) {
                _carX    = x * (float)TILE_SZ + TILE_SZ * 0.5f;
                _carY    = y * (float)TILE_SZ + TILE_SZ * 0.5f;
                _carRot  = (float)M_PI * 0.5f;  // est (→)
                _lastTile = 24;
            } else if (t == 25) {
                _carX    = x * (float)TILE_SZ + TILE_SZ * 0.5f;
                _carY    = y * (float)TILE_SZ + TILE_SZ * 0.5f;
                _carRot  = 0.0f;  // nord (↑)
                _lastTile = 25;
            }
            if (t == 26 || t == 27) _chkTotal++;
        }
    }
    if (_chkTotal == 0) _chkTotal = 1;

    _carSpeed   = 0.0f;
    _offRoad    = false;
    _lapRunning = false;
    _lapStart   = 0;
    _lapCurrent = 0;
    _curLap     = 0;
    _chkPassed  = 0;
    _newBest    = false;
    _newBestMs  = 0;
}

void ArduRacerGame::doPhysics(float dt, float gravX, bool btnHeld) {
    // Sterzo proporzionale: dead zone 6 m/s², pieno a 14 m/s²
    if (fabsf(gravX) > STEER_THRESH) {
        float str = constrain((fabsf(gravX) - STEER_THRESH) / 8.0f, 0.0f, 1.0f);
        float dir = (gravX > 0) ? 1.0f : -1.0f;
        _carRot += dir * TURN_RATE * str * dt;
    }
    while (_carRot < 0.0f)               _carRot += 2.0f * (float)M_PI;
    while (_carRot >= 2.0f * (float)M_PI) _carRot -= 2.0f * (float)M_PI;

    // Accelerazione solo con btn.held() (come A/B nell'originale)
    if (btnHeld) {
        float maxSpd = _offRoad ? OFFROAD_MAX : MAX_SPEED;
        _carSpeed += ACCEL_RATE * dt;
        if (_carSpeed > maxSpd) _carSpeed = maxSpd;
    }

    // Decelerazione fuoristrada
    if (_offRoad && _carSpeed > OFFROAD_MAX) {
        _carSpeed -= OFFROAD_DEC * dt;
        if (_carSpeed < OFFROAD_MAX) _carSpeed = OFFROAD_MAX;
    }

    // Attrito (se non si accelera)
    if (!btnHeld && _carSpeed > 0.0f) {
        _carSpeed -= DRAG_RATE * dt;
        if (_carSpeed < 0.0f) _carSpeed = 0.0f;
    }

    _carX += sinf(_carRot) * _carSpeed * dt;
    _carY -= cosf(_carRot) * _carSpeed * dt;

    const float worldMax = (float)(_levelSize * TILE_SZ) - 1.0f;
    if (_carX < 0.0f)     { _carX = 0.0f;     _carSpeed *= 0.4f; }
    if (_carY < 0.0f)     { _carY = 0.0f;     _carSpeed *= 0.4f; }
    if (_carX > worldMax) { _carX = worldMax; _carSpeed *= 0.4f; }
    if (_carY > worldMax) { _carY = worldMax; _carSpeed *= 0.4f; }

    uint8_t tx = (uint8_t)((uint16_t)_carX / TILE_SZ);
    uint8_t ty = (uint8_t)((uint16_t)_carY / TILE_SZ);
    _offRoad = !isRoad(getTile(tx, ty));
}

void ArduRacerGame::checkTileEvents() {
    uint8_t tx  = (uint8_t)((uint16_t)_carX / TILE_SZ);
    uint8_t ty  = (uint8_t)((uint16_t)_carY / TILE_SZ);
    uint8_t cur = getTile(tx, ty);

    if (cur == _lastTile) return;

    if ((cur == 26 || cur == 27) && _lapRunning) {
        _chkPassed++;
    }

    // L'auto ha lasciato la casella di partenza
    if (_lastTile == 24 || _lastTile == 25) {
        if (!_lapRunning) {
            _lapRunning = true;
            _lapStart   = millis();
            _chkPassed  = 0;
        } else if (_chkPassed >= _chkTotal) {
            uint32_t lapTime = millis() - _lapStart;
            uint32_t& best   = _bestLaps[_level - 1];
            if (best == 0 || lapTime < best) {
                best       = lapTime;
                _newBest   = true;
                _newBestMs = millis();
            }
            _curLap++;
            _lapStart  = millis();
            _chkPassed = 0;
        }
    }

    _lastTile = cur;
}

uint8_t ArduRacerGame::getTile(uint8_t tx, uint8_t ty) const {
    if (tx >= _levelSize || ty >= _levelSize) return 20;
    return LEVELS[_level - 1][ty * _levelSize + tx];
}

bool ArduRacerGame::isRoad(uint8_t t) const {
    // Road: 0-11, 16-19, 24-27  (da buildMapCache in racer.cpp)
    return (t <= 11) || (t >= 16 && t <= 19) || (t >= 24 && t <= 27);
}

void ArduRacerGame::fmtTime(char* buf, uint32_t ms) const {
    uint32_t sec  = ms / 1000;
    uint32_t frac = (ms % 1000) / 10;
    snprintf(buf, 10, "%u.%02u", (unsigned)sec, (unsigned)frac);
}

// ─── Rendering ────────────────────────────────────────────────────────────────

void ArduRacerGame::renderTitle(Adafruit_SSD1306& d) {
    d.clearDisplay();
    d.setTextColor(SSD1306_WHITE);
    d.setTextSize(2);
    d.setCursor(4, 4);
    d.print("ARDURACER");
    d.drawFastHLine(0, 24, 128, SSD1306_WHITE);
    d.setTextSize(1);
    d.setCursor(4, 30);
    d.print("Tieni BTN: accelera");
    d.setCursor(4, 40);
    d.print("Inclina: sterzo");
    d.setCursor(4, 50);
    d.print("BTN:livello SHAKE:via!");
    d.display();
}

void ArduRacerGame::renderSelect(Adafruit_SSD1306& d) {
    d.clearDisplay();
    d.setTextColor(SSD1306_WHITE);

    d.setTextSize(1);
    d.setCursor(19, 0);
    d.print("SCEGLI CIRCUITO");
    d.drawFastHLine(0, 9, 128, SSD1306_WHITE);

    d.setTextSize(2);
    char lname[12];
    snprintf(lname, sizeof(lname), "LVL %u", (unsigned)_level);
    int16_t tw = (int16_t)(strlen(lname) * 12);
    d.setCursor((128 - tw) / 2, 14);
    d.print(lname);

    d.setTextSize(1);
    char buf[20];
    snprintf(buf, sizeof(buf), "Livello %u / %u", (unsigned)_level, (unsigned)NUM_LEVELS);
    tw = (int16_t)(strlen(buf) * 6);
    d.setCursor((128 - tw) / 2, 34);
    d.print(buf);

    uint32_t best = _bestLaps[_level - 1];
    if (best > 0) {
        char bt[10]; fmtTime(bt, best);
        snprintf(buf, sizeof(buf), "Best: %ss", bt);
        tw = (int16_t)(strlen(buf) * 6);
        d.setCursor((128 - tw) / 2, 44);
        d.print(buf);
    }

    d.drawFastHLine(0, 54, 128, SSD1306_WHITE);
    d.setCursor(4, 57);
    d.print("BTN:cambia  SHAKE:via!");
    d.display();
}

void ArduRacerGame::renderRace(Adafruit_SSD1306& d) {
    d.clearDisplay();

    // Camera centrata sull'auto
    int16_t camX = (int16_t)_carX - 64;
    int16_t camY = (int16_t)_carY - 27;
    const int16_t wMax = (int16_t)(_levelSize * TILE_SZ) - 128;
    const int16_t hMax = (int16_t)(_levelSize * TILE_SZ) - 54;
    if (camX < 0)    camX = 0;
    if (camY < 0)    camY = 0;
    if (camX > wMax) camX = wMax;
    if (camY > hMax) camY = hMax;

    // Disegna tile visibili
    int16_t firstTX = camX / TILE_SZ;
    int16_t firstTY = camY / TILE_SZ;
    int16_t lastTX  = (camX + 127) / TILE_SZ + 1;
    int16_t lastTY  = (camY +  63) / TILE_SZ + 1;
    if (lastTX >= _levelSize) lastTX = _levelSize - 1;
    if (lastTY >= _levelSize) lastTY = _levelSize - 1;

    for (int16_t ty = firstTY; ty <= lastTY; ty++) {
        for (int16_t tx = firstTX; tx <= lastTX; tx++) {
            drawTile(d,
                     (int16_t)(tx * TILE_SZ - camX),
                     (int16_t)(ty * TILE_SZ - camY),
                     getTile((uint8_t)tx, (uint8_t)ty));
        }
    }

    // Pass 2: bridge the single-pixel gaps where diagonal tiles meet through grass corners
    {
        constexpr int16_t H = (int16_t)TILE_SZ / 2;
        for (int16_t ty = firstTY; ty <= lastTY; ty++) {
            for (int16_t tx = firstTX; tx <= lastTX; tx++) {
                uint8_t t = getTile((uint8_t)tx, (uint8_t)ty);
                int16_t sx = (int16_t)(tx * TILE_SZ) - camX;
                int16_t sy = (int16_t)(ty * TILE_SZ) - camY;

                // \ diagonals (6,8,11,19): fill corners toward SE neighbor
                if (t==6||t==8||t==11||t==19) {
                    uint8_t se = getTile((uint8_t)(tx+1), (uint8_t)(ty+1));
                    if (se==6||se==8||se==11||se==19) {
                        // SW corner of NE grass tile
                        d.fillTriangle(sx+(int16_t)TILE_SZ, sy+H,
                                       sx+(int16_t)TILE_SZ, sy+(int16_t)TILE_SZ,
                                       sx+(int16_t)TILE_SZ+H, sy+(int16_t)TILE_SZ, SSD1306_BLACK);
                        // NE corner of SW grass tile
                        d.fillTriangle(sx+H, sy+(int16_t)TILE_SZ,
                                       sx+(int16_t)TILE_SZ, sy+(int16_t)TILE_SZ,
                                       sx+(int16_t)TILE_SZ, sy+(int16_t)TILE_SZ+H, SSD1306_BLACK);
                    }
                }

                // / diagonals (7,9,10,16,17,18): fill corners toward SW neighbor
                if (t==7||t==9||t==10||t==16||t==17||t==18) {
                    uint8_t sw = getTile((uint8_t)(tx-1), (uint8_t)(ty+1));
                    if (sw==7||sw==9||sw==10||sw==16||sw==17||sw==18) {
                        // SE corner of NW grass tile
                        d.fillTriangle(sx-H, sy+(int16_t)TILE_SZ,
                                       sx,   sy+(int16_t)TILE_SZ,
                                       sx,   sy+H, SSD1306_BLACK);
                        // NW corner of SE grass tile
                        d.fillTriangle(sx,   sy+(int16_t)TILE_SZ,
                                       sx+H, sy+(int16_t)TILE_SZ,
                                       sx,   sy+(int16_t)TILE_SZ+H, SSD1306_BLACK);
                    }
                }
            }
        }
    }

    // Auto
    drawCar(d, (int16_t)_carX - camX, (int16_t)_carY - camY);

    // Minimap (angolo basso-destra, 20×20 = 2px/tile × 10 tile)
    const int16_t mmX = 106;
    const int16_t mmY = 33;
    drawMinimap(d, mmX, mmY);

    // HUD strip (y=54..63)
    d.fillRect(0, 54, 106, 10, SSD1306_BLACK);
    d.drawFastHLine(0, 53, 128, SSD1306_WHITE);

    d.setTextSize(1);
    d.setTextColor(SSD1306_WHITE);

    char timeBuf[10]; fmtTime(timeBuf, _lapCurrent);
    char left[20];
    snprintf(left, sizeof(left), "L%u %ss", (unsigned)(_curLap + 1), timeBuf);
    d.setCursor(1, 56);
    d.print(left);

    uint32_t best = _bestLaps[_level - 1];
    bool showRecord = _newBest && (millis() - _newBestMs < 2000);
    if (best > 0 && !showRecord) {
        char bt[10]; fmtTime(bt, best);
        char right[14]; snprintf(right, sizeof(right), "B%ss", bt);
        int16_t rw = (int16_t)(strlen(right) * 6);
        d.setCursor(105 - rw, 56);
        d.print(right);
    } else if (showRecord) {
        d.setCursor(42, 56);
        d.print("RECORD!");
    }

    // Barra velocità (1px, sopra il separatore HUD)
    if (_carSpeed > 0.5f) {
        uint8_t bl = (uint8_t)((_carSpeed / MAX_SPEED) * 105.0f);
        d.drawFastHLine(0, 52, bl, SSD1306_WHITE);
    }

    d.display();
}

void ArduRacerGame::drawTile(Adafruit_SSD1306& d, int16_t sx, int16_t sy, uint8_t t) const {
    if (sx + (int16_t)TILE_SZ <= 0 || sx >= 128 ||
        sy + (int16_t)TILE_SZ <= 0 || sy >= 64) return;

    if (!isRoad(t)) {
        // Erba = bianco
        d.fillRect(sx, sy, TILE_SZ, TILE_SZ, SSD1306_WHITE);
        return;
    }

    // Tile di strada: sfondo già nero (clearDisplay), disegno solo le spalle bianche
    // SH=12 = larghezza spalla; RD=52 = offset bordo opposto
    constexpr int16_t SH = 12;
    constexpr int16_t RD = TILE_SZ - SH;  // 52

    switch (t) {
        // ─ Retto orizzontale ──────────────────────────────────────────────
        case 0:
            d.fillRect(sx, sy,    TILE_SZ, SH, SSD1306_WHITE);  // spalla top
            d.fillRect(sx, sy+RD, TILE_SZ, SH, SSD1306_WHITE);  // spalla bottom
            break;

        // ─ Retto verticale ────────────────────────────────────────────────
        case 1:
            d.fillRect(sx,    sy, SH, TILE_SZ, SSD1306_WHITE);  // spalla left
            d.fillRect(sx+RD, sy, SH, TILE_SZ, SSD1306_WHITE);  // spalla right
            break;

        // ─ Diagonale \ (NW-SE): erba negli angoli NE e SW ────────────────
        case 6: case 8: case 11: case 19:
            d.fillTriangle(sx+32, sy,      sx+64, sy,      sx+64, sy+32, SSD1306_WHITE); // NE
            d.fillTriangle(sx,    sy+32,   sx,    sy+64,   sx+32, sy+64, SSD1306_WHITE); // SW
            break;

        // ─ Diagonale / (NE-SW): erba negli angoli NW e SE ────────────────
        case 7: case 9: case 10: case 16: case 17: case 18:
            d.fillTriangle(sx, sy,      sx+32, sy,      sx, sy+32,      SSD1306_WHITE); // NW
            d.fillTriangle(sx+32, sy+64, sx+64, sy+64,  sx+64, sy+32,   SSD1306_WHITE); // SE
            break;

        // ─ Angolo W↔S (strada a sinistra+basso, erba in alto e a destra) ─
        case 2:
            d.fillRect(sx,    sy,       TILE_SZ, SH, SSD1306_WHITE);  // top
            d.fillRect(sx+RD, sy+SH,   SH, RD,  SSD1306_WHITE);      // right
            d.fillRect(sx,    sy+RD,   SH, SH,  SSD1306_WHITE);      // patch BL
            break;

        // ─ Angolo W↔N (strada a sinistra+alto, erba in basso e a destra) ─
        case 3:
            d.fillRect(sx,    sy+RD,  TILE_SZ, SH, SSD1306_WHITE);  // bottom
            d.fillRect(sx+RD, sy,     SH, RD,  SSD1306_WHITE);      // right
            d.fillRect(sx,    sy,     SH, SH,  SSD1306_WHITE);      // patch TL
            break;

        // ─ Angolo E↔N (strada a destra+alto, erba in basso e a sinistra) ─
        case 4:
            d.fillRect(sx,    sy+RD, TILE_SZ, SH, SSD1306_WHITE);  // bottom
            d.fillRect(sx,    sy,    SH, RD,  SSD1306_WHITE);      // left
            d.fillRect(sx+RD, sy,    SH, SH,  SSD1306_WHITE);      // patch TR
            break;

        // ─ Angolo E↔S (strada a destra+basso, erba in alto e a sinistra) ─
        case 5:
            d.fillRect(sx, sy,    TILE_SZ, SH, SSD1306_WHITE);  // top
            d.fillRect(sx, sy+SH, SH, RD,  SSD1306_WHITE);     // left
            d.fillRect(sx+RD, sy+RD, SH, SH, SSD1306_WHITE);  // patch BR
            break;

        // ─ Partenza orizzontale ───────────────────────────────────────────
        case 24:
            d.fillRect(sx,    sy,    TILE_SZ, SH, SSD1306_WHITE);   // top
            d.fillRect(sx,    sy+RD, TILE_SZ, SH, SSD1306_WHITE);   // bottom
            d.fillRect(sx+28, sy+SH, 8, 40,  SSD1306_WHITE);        // linea partenza
            break;

        // ─ Partenza verticale ─────────────────────────────────────────────
        case 25:
            d.fillRect(sx,    sy, SH, TILE_SZ, SSD1306_WHITE);   // left
            d.fillRect(sx+RD, sy, SH, TILE_SZ, SSD1306_WHITE);   // right
            d.fillRect(sx+SH, sy+28, 40, 8, SSD1306_WHITE);      // linea partenza
            break;

        // ─ Checkpoint orizzontale ─────────────────────────────────────────
        case 26:
            d.fillRect(sx,    sy,    TILE_SZ, SH, SSD1306_WHITE);   // top
            d.fillRect(sx,    sy+RD, TILE_SZ, SH, SSD1306_WHITE);   // bottom
            for (int16_t cy = sy + SH; cy < sy + RD; cy += 8)
                d.fillRect(sx + 28, cy, 4, 4, SSD1306_WHITE);
            break;

        // ─ Checkpoint verticale ───────────────────────────────────────────
        case 27:
            d.fillRect(sx,    sy, SH, TILE_SZ, SSD1306_WHITE);   // left
            d.fillRect(sx+RD, sy, SH, TILE_SZ, SSD1306_WHITE);   // right
            for (int16_t cx = sx + SH; cx < sx + RD; cx += 8)
                d.fillRect(cx, sy + 28, 4, 4, SSD1306_WHITE);
            break;
    }
}

void ArduRacerGame::drawCar(Adafruit_SSD1306& d, int16_t cx, int16_t cy) const {
    float s = sinf(_carRot), c = cosf(_carRot);
    // Vettore "avanti" (+7) e "dietro" (-5), larghezza half = 4
    int16_t fx = (int16_t)(s * 7.0f),  fy = (int16_t)(-c * 7.0f);
    int16_t bx = (int16_t)(-s * 5.0f), by = (int16_t)(c * 5.0f);
    int16_t px = (int16_t)(-c * 4.0f), py = (int16_t)(-s * 4.0f);

    // Triangolo bianco riempito: visibile su strada nera
    d.fillTriangle(
        cx + fx,        cy + fy,
        cx + bx + px,  cy + by + py,
        cx + bx - px,  cy + by - py,
        SSD1306_WHITE);
    // Contorno nero: visibile sull'erba bianca
    d.drawTriangle(
        cx + fx,        cy + fy,
        cx + bx + px,  cy + by + py,
        cx + bx - px,  cy + by - py,
        SSD1306_BLACK);
}

void ArduRacerGame::drawMinimap(Adafruit_SSD1306& d, int16_t ox, int16_t oy) const {
    const uint8_t ppt  = 2;         // 2px per tile
    const uint8_t mmSz = 10 * ppt;  // 20px

    d.fillRect(ox - 1, oy - 1, mmSz + 2, mmSz + 2, SSD1306_BLACK);
    d.drawRect(ox - 1, oy - 1, mmSz + 2, mmSz + 2, SSD1306_WHITE);

    for (uint8_t ty = 0; ty < 10; ty++) {
        for (uint8_t tx = 0; tx < 10; tx++) {
            if (isRoad(getTile(tx, ty))) {
                d.fillRect(ox + tx * ppt, oy + ty * ppt, ppt, ppt, SSD1306_WHITE);
            }
        }
    }

    // Posizione auto (pixel invertito)
    uint8_t cmx = (uint8_t)(_carX / TILE_SZ);
    uint8_t cmy = (uint8_t)(_carY / TILE_SZ);
    if (cmx < 10 && cmy < 10) {
        int16_t dotX = ox + cmx * ppt;
        int16_t dotY = oy + cmy * ppt;
        d.fillRect(dotX, dotY, ppt, ppt,
                   isRoad(getTile(cmx, cmy)) ? SSD1306_BLACK : SSD1306_WHITE);
    }
}
