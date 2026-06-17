#include "TetrisGame.h"
#include <Arduino.h>
#include <string.h>

// ─── Piece bitmasks (4×4 grid, bit 15 = row0 col0) ───────────────────────────
static const uint16_t PIECES[7][4] = {
    { 0x0F00, 0x2222, 0x00F0, 0x4444 }, // I
    { 0x0660, 0x0660, 0x0660, 0x0660 }, // O
    { 0x0E40, 0x4C40, 0x4E00, 0x4640 }, // T
    { 0x06C0, 0x8C40, 0x06C0, 0x8C40 }, // S
    { 0x0C60, 0x4C80, 0x0C60, 0x4C80 }, // Z
    { 0x8E00, 0x6440, 0x0E20, 0x44C0 }, // J
    { 0x2E00, 0x4460, 0x0E80, 0xC440 }, // L
};

// ─── Block drawing helper ─────────────────────────────────────────────────────

static constexpr int CELL = 4;  // mirrors TT_CELL

static void drawBlock(Adafruit_SSD1306& d, int16_t x, int16_t y) {
    d.fillRect(x, y, CELL, CELL, SSD1306_WHITE);
    d.drawRect(x + 1, y + 1, CELL - 2, CELL - 2, SSD1306_BLACK);
}

// ─── Helpers ──────────────────────────────────────────────────────────────────

uint32_t TetrisGame::fallIntervalMs() const {
    if (_level >= 10) return 100u;
    const uint32_t ms = 800u - (uint32_t)(_level - 1) * 78u;
    return ms > 100u ? ms : 100u;
}

bool TetrisGame::canPlace(int8_t px, int8_t py, uint8_t type, uint8_t rot) const {
    const uint16_t mask = PIECES[type][rot];
    for (int r = 0; r < 4; r++) {
        for (int c = 0; c < 4; c++) {
            if (!((mask >> (15 - r*4 - c)) & 1)) continue;
            const int row = py + r;
            const int col = px + c;
            if (col < 0 || col >= TT_COLS || row >= TT_ROWS) return false;
            if (row >= 0 && _board[row][col]) return false;
        }
    }
    return true;
}

void TetrisGame::lockPiece() {
    const uint16_t mask = PIECES[_type][_rot];
    for (int r = 0; r < 4; r++) {
        for (int c = 0; c < 4; c++) {
            if (!((mask >> (15 - r*4 - c)) & 1)) continue;
            const int row = _py + r;
            const int col = _px + c;
            if (row >= 0 && row < TT_ROWS && col >= 0 && col < TT_COLS)
                _board[row][col] = 1;
        }
    }
}

int TetrisGame::clearLines() {
    _clearMask = 0;
    int count  = 0;
    for (int r = 0; r < TT_ROWS; r++) {
        bool full = true;
        for (int c = 0; c < TT_COLS && full; c++)
            if (!_board[r][c]) full = false;
        if (full) { _clearMask |= (uint16_t)(1u << r); count++; }
    }
    return count;
}

void TetrisGame::collapseLines() {
    int wr = TT_ROWS - 1;
    for (int rr = TT_ROWS - 1; rr >= 0; rr--) {
        bool full = true;
        for (int c = 0; c < TT_COLS && full; c++)
            if (!_board[rr][c]) full = false;
        if (!full) {
            if (wr != rr) memcpy(_board[wr], _board[rr], TT_COLS);
            wr--;
        }
    }
    for (; wr >= 0; wr--) memset(_board[wr], 0, TT_COLS);
}

void TetrisGame::spawnPiece() {
    _type     = _nextType;
    _rot      = 0;
    _px       = (int8_t)(TT_COLS / 2 - 2);
    _py       = -1;
    _nextType = (uint8_t)random(0, 7);
    _hasHeld  = true;  // reset hold allowance each new piece
    if (!canPlace(_px, _py, _type, _rot)) {
        _state      = GAME_OVER;
        _stateTimer = 0.f;
    }
}

void TetrisGame::holdPiece() {
    if (!_hasHeld) return;
    _hasHeld = false;

    if (_heldType == 0xFF) {
        // Nothing held: stash current piece, bring in next
        _heldType = _type;
        _type     = _nextType;
        _rot      = 0;
        _px       = (int8_t)(TT_COLS / 2 - 2);
        _py       = -1;
        _nextType = (uint8_t)random(0, 7);
    } else {
        // Swap current with held
        const uint8_t tmp = _heldType;
        _heldType = _type;
        _type     = tmp;
        _rot      = 0;
        _px       = (int8_t)(TT_COLS / 2 - 2);
        _py       = -1;
    }
    if (!canPlace(_px, _py, _type, _rot)) {
        _state      = GAME_OVER;
        _stateTimer = 0.f;
    }
}

void TetrisGame::addScore(int lines) {
    static const uint16_t PTS[5] = { 0, 100, 300, 500, 800 };
    _score += (uint32_t)(PTS[lines > 4 ? 4 : lines] * _level);
    _lines += (uint16_t)lines;
    const uint8_t lv = (uint8_t)(_lines / 10 + 1);
    _level = lv > 10 ? 10 : lv;
}

