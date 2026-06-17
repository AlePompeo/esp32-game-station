#include "SpaceInvaders.h"
#include <Arduino.h>

// ─── Layout (128×64 display) ──────────────────────────────────────────────────
constexpr int16_t INV_W        = 7;
constexpr int16_t INV_H        = 5;
constexpr int16_t CELL_W       = 14;
constexpr int16_t CELL_H       = 9;
constexpr float   BLOCK_BASE_Y = 10.f;
constexpr float   SHIP_Y       = 53.f;
constexpr float   SHIP_MIN_X   =  5.f;
constexpr float   SHIP_MAX_X   = 119.f;
constexpr float   GAMEOVER_Y   = 48.f;
constexpr float   PB_SPD       = -160.f;
constexpr float   PB_FIRE_Y    = SHIP_Y - 1.f;
constexpr float   INV_STEP_X   = 3.f;
constexpr float   INV_DROP_Y   = 4.f;
constexpr int16_t BNK_Y        = 43;

static const int16_t BNK_X[SpaceInvaders::NUM_BUNKERS] = { 14, 57, 100 };

// ─── Difficulty configs ───────────────────────────────────────────────────────
//                        rows cols  lives  maxIb  baseInt  minInt  fireInt ibSpd  name
const SpaceInvaders::DiffCfg SpaceInvaders::DIFF[5] = {
    { 2, 5,  5, 1, 1.00f, 0.12f, 3.0f,  50.f, "FACILE"    },
    { 3, 6,  3, 2, 0.70f, 0.08f, 2.0f,  65.f, "MEDIO"     },
    { 3, 7,  3, 3, 0.55f, 0.06f, 1.5f,  80.f, "DIFFICILE" },
    { 4, 8,  2, 4, 0.40f, 0.05f, 1.2f,  95.f, "ESPERTO"   },
    { 4, 8,  1, 4, 0.30f, 0.04f, 0.8f, 120.f, "INFERNO"   },
};

// ─── Points per row (row 0 = topmost, highest value) ─────────────────────────
static const int16_t ROW_PTS[SpaceInvaders::MAX_ROWS] = { 40, 30, 20, 10 };

// ─── Sprite patterns (7-bit wide × 5-row, MSB = leftmost pixel) ──────────────
static const uint8_t INV_PAT[3][5] = {
    { 0b0011100, 0b1111111, 0b1010101, 0b0111110, 0b1001001 }, // squid
    { 0b0100010, 0b0111110, 0b1111111, 0b1010101, 0b0010100 }, // crab
    { 0b0001000, 0b0111110, 0b1111111, 0b1111111, 0b0101010 }, // simple
};

// ─── Bunker initial bitmask (12 cols × 6 rows) ───────────────────────────────
// bit11 = col 0 (leftmost), bit0 = col 11 (rightmost).
// Rows 4-5 have a 4-pixel notch cut from the bottom-centre.
static const uint16_t BNK_INIT[SpaceInvaders::BNK_ROWS] = {
    0b011111111110,   // row 0: arched top (cols 1-10)
    0b111111111111,   // row 1
    0b111111111111,   // row 2
    0b111111111111,   // row 3
    0b111100001111,   // row 4: notch (cols 0-3, 8-11)
    0b111100001111,   // row 5: notch
};

// ─── Init ─────────────────────────────────────────────────────────────────────

void SpaceInvaders::begin() {
    if (!_mutex) _mutex = xSemaphoreCreateMutex();
    _gstate  = DIFF_SELECT;
    _diffIdx = 1;   // default highlight: MEDIO
    _shipX   = 64.f;
    _stateMs = millis();
    copyToSnap();
}

// ─── Start a specific difficulty ──────────────────────────────────────────────

void SpaceInvaders::startGame(int idx) {
    const DiffCfg& d = DIFF[idx];
    _rows          = d.rows;
    _cols          = d.cols;
    _lives         = d.lives;
    _maxIb         = d.maxIb;
    _baseInterval  = d.baseInterval;
    _minInterval   = d.minInterval;
    _fireInterval  = d.fireInterval;
    _ibSpeed       = d.ibSpeed;

    const float bw = (_cols - 1) * CELL_W + INV_W;
    _blockBaseX    = (128.f - bw) / 2.f;

    for (int r = 0; r < _rows; r++)
        for (int c = 0; c < _cols; c++)
            _alive[r][c] = true;
    _aliveCount = _rows * _cols;
    _blockX     = 0.f;
    _blockY     = 0.f;
    _dirX       = 1.f;
    _moveTimer  = 0.f;
    _fireTimer  = _fireInterval * 0.4f;
    _score      = 0;
    _gstate     = PLAYING;
    _stateMs    = millis();

    for (int i = 0; i < MAX_PB; i++) _pb[i].active = false;
    for (int i = 0; i < MAX_IB; i++) _ib[i].active = false;

    for (int k = 0; k < NUM_BUNKERS; k++) {
        _bunkers[k].x = BNK_X[k];
        _bunkers[k].y = BNK_Y;
        for (int r = 0; r < BNK_ROWS; r++)
            _bunkers[k].mask[r] = BNK_INIT[r];
    }

    recalcSpeed();
    copyToSnap();
}

