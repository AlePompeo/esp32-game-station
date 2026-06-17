#include "LabyrinthGame.h"
#include <Arduino.h>
#include <string.h>

// ─── Maze generation (iterative DFS / recursive backtracker) ─────────────────

void LabyrinthGame::generateMaze() {
    for (int r = 0; r < LB_ROWS; r++)
        for (int c = 0; c < LB_COLS; c++)
            _maze[r][c] = WALL_N | WALL_S | WALL_E | WALL_W;

    static const int8_t  DC[4] = {  0, 0, 1, -1 };
    static const int8_t  DR[4] = { -1, 1, 0,  0 };
    static const uint8_t DW[4] = { WALL_N, WALL_S, WALL_E, WALL_W };
    static const uint8_t OW[4] = { WALL_S, WALL_N, WALL_W, WALL_E };

    struct Cell { int8_t c, r; };
    Cell stack[LB_ROWS * LB_COLS];
    bool visited[LB_ROWS][LB_COLS];
    memset(visited, 0, sizeof(visited));
    int top = 0;

    stack[top++] = {0, 0};
    visited[0][0] = true;

    while (top > 0) {
        Cell cur = stack[top - 1];
        Cell nbr[4];
        uint8_t ndir[4];
        int cnt = 0;

        for (int d = 0; d < 4; d++) {
            const int8_t nc = cur.c + DC[d];
            const int8_t nr = cur.r + DR[d];
            if (nc >= 0 && nc < LB_COLS && nr >= 0 && nr < LB_ROWS && !visited[nr][nc]) {
                nbr[cnt]  = {nc, nr};
                ndir[cnt] = (uint8_t)d;
                cnt++;
            }
        }

        if (cnt == 0) {
            top--;
        } else {
            const int pick = (int)random(cnt);
            const int d    = ndir[pick];
            const Cell next = nbr[pick];
            _maze[cur.r][cur.c]   &= ~DW[d];
            _maze[next.r][next.c] &= ~OW[d];
            visited[next.r][next.c] = true;
            stack[top++] = next;
        }
    }
}

// ─── newRound: partial reset — keeps _level and _bestSec ─────────────────────

void LabyrinthGame::newRound() {
    randomSeed(millis());
    _px = 0; _py = 0;
    _state = READY;
    _stateMs = millis();
    _lastMoveMs = 0;
    // Move interval shrinks 8 ms per level (160 ms at Lv.1, floor 80 ms at Lv.11+)
    const int ms = 160 - ((int)_level - 1) * 8;
    _moveMs = (uint32_t)(ms < 80 ? 80 : ms);
    generateMaze();
}

// ─── begin: full reset (called from menu) ────────────────────────────────────

void LabyrinthGame::begin() {
    _level   = 1;
    _bestSec = 0;
    newRound();
}

// ─── Draw ─────────────────────────────────────────────────────────────────────

void LabyrinthGame::drawHud(Adafruit_SSD1306& d) const {
    d.setTextSize(1);
    d.setTextColor(SSD1306_WHITE);
    d.setCursor(0, 0);
    d.print("LABIRINTO");

    char buf[16];
    if (_state == PLAYING) {
        const uint32_t sec = (millis() - _startMs) / 1000;
        snprintf(buf, sizeof(buf), "Lv.%d %ds", _level, (int)sec);
    } else {
        snprintf(buf, sizeof(buf), "Lv.%d", _level);
    }
    d.setCursor(128 - (int16_t)(strlen(buf) * 6), 0);
    d.print(buf);

    d.drawFastHLine(0, 8, 128, SSD1306_WHITE);
}

void LabyrinthGame::drawMaze(Adafruit_SSD1306& d) const {
    // Outer border
    d.drawRect(LB_X0, LB_Y0, LB_COLS * LB_STEP + 1, LB_ROWS * LB_STEP + 1, SSD1306_WHITE);

    // Goal: filled square with black X in bottom-right cell interior
    const int16_t gx = (int16_t)(LB_X0 + (LB_COLS-1)*LB_STEP + 1);
    const int16_t gy = (int16_t)(LB_Y0 + (LB_ROWS-1)*LB_STEP + 1);
    d.fillRect(gx + 1, gy + 1, LB_CELL - 2, LB_CELL - 2, SSD1306_WHITE);
    d.drawLine(gx + 1, gy + 1, gx + LB_CELL - 2, gy + LB_CELL - 2, SSD1306_BLACK);
    d.drawLine(gx + LB_CELL - 2, gy + 1, gx + 1, gy + LB_CELL - 2, SSD1306_BLACK);

    // Interior walls (E and S only — outer border already drawn)
    for (int r = 0; r < LB_ROWS; r++) {
        for (int c = 0; c < LB_COLS; c++) {
            const int16_t cx = (int16_t)(LB_X0 + c * LB_STEP);
            const int16_t cy = (int16_t)(LB_Y0 + r * LB_STEP);

            if (c < LB_COLS-1 && (_maze[r][c] & WALL_E))
                d.drawFastVLine(cx + LB_STEP, cy, LB_STEP + 1, SSD1306_WHITE);

            if (r < LB_ROWS-1 && (_maze[r][c] & WALL_S))
                d.drawFastHLine(cx, cy + LB_STEP, LB_STEP + 1, SSD1306_WHITE);
        }
    }
}

