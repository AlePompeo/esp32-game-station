#include "SnakeGame.h"
#include <Arduino.h>
#include <string.h>

// ─── Constants ────────────────────────────────────────────────────────────────
constexpr int      SN_CELL        = 4;
constexpr int      SN_COLS        = 32;
constexpr int      SN_ROWS        = 13;
constexpr int      SN_Y_OFF       = 12;
constexpr int      SN_TILT_THRESH = 30;
constexpr uint32_t SN_MOVE_START  = 280;
constexpr uint32_t SN_MOVE_MIN    = 80;
constexpr uint32_t SN_MOVE_STEP   = 15;
constexpr int      SN_FOOD_ACCEL  = 5;
constexpr uint32_t SN_DEAD_MS     = 3000;

// ─── Helpers ──────────────────────────────────────────────────────────────────

uint32_t SnakeGame::moveInterval() const {
    const uint32_t reductions = (uint32_t)(_foodEaten / SN_FOOD_ACCEL) * SN_MOVE_STEP;
    return (SN_MOVE_START > reductions + SN_MOVE_MIN)
        ? SN_MOVE_START - reductions
        : SN_MOVE_MIN;
}

bool SnakeGame::bodyOccupies(int col, int row, int skipTail) const {
    const int limit = _len - skipTail;
    for (int i = 0; i < limit; i++)
        if (_body[i].col == (int8_t)col && _body[i].row == (int8_t)row) return true;
    return false;
}

void SnakeGame::spawnFood() {
    int col, row;
    do {
        col = (int)random(0, SN_COLS);
        row = (int)random(0, SN_ROWS);
    } while (bodyOccupies(col, row, 0));
    _food.col = (int8_t)col;
    _food.row = (int8_t)row;
}

// ─── Begin ────────────────────────────────────────────────────────────────────

void SnakeGame::begin() {
    _len        = 3;
    _dir        = RIGHT;
    _nextDir    = RIGHT;
    _score      = 0;
    _foodEaten  = 0;
    _state      = READY;
    _stateMs    = millis();
    _lastMoveMs = millis();

    const int8_t sc = (int8_t)(SN_COLS / 2);
    const int8_t sr = (int8_t)(SN_ROWS / 2);
    for (int i = 0; i < _len; i++) {
        _body[i].col = sc - (int8_t)i;
        _body[i].row = sr;
    }
    spawnFood();
}

// ─── Draw ─────────────────────────────────────────────────────────────────────

void SnakeGame::drawHud(Adafruit_SSD1306& d) const {
    d.setTextSize(1); d.setTextColor(SSD1306_WHITE);
    d.setCursor(0, 1); d.print("SCORE:"); d.print(_score);
    char hi[12];
    snprintf(hi, sizeof(hi), "HI:%d", _hiScore);
    d.setCursor(128 - (int16_t)(strlen(hi) * 6), 1);
    d.print(hi);
    d.drawFastHLine(0, SN_Y_OFF - 1, 128, SSD1306_WHITE);
}

void SnakeGame::drawField(Adafruit_SSD1306& d) const {
    // Border
    d.drawFastVLine(0,   SN_Y_OFF, 64 - SN_Y_OFF, SSD1306_WHITE);
    d.drawFastVLine(127, SN_Y_OFF, 64 - SN_Y_OFF, SSD1306_WHITE);
    d.drawFastHLine(0,   63,       128,            SSD1306_WHITE);

    // Food: outlined cell with center dot
    const int16_t fx = (int16_t)_food.col * SN_CELL;
    const int16_t fy = (int16_t)_food.row * SN_CELL + SN_Y_OFF;
    d.drawRect(fx, fy, SN_CELL, SN_CELL, SSD1306_WHITE);
    d.drawPixel(fx + SN_CELL / 2, fy + SN_CELL / 2, SSD1306_WHITE);

    // Body segments (skip head at index 0)
    for (int i = 1; i < _len; i++) {
        d.fillRect((int16_t)_body[i].col * SN_CELL,
                   (int16_t)_body[i].row * SN_CELL + SN_Y_OFF,
                   SN_CELL, SN_CELL, SSD1306_WHITE);
    }

    // Head with eye
    const int16_t hx = (int16_t)_body[0].col * SN_CELL;
    const int16_t hy = (int16_t)_body[0].row * SN_CELL + SN_Y_OFF;
    d.fillRect(hx, hy, SN_CELL, SN_CELL, SSD1306_WHITE);
    int16_t ex, ey;
    switch (_dir) {
        case RIGHT: ex = hx + SN_CELL - 2; ey = hy + 1;            break;
        case LEFT:  ex = hx + 1;            ey = hy + 1;            break;
        case DOWN:  ex = hx + 1;            ey = hy + SN_CELL - 2; break;
        default:    ex = hx + 1;            ey = hy + 1;            break; // UP
    }
    d.drawPixel(ex, ey, SSD1306_BLACK);
}