void SpaceInvaders::recalcSpeed() {
    const float ratio = (float)_aliveCount / (_rows * _cols);
    _moveInterval = _minInterval + (_baseInterval - _minInterval) * ratio;
}

// ─── Core 0: logic step ───────────────────────────────────────────────────────

void SpaceInvaders::logicStep(float shipX, bool firePending, float dt) {
    if (_gstate == DIFF_SELECT) {
        _shipX = shipX;
        // Map horizontal ship position to difficulty index
        float frac = (shipX - SHIP_MIN_X) / (SHIP_MAX_X - SHIP_MIN_X);
        int idx = (int)(frac * NUM_DIFF);
        if (idx < 0) idx = 0;
        if (idx >= NUM_DIFF) idx = NUM_DIFF - 1;
        _diffIdx = idx;
        if (firePending) startGame(_diffIdx);
        copyToSnap();
        return;
    }

    if (_gstate != PLAYING) return;

    _shipX = shipX;
    if (firePending) firePlayer(shipX);

    // Invader block movement
    _moveTimer += dt;
    if (_moveTimer >= _moveInterval) {
        _moveTimer = 0.f;

        int minC = _cols, maxC = -1;
        for (int r = 0; r < _rows; r++)
            for (int c = 0; c < _cols; c++)
                if (_alive[r][c]) {
                    if (c < minC) minC = c;
                    if (c > maxC) maxC = c;
                }

        if (minC <= maxC) {
            const float nx        = _blockX + _dirX * INV_STEP_X;
            const float leftEdge  = _blockBaseX + nx + minC * CELL_W;
            const float rightEdge = _blockBaseX + nx + maxC * CELL_W + INV_W;
            if (leftEdge < 0.f || rightEdge > 128.f) {
                _blockY += INV_DROP_Y;
                _dirX    = -_dirX;
            } else {
                _blockX  = nx;
            }
        }
    }

    // Bullet movement
    for (int i = 0; i < MAX_PB; i++)
        if (_pb[i].active) { _pb[i].y += PB_SPD * dt; if (_pb[i].y < 8.f) _pb[i].active = false; }
    for (int i = 0; i < _maxIb; i++)
        if (_ib[i].active) { _ib[i].y += _ibSpeed * dt; if (_ib[i].y > 64.f) _ib[i].active = false; }

    // Invader auto-fire
    _fireTimer += dt;
    if (_fireTimer >= _fireInterval) { _fireTimer = 0.f; fireInvader(); }

    checkCollisions();

    // Win / Lose checks
    if (_aliveCount == 0 && _gstate == PLAYING) { _gstate = WIN;  _stateMs = millis(); }
    if (_lives <= 0      && _gstate == PLAYING) { _gstate = LOSE; _stateMs = millis(); }
    for (int r = 0; r < _rows && _gstate == PLAYING; r++)
        for (int c = 0; c < _cols; c++)
            if (_alive[r][c] &&
                BLOCK_BASE_Y + _blockY + r * CELL_H + INV_H >= GAMEOVER_Y)
                { _gstate = LOSE; _stateMs = millis(); }

    copyToSnap();
}

// ─── Bullet helpers ───────────────────────────────────────────────────────────

void SpaceInvaders::firePlayer(float x) {
    for (int i = 0; i < MAX_PB; i++)
        if (!_pb[i].active) { _pb[i] = {x, PB_FIRE_Y, true}; return; }
}

void SpaceInvaders::fireInvader() {
    if (_aliveCount == 0) return;
    int col = random(0, _cols);
    for (int tries = 0; tries < _cols; tries++, col = (col + 1) % _cols) {
        int bot = -1;
        for (int r = _rows - 1; r >= 0; r--) if (_alive[r][col]) { bot = r; break; }
        if (bot < 0) continue;
        const float ix = _blockBaseX + _blockX + col * CELL_W + INV_W * 0.5f;
        const float iy = BLOCK_BASE_Y + _blockY + bot * CELL_H + INV_H;
        for (int i = 0; i < _maxIb; i++)
            if (!_ib[i].active) { _ib[i] = {ix, iy, true}; return; }
        return;
    }
}

// ─── Collision detection ──────────────────────────────────────────────────────