// ─── Render helpers ───────────────────────────────────────────────────────────

void TetrisGame::renderBoard(Adafruit_SSD1306& d) const {
    d.drawFastVLine(0,                     0, 64, SSD1306_WHITE);
    d.drawFastVLine(1 + TT_COLS * TT_CELL, 0, 64, SSD1306_WHITE);

    const uint32_t flashPhase = millis() / 80;
    for (int r = 0; r < TT_ROWS; r++) {
        const bool flashRow = (_state == LINE_CLEAR) && ((_clearMask >> r) & 1);
        if (flashRow && (flashPhase % 2 == 0)) continue;
        for (int c = 0; c < TT_COLS; c++) {
            if (_board[r][c])
                drawBlock(d, (int16_t)(1 + c * TT_CELL), (int16_t)(r * TT_CELL));
        }
    }
}

void TetrisGame::renderGhost(Adafruit_SSD1306& d) const {
    int8_t gy = _py;
    while (canPlace(_px, gy + 1, _type, _rot)) gy++;
    if (gy == _py) return;  // already grounded, no ghost needed

    const uint16_t mask = PIECES[_type][_rot];
    for (int r = 0; r < 4; r++) {
        for (int c = 0; c < 4; c++) {
            if (!((mask >> (15 - r*4 - c)) & 1)) continue;
            const int row = gy + r;
            const int col = _px + c;
            if (row >= 0 && row < TT_ROWS && col >= 0 && col < TT_COLS)
                d.drawRect((int16_t)(1 + col * TT_CELL), (int16_t)(row * TT_CELL),
                           TT_CELL, TT_CELL, SSD1306_WHITE);
        }
    }
}

void TetrisGame::renderActivePiece(Adafruit_SSD1306& d) const {
    const uint16_t mask = PIECES[_type][_rot];
    for (int r = 0; r < 4; r++) {
        for (int c = 0; c < 4; c++) {
            if (!((mask >> (15 - r*4 - c)) & 1)) continue;
            const int row = _py + r;
            const int col = _px + c;
            if (row >= 0 && row < TT_ROWS && col >= 0 && col < TT_COLS)
                drawBlock(d, (int16_t)(1 + col * TT_CELL), (int16_t)(row * TT_CELL));
        }
    }
}

void TetrisGame::renderHUD(Adafruit_SSD1306& d) const {
    // HUD starts after right board border (x=41), leave 3px gap
    static constexpr int16_t hx  = 44;   // left HUD column
    static constexpr int16_t hx2 = 86;   // right HUD column (NXT vs HLD)

    d.setTextSize(1);
    d.setTextColor(SSD1306_WHITE);

    // Score
    d.setCursor(hx, 0);  d.print("SC");
    d.setCursor(hx, 9);  d.print(_score);

    // Level and lines compact on one row
    d.setCursor(hx, 19);
    d.print("LV:"); d.print(_level);
    d.print(" LN:"); d.print(_lines);

    // Separator
    d.drawFastHLine(hx, 28, 84, SSD1306_WHITE);

    // NXT / HLD column labels
    d.setCursor(hx,  30); d.print("NXT");
    d.setCursor(hx2, 30); d.print("HLD");

    // Next piece preview (3×3 blocks)
    const uint16_t nm = PIECES[_nextType][0];
    for (int r = 0; r < 4; r++) {
        for (int c = 0; c < 4; c++) {
            if ((nm >> (15 - r*4 - c)) & 1)
                d.fillRect((int16_t)(hx + c * 3), (int16_t)(39 + r * 3), 3, 3, SSD1306_WHITE);
        }
    }

    // Hold piece preview: filled when hold available, outline when used this turn
    if (_heldType != 0xFF) {
        const uint16_t hm = PIECES[_heldType][0];
        for (int r = 0; r < 4; r++) {
            for (int c = 0; c < 4; c++) {
                if (!((hm >> (15 - r*4 - c)) & 1)) continue;
                const int16_t bx = (int16_t)(hx2 + c * 3);
                const int16_t by = (int16_t)(39 + r * 3);
                if (_hasHeld)
                    d.fillRect(bx, by, 3, 3, SSD1306_WHITE);
                else
                    d.drawRect(bx, by, 3, 3, SSD1306_WHITE);
            }
        }
    }
}

void TetrisGame::renderGameOver(Adafruit_SSD1306& d) const {
    // Centred overlay panel (double border)
    d.fillRect(2, 8, 124, 48, SSD1306_BLACK);
    d.drawRect(2, 8, 124, 48, SSD1306_WHITE);
    d.drawRect(3, 9, 122, 46, SSD1306_WHITE);

    d.setTextSize(2);
    d.setTextColor(SSD1306_WHITE);
    d.setCursor(5, 12);
    d.print("GAME OVER");

    d.setTextSize(1);
    d.setCursor(5, 32);
    d.print("Punti: ");
    d.print(_score);
    d.setCursor(5, 43);
    d.print("BTN: riprova");
}