// ─── Update ───────────────────────────────────────────────────────────────────

bool SnakeGame::update(float /*dt*/, float gravX, float gravY, bool btnPressed, Adafruit_SSD1306& d) {
    // Direction input — dominant axis, no 180° reversal
    const float ax = fabsf(gravX), ay = fabsf(gravY);
    if (ax > SN_TILT_THRESH || ay > SN_TILT_THRESH) {
        Dir candidate;
        if (ax >= ay) candidate = (gravX > 0) ? RIGHT : LEFT;
        else          candidate = (gravY > 0) ? DOWN  : UP;
        const bool rev = ((candidate==RIGHT && _dir==LEFT) || (candidate==LEFT  && _dir==RIGHT) ||
                          (candidate==DOWN  && _dir==UP)   || (candidate==UP    && _dir==DOWN));
        if (!rev) _nextDir = candidate;
    }

    switch (_state) {
        case READY:
            if (btnPressed) { _state = PLAYING; _lastMoveMs = millis(); }
            break;

        case PLAYING: {
            const uint32_t now = millis();
            if (now - _lastMoveMs >= moveInterval()) {
                _lastMoveMs = now;
                _dir = _nextDir;

                const int8_t dc = (_dir==RIGHT) ? 1 : (_dir==LEFT) ? -1 : 0;
                const int8_t dr = (_dir==DOWN)  ? 1 : (_dir==UP)   ? -1 : 0;
                const int8_t nc = _body[0].col + dc;
                const int8_t nr = _body[0].row + dr;

                if (nc < 0 || nc >= SN_COLS || nr < 0 || nr >= SN_ROWS || bodyOccupies(nc, nr, 1)) {
                    if (_score > _hiScore) _hiScore = _score;
                    _state = DEAD; _stateMs = millis();
                    break;
                }

                const bool ate = (nc == _food.col && nr == _food.row);
                if (ate && _len < SN_MAX_LEN) {
                    memmove(&_body[1], &_body[0], (size_t)_len * sizeof(Pos));
                    _len++;
                } else {
                    memmove(&_body[1], &_body[0], (size_t)(_len - 1) * sizeof(Pos));
                }
                _body[0].col = nc;
                _body[0].row = nr;

                if (ate) { _score++; _foodEaten++; spawnFood(); }
            }
            break;
        }

        case DEAD:
            if (btnPressed || millis() - _stateMs >= SN_DEAD_MS) { begin(); return true; }
            break;
    }

    // ── Render ────────────────────────────────────────────────────────────────
    d.clearDisplay();
    drawHud(d);

    if (_state == READY || _state == PLAYING) {
        drawField(d);
    }
    if (_state == READY) {
        d.setTextSize(1); d.setTextColor(SSD1306_WHITE);
        const char* msg = "BTN: inizia";
        d.setCursor((128 - (int16_t)(strlen(msg)*6)) / 2, SN_Y_OFF + (64-SN_Y_OFF)/2 - 4);
        d.print(msg);
    }
    if (_state == DEAD) {
        // Flash: alternate showing/hiding the field
        if ((millis() - _stateMs) / 100 % 2 == 0) drawField(d);
        d.setTextSize(2); d.setTextColor(SSD1306_WHITE);
        d.setCursor((128 - 9*12) / 2, SN_Y_OFF + 8); d.print("GAME OVER");
        d.setTextSize(1);
        char sc[16]; snprintf(sc, sizeof(sc), "SCORE: %d", _score);
        d.setCursor((128 - (int16_t)(strlen(sc)*6)) / 2, SN_Y_OFF + 28); d.print(sc);
    }

    d.display();
    return true;
}