void SpaceInvaders::checkCollisions() {
    // Player bullets vs invaders
    for (int bi = 0; bi < MAX_PB; bi++) {
        if (!_pb[bi].active) continue;
        for (int r = 0; r < _rows; r++) for (int c = 0; c < _cols; c++) {
            if (!_alive[r][c]) continue;
            const float ix = _blockBaseX + _blockX + c * CELL_W;
            const float iy = BLOCK_BASE_Y + _blockY + r * CELL_H;
            if (_pb[bi].x >= ix && _pb[bi].x <= ix + INV_W &&
                _pb[bi].y >= iy && _pb[bi].y <= iy + INV_H) {
                _alive[r][c]    = false;
                _pb[bi].active  = false;
                _score         += ROW_PTS[r];
                _aliveCount--;
                recalcSpeed();
            }
        }
    }

    // Player bullets vs bunkers
    for (int bi = 0; bi < MAX_PB; bi++) {
        if (!_pb[bi].active) continue;
        for (int k = 0; k < NUM_BUNKERS; k++)
            if (bulletHitsBunker(_pb[bi], _bunkers[k]))
                _pb[bi].active = false;
    }

    // Invader bullets vs ship
    const float sx1 = _shipX - 4.f, sx2 = _shipX + 4.f;
    for (int i = 0; i < _maxIb; i++) {
        if (!_ib[i].active) continue;
        if (_ib[i].x >= sx1 && _ib[i].x <= sx2 &&
            _ib[i].y >= SHIP_Y && _ib[i].y <= SHIP_Y + 4.f) {
            _ib[i].active = false;
            _lives--;
        }
    }

    // Invader bullets vs bunkers
    for (int bi = 0; bi < _maxIb; bi++) {
        if (!_ib[bi].active) continue;
        for (int k = 0; k < NUM_BUNKERS; k++)
            if (bulletHitsBunker(_ib[bi], _bunkers[k]))
                _ib[bi].active = false;
    }
}

// ─── Snapshot ─────────────────────────────────────────────────────────────────

void SpaceInvaders::copyToSnap() {
    if (!_mutex || xSemaphoreTake(_mutex, 0) != pdTRUE) return;
    for (int r = 0; r < _rows; r++)
        for (int c = 0; c < _cols; c++) _snap.alive[r][c] = _alive[r][c];
    _snap.rows       = _rows;
    _snap.cols       = _cols;
    _snap.blockX     = _blockX;
    _snap.blockY     = _blockY;
    _snap.blockBaseX = _blockBaseX;
    for (int i = 0; i < MAX_PB; i++) _snap.pb[i] = _pb[i];
    for (int i = 0; i < MAX_IB; i++) _snap.ib[i] = _ib[i];
    for (int k = 0; k < NUM_BUNKERS; k++) _snap.bunkers[k] = _bunkers[k];
    _snap.shipX   = _shipX;
    _snap.score   = _score;
    _snap.lives   = _lives;
    _snap.diffIdx = _diffIdx;
    _snap.gstate  = _gstate;
    _snap.stateMs = _stateMs;
    xSemaphoreGive(_mutex);
}

// ─── Bunker pixel helpers ─────────────────────────────────────────────────────

bool SpaceInvaders::bunkerPixel(const Bunker& b, int row, int col) {
    return (b.mask[row] & (uint16_t)(0x800u >> col)) != 0;
}

void SpaceInvaders::erasePixels(Bunker& b, int row, int col) {
    for (int dr = -1; dr <= 1; dr++) {
        const int r = row + dr;
        if (r < 0 || r >= BNK_ROWS) continue;
        for (int dc = -1; dc <= 1; dc++) {
            const int c = col + dc;
            if (c < 0 || c >= BNK_COLS) continue;
            b.mask[r] &= ~(uint16_t)(0x800u >> c);
        }
    }
}

bool SpaceInvaders::bulletHitsBunker(const Bullet& bul, Bunker& b) {
    const int bx = (int)(bul.x - b.x);
    const int by = (int)(bul.y - b.y);
    if (bx < 0 || bx >= BNK_COLS || by < 0 || by >= BNK_ROWS) return false;
    if (!bunkerPixel(b, by, bx)) return false;
    erasePixels(b, by, bx);
    return true;
}

// ─── Sprites ──────────────────────────────────────────────────────────────────

void SpaceInvaders::drawInvader(Adafruit_SSD1306& d, int16_t x, int16_t y, int row) {
    const uint8_t* p = INV_PAT[row < 3 ? row : 2];
    for (int r = 0; r < INV_H; r++)
        for (int c = 0; c < INV_W; c++)
            if (p[r] & (0x40 >> c)) d.drawPixel(x + c, y + r, SSD1306_WHITE);
}