// ─── begin ────────────────────────────────────────────────────────────────────

void TetrisGame::begin() {
    memset(_board, 0, sizeof(_board));
    _score      = 0;
    _lines      = 0;
    _level      = 1;
    _fallTimer  = 0.f;
    _moveTimer  = 0.f;
    _stateTimer = 0.f;
    _clearMask  = 0;
    _state      = PLAYING;
    _lastBtn    = false;
    _lastShake  = false;
    _heldType   = 0xFF;
    _hasHeld    = true;
    _btnDownMs  = 0;
    _nextType   = (uint8_t)random(0, 7);
    spawnPiece();
}

// ─── update ───────────────────────────────────────────────────────────────────

bool TetrisGame::update(float dt, float gravX, float gravY, bool btnPressed, bool shake, Adafruit_SSD1306& d) {
    // Button edge detection
    const bool btnDown    = btnPressed && !_lastBtn;   // rising edge
    const bool btnRelease = !btnPressed && _lastBtn;   // falling edge
    const bool shakeEdge  = shake && !_lastShake;
    _lastBtn   = btnPressed;
    _lastShake = shake;

    if (btnDown) _btnDownMs = millis();

    // Short press → rotate; long press → hold
    bool doRotate = false, doHold = false;
    if (btnRelease) {
        if (millis() - _btnDownMs < TT_HOLD_MS) doRotate = true;
        else                                      doHold   = true;
    }

    // ── GAME OVER ─────────────────────────────────────────────────────────────
    if (_state == GAME_OVER) {
        _stateTimer += dt;
        d.clearDisplay();
        renderBoard(d);
        renderHUD(d);
        renderGameOver(d);
        d.display();
        if (btnDown || shakeEdge || _stateTimer >= TT_OVER_TIMEOUT) return false;
        return true;
    }

    // ── LINE_CLEAR pause ──────────────────────────────────────────────────────
    if (_state == LINE_CLEAR) {
        _stateTimer += dt;
        if (_stateTimer >= TT_CLEAR_PAUSE) {
            collapseLines();
            _clearMask  = 0;
            _stateTimer = 0.f;
            _state      = PLAYING;
            spawnPiece();
        }
        d.clearDisplay();
        renderBoard(d);
        renderHUD(d);
        d.display();
        return true;
    }

    // ── PLAYING ───────────────────────────────────────────────────────────────

    // Rotate (wall-kick)
    if (doRotate) {
        const uint8_t newRot  = (_rot + 1) & 3;
        const int8_t  kicks[] = {0, 1, -1, 2, -2};
        for (int ki = 0; ki < 5; ki++) {
            if (canPlace(_px + kicks[ki], _py, _type, newRot)) {
                _px  = (int8_t)(_px + kicks[ki]);
                _rot = newRot;
                break;
            }
        }
    }

    // Hold (long press)
    if (doHold) holdPiece();

    // Hard drop (shake)
    bool hardDropped = false;
    if (shakeEdge) {
        int dropped = 0;
        while (canPlace(_px, _py + 1, _type, _rot)) { _py++; dropped++; }
        _score += (uint32_t)(dropped * TT_DROP_SCORE);
        lockPiece();
        const int cleared = clearLines();
        if (cleared > 0) {
            addScore(cleared);
            _state      = LINE_CLEAR;
            _stateTimer = 0.f;
        } else {
            spawnPiece();
        }
        _fallTimer  = 0.f;
        _moveTimer  = 0.f;
        hardDropped = true;
    }

    if (!hardDropped) {
        // Lateral movement (tilt left/right)
        _moveTimer += dt;
        if (_moveTimer >= TT_MOVE_INTERVAL) {
            _moveTimer = 0.f;
            if      (gravX < -TT_TILT_THRESHOLD) { if (canPlace(_px - 1, _py, _type, _rot)) _px--; }
            else if (gravX >  TT_TILT_THRESHOLD) { if (canPlace(_px + 1, _py, _type, _rot)) _px++; }
        }

        // Gravity / soft drop (tilt toward player)
        const bool    softDrop  = (gravY > TT_SOFT_THRESHOLD);
        const float   intervalS = (float)(softDrop ? TT_SOFT_INTERVAL : fallIntervalMs()) * 0.001f;
        _fallTimer += dt;
        if (_fallTimer >= intervalS) {
            _fallTimer = 0.f;
            if (canPlace(_px, _py + 1, _type, _rot)) {
                _py++;
            } else {
                lockPiece();
                const int cleared = clearLines();
                if (cleared > 0) {
                    addScore(cleared);
                    _state      = LINE_CLEAR;
                    _stateTimer = 0.f;
                } else {
                    spawnPiece();
                }
            }
        }
    }

    // ── Render ────────────────────────────────────────────────────────────────
    d.clearDisplay();
    renderBoard(d);
    if (_state == PLAYING) {
        renderGhost(d);
        renderActivePiece(d);
    }
    renderHUD(d);
    d.display();
    return true;
}