// ─── Update ───────────────────────────────────────────────────────────────────

bool LabyrinthGame::update(float /*dt*/, float gravX, float gravY,
                           bool btnPressed, Adafruit_SSD1306& d) {
    // ── WIN screen ────────────────────────────────────────────────────────────
    if (_state == WIN) {
        if (btnPressed || millis() - _stateMs >= WIN_MS) {
            _level++;
            newRound();
            return true;
        }

        d.clearDisplay();

        // Panel with double border
        d.drawRect(2, 2, 124, 60, SSD1306_WHITE);
        d.drawRect(3, 3, 122, 58, SSD1306_WHITE);

        // Level completed header
        char lvlBuf[16];
        snprintf(lvlBuf, sizeof(lvlBuf), "LIVELLO %d", _level);
        d.setTextSize(1); d.setTextColor(SSD1306_WHITE);
        d.setCursor((128 - (int16_t)(strlen(lvlBuf) * 6)) / 2, 7);
        d.print(lvlBuf);

        // "SUPERATO!" centred
        d.setTextSize(2);
        const char* title = "SUPERATO!";
        d.setCursor((128 - (int16_t)(strlen(title) * 12)) / 2, 18);
        d.print(title);

        // Time and record
        const uint32_t sec = (millis() - _startMs) / 1000;
        char buf[22];
        d.setTextSize(1);
        snprintf(buf, sizeof(buf), "Tempo: %ds", (int)sec);
        d.setCursor((128 - (int16_t)(strlen(buf) * 6)) / 2, 38);
        d.print(buf);

        if (_bestSec > 0) {
            snprintf(buf, sizeof(buf), "Record: %ds", (int)_bestSec);
            d.setCursor((128 - (int16_t)(strlen(buf) * 6)) / 2, 48);
            d.print(buf);
        }

        d.display();
        return true;
    }

    // ── State transitions ─────────────────────────────────────────────────────
    switch (_state) {
        case READY:
            if (btnPressed) {
                _state = PLAYING;
                _startMs = _lastMoveMs = millis();
            }
            break;

        case PLAYING: {
            const uint32_t now = millis();
            if (now - _lastMoveMs >= _moveMs) {
                _lastMoveMs = now;
                const float ax = fabsf(gravX), ay = fabsf(gravY);
                if (ax > TILT || ay > TILT) {
                    int8_t  dc = 0, dr = 0;
                    uint8_t wall;
                    if (ax >= ay) {
                        dc   = (gravX > 0) ? 1 : -1;
                        wall = (dc > 0) ? WALL_E : WALL_W;
                    } else {
                        dr   = (gravY > 0) ? 1 : -1;
                        wall = (dr > 0) ? WALL_S : WALL_N;
                    }

                    if (!(_maze[_py][_px] & wall)) {
                        _px += dc; _py += dr;
                        if (_px == LB_COLS-1 && _py == LB_ROWS-1) {
                            const uint32_t sec = (millis() - _startMs) / 1000;
                            if (_bestSec == 0 || sec < _bestSec) _bestSec = sec;
                            _state   = WIN;
                            _stateMs = millis();
                        }
                    }
                }
            }
            break;
        }

        default: break;
    }

    // ── Render ────────────────────────────────────────────────────────────────
    d.clearDisplay();
    drawHud(d);
    drawMaze(d);

    // Player: 3×3 filled square centred in the 5×5 cell interior
    const int16_t px = (int16_t)(LB_X0 + _px * LB_STEP + 2);
    const int16_t py = (int16_t)(LB_Y0 + _py * LB_STEP + 2);
    d.fillRect(px, py, 3, 3, SSD1306_WHITE);

    if (_state == READY) {
        const char* msg = "BTN: avvia";
        const int16_t tw = (int16_t)(strlen(msg) * 6);
        const int16_t tx = (128 - tw) / 2;
        const int16_t ty = (int16_t)(LB_Y0 + (LB_ROWS * LB_STEP) / 2 - 4);
        d.fillRect(tx - 2, ty - 1, tw + 4, 9, SSD1306_BLACK);
        d.setTextSize(1); d.setTextColor(SSD1306_WHITE);
        d.setCursor(tx, ty);
        d.print(msg);
    }

    d.display();
    return true;
}