void SpaceInvaders::drawShip(Adafruit_SSD1306& d, int16_t cx, int16_t y) {
    d.drawPixel(cx, y, SSD1306_WHITE);
    d.fillRect(cx - 2, y + 1, 5, 1, SSD1306_WHITE);
    d.fillRect(cx - 4, y + 2, 9, 2, SSD1306_WHITE);
}

void SpaceInvaders::drawBunker(Adafruit_SSD1306& d, const Bunker& b) {
    for (int r = 0; r < BNK_ROWS; r++) {
        const uint16_t m = b.mask[r];
        if (!m) continue;
        for (int c = 0; c < BNK_COLS; c++)
            if (m & (uint16_t)(0x800u >> c))
                d.drawPixel(b.x + c, b.y + r, SSD1306_WHITE);
    }
}

// ─── Core 1: render frame ────────────────────────────────────────────────────

bool SpaceInvaders::renderFrame(Adafruit_SSD1306& d, bool btnPressed, bool shake) {
    Snap s;
    xSemaphoreTake(_mutex, portMAX_DELAY);
    s = _snap;
    xSemaphoreGive(_mutex);

    // ── Difficulty select screen ──────────────────────────────────────────────
    if (s.gstate == DIFF_SELECT) {
        d.clearDisplay();
        d.setTextSize(1); d.setTextColor(SSD1306_WHITE);
        d.setCursor(22, 2); d.print("SPACE INVADERS");
        d.drawFastHLine(0, 12, 128, SSD1306_WHITE);

        for (int i = 0; i < NUM_DIFF; i++) {
            const int16_t y = 14 + i * 10;
            if (i == s.diffIdx) {
                d.fillRect(0, y - 1, 128, 10, SSD1306_WHITE);
                d.setTextColor(SSD1306_BLACK);
            } else {
                d.setTextColor(SSD1306_WHITE);
            }
            d.setCursor(6, y);
            d.print(DIFF[i].name);
            // Right-align grid dimensions
            char buf[6];
            snprintf(buf, sizeof(buf), "%dx%d", DIFF[i].rows, DIFF[i].cols);
            d.setCursor(128 - (int16_t)strlen(buf) * 6 - 4, y);
            d.print(buf);
        }
        d.setTextColor(SSD1306_WHITE);
        d.display();
        return true;
    }

    // ── Win / Lose screen ─────────────────────────────────────────────────────
    if (s.gstate == WIN || s.gstate == LOSE) {
        const bool timeout = millis() - s.stateMs > 4000UL;
        if (btnPressed || shake || timeout) return false;

        d.clearDisplay();
        d.setTextSize(2); d.setTextColor(SSD1306_WHITE);
        if (s.gstate == WIN) {
            d.setCursor(16, 10); d.print("VITTORIA!");
            d.setTextSize(1);
            d.setCursor(20, 34); d.print("PUNTEGGIO:"); d.print(s.score);
        } else {
            d.setCursor(16, 10); d.print("GAME");
            d.setCursor(22, 28); d.print("OVER");
        }
        d.setTextSize(1);
        d.setCursor(10, 52); d.print("BTN/SCUOTI: rivinci");
        d.display();
        return true;
    }

    // ── Normal game ───────────────────────────────────────────────────────────
    d.clearDisplay();

    // HUD
    d.setTextSize(1); d.setTextColor(SSD1306_WHITE);
    d.setCursor(0, 0); d.print("SC:"); d.print(s.score);
    for (int i = 0; i < s.lives; i++) d.fillRect(128 - 4 - i * 6, 1, 4, 4, SSD1306_WHITE);
    d.drawFastHLine(0, 8, 128, SSD1306_WHITE);

    // Invaders
    for (int r = 0; r < s.rows; r++) for (int c = 0; c < s.cols; c++) {
        if (!s.alive[r][c]) continue;
        drawInvader(d,
            (int16_t)(s.blockBaseX + s.blockX + c * CELL_W),
            (int16_t)(BLOCK_BASE_Y + s.blockY + r * CELL_H),
            r);
    }

    // Bunkers
    for (int k = 0; k < NUM_BUNKERS; k++) drawBunker(d, s.bunkers[k]);

    // Ground line
    d.drawFastHLine(0, 51, 128, SSD1306_WHITE);

    // Ship
    drawShip(d, (int16_t)s.shipX, (int16_t)SHIP_Y);

    // Player bullets (2×4)
    for (int i = 0; i < MAX_PB; i++)
        if (s.pb[i].active)
            d.fillRect((int16_t)s.pb[i].x - 1, (int16_t)s.pb[i].y, 2, 4, SSD1306_WHITE);

    // Invader bullets (3×4)
    for (int i = 0; i < MAX_IB; i++)
        if (s.ib[i].active)
            d.fillRect((int16_t)s.ib[i].x - 1, (int16_t)s.ib[i].y, 3, 4, SSD1306_WHITE);

    d.display();
    return true;
}
