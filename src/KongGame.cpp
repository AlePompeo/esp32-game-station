#include "KongGame.h"
#include <Arduino.h>
#include <pgmspace.h>

// ── Sprite data ────────────────────────────────────────────────────────────────

// Mario 7×11 – frames: 0=StandL 1=StandR 2-7=Run(L/R x3) 8=JumpL 9=JumpR 10-11=Ladder
static const uint8_t PROGMEM spr_mario[] = {
    7, 11,
    // 0 Stand LHS
    0x00,0x0a,0xde,0xfb,0x9d,0x6b,0x06, 0x00,0x04,0x02,0x04,0x06,0x00,0x00,
    // 1 Stand RHS
    0x06,0x6b,0x9d,0xfb,0xde,0x0a,0x00, 0x00,0x00,0x06,0x04,0x02,0x04,0x00,
    // 2 Run01 LHS
    0x4a,0x1e,0xdb,0xfd,0xeb,0x26,0x40, 0x00,0x00,0x02,0x04,0x06,0x00,0x00,
    // 3 Run01 RHS
    0x40,0x26,0xeb,0xfd,0xdb,0x1e,0x4a, 0x00,0x00,0x06,0x04,0x02,0x00,0x00,
    // 4 Run02 LHS
    0x0a,0x1e,0xdb,0xfd,0xeb,0x66,0x00, 0x00,0x00,0x02,0x00,0x06,0x00,0x00,
    // 5 Run02 RHS
    0x00,0x66,0xeb,0xfd,0xdb,0x1e,0x0a, 0x00,0x00,0x06,0x00,0x02,0x00,0x00,
    // 6 Run03 LHS
    0x0a,0x1e,0xdb,0xfd,0xab,0x26,0x00, 0x00,0x04,0x02,0x00,0x06,0x00,0x00,
    // 7 Run03 RHS
    0x00,0x26,0xab,0xfd,0xdb,0x1e,0x0a, 0x00,0x00,0x06,0x00,0x02,0x04,0x00,
    // 8 Jump LHS
    0x4a,0x1e,0xdb,0xfd,0xeb,0x26,0x40, 0x00,0x04,0x02,0x00,0x02,0x02,0x00,
    // 9 Jump RHS
    0x40,0x26,0xeb,0xfd,0xdb,0x1e,0x4a, 0x00,0x02,0x02,0x00,0x02,0x04,0x00,
    // 10 Ladder01
    0x40,0x22,0xeb,0xfb,0xeb,0x22,0x10, 0x00,0x00,0x06,0x00,0x02,0x00,0x00,
    // 11 Ladder02
    0x10,0x22,0xeb,0xfb,0xeb,0x22,0x40, 0x00,0x00,0x02,0x00,0x06,0x00,0x00,
};

// Barrel 9×9 – 3 roll frames
static const uint8_t PROGMEM spr_barrel[] = {
    9, 9,
    0x00,0x38,0x54,0x82,0xd6,0x82,0x54,0x38,0x00, 0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x38,0x4c,0xc2,0x92,0x86,0x64,0x38,0x00, 0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x38,0x64,0x86,0x92,0xc2,0x4c,0x38,0x00, 0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
};

// Gorilla 20×20 – 0=Stand 1=MoveL 2=MoveR 3-6=Throw01-04
static const uint8_t PROGMEM spr_gorilla[] = {
    20, 20,
    // 0 Standing
    0x00,0x00,0x00,0xc0,0x38,0x04,0xc2,0x79,0x65,0x51,0x51,0x65,0x79,0xc2,0x04,0x38,0xc0,0x00,0x00,0x00,
    0x38,0xc4,0x02,0x13,0xe4,0xe9,0xa2,0xd7,0x35,0xe5,0xe5,0x35,0xd7,0xa2,0xe9,0xe4,0x13,0x02,0xc4,0x38,
    0x00,0x0c,0x05,0x0a,0x08,0x09,0x0d,0x0a,0x01,0x01,0x01,0x01,0x0a,0x0d,0x09,0x08,0x0a,0x05,0x0c,0x00,
    // 1 Moving Left
    0x00,0x00,0x00,0x00,0xc0,0x38,0x44,0xda,0xe5,0xf9,0xc5,0x33,0x8a,0x74,0x88,0x70,0x80,0x00,0x00,0x00,
    0x00,0x00,0x00,0x43,0x24,0x9b,0x07,0x06,0x97,0x33,0x79,0x38,0x10,0x10,0x00,0x80,0x01,0x06,0x08,0xf0,
    0x00,0x06,0x0d,0x0f,0x07,0x00,0x08,0x09,0x06,0x0d,0x0f,0x07,0x00,0x08,0x06,0x09,0x08,0x00,0x0e,0x01,
    // 2 Moving Right
    0x00,0x00,0x00,0x80,0x70,0x88,0x74,0x8a,0x33,0xc5,0xf9,0xe5,0xda,0x44,0x38,0xc0,0x00,0x00,0x00,0x00,
    0xf0,0x08,0x06,0x01,0x80,0x00,0x10,0x10,0x38,0x79,0x33,0x97,0x06,0x07,0x9b,0x24,0x43,0x00,0x00,0x00,
    0x01,0x0e,0x00,0x08,0x09,0x06,0x08,0x00,0x07,0x0f,0x0d,0x06,0x09,0x08,0x00,0x07,0x0f,0x0d,0x06,0x00,
    // 3 Throw01
    0xc0,0x30,0x08,0x08,0x7c,0xf4,0xc2,0x1d,0x2b,0x41,0x6b,0x41,0x2b,0x9e,0xc4,0x7c,0x08,0x08,0x30,0xc0,
    0x07,0x38,0xc0,0x12,0xc4,0xe8,0xe2,0xf7,0xf5,0x65,0x65,0xf5,0xf7,0xe2,0xe8,0xc4,0x12,0xc0,0x38,0x07,
    0x00,0x0c,0x05,0x0a,0x08,0x09,0x0d,0x0b,0x01,0x01,0x01,0x01,0x0b,0x0d,0x09,0x08,0x0a,0x05,0x0c,0x00,
    // 4 Throw02
    0x00,0x00,0x00,0xc0,0x38,0x04,0x02,0xd9,0xa5,0x11,0xb1,0x15,0xa1,0xc2,0x04,0x38,0xc0,0x00,0x00,0x00,
    0x78,0x84,0x82,0x03,0xcf,0xcf,0xec,0xf1,0xe2,0x64,0x66,0xe4,0xf2,0xe9,0xcc,0xc7,0x03,0x82,0x84,0x78,
    0x00,0x0c,0x05,0x0a,0x08,0x09,0x0d,0x0b,0x01,0x01,0x01,0x01,0x0b,0x0d,0x09,0x08,0x0a,0x05,0x0c,0x00,
    // 5 Throw03
    0x00,0x00,0x00,0xc0,0x38,0x04,0xc2,0x79,0x65,0x51,0x51,0x65,0x79,0xc2,0x04,0x38,0xc0,0x00,0x00,0x00,
    0x70,0x88,0x84,0x03,0xc4,0xd9,0x82,0x38,0x54,0x82,0xd6,0x82,0x54,0x38,0x81,0xc4,0x03,0x84,0x88,0x70,
    0x00,0x0c,0x05,0x0a,0x08,0x09,0x0d,0x0b,0x02,0x00,0x00,0x02,0x0a,0x0d,0x09,0x08,0x0a,0x05,0x0c,0x00,
    // 6 Throw04
    0x00,0x00,0x00,0xc0,0x38,0x04,0xc2,0x79,0x65,0x51,0x51,0x65,0x79,0xc2,0x04,0x38,0xc0,0x00,0x00,0x00,
    0x38,0xc4,0x02,0x13,0xe4,0xe9,0x22,0x97,0x45,0x25,0x65,0x25,0x47,0x82,0x29,0xe4,0x13,0x02,0xc4,0x38,
    0x00,0x0c,0x05,0x0a,0x08,0x09,0x08,0x03,0x05,0x08,0x0d,0x08,0x05,0x03,0x08,0x08,0x0a,0x05,0x0c,0x00,
};

// Fire 5×8 – 4 flicker frames (patrols floor 0 from level 3+)
static const uint8_t PROGMEM spr_fire[] = {
    5, 8,
    // frame 0: symmetric
    0x06, 0x1F, 0x1E, 0x1F, 0x06,
    // frame 1: tall centre
    0x0E, 0x0E, 0x1F, 0x0E, 0x0E,
    // frame 2: lean left
    0x07, 0x1F, 0x1E, 0x1C, 0x08,
    // frame 3: lean right
    0x08, 0x1C, 0x1E, 0x1F, 0x07,
};

// Item (Pauline's hat) 5×5 – 1 frame
static const uint8_t PROGMEM spr_item[] = {
    5, 5,
    0x02, 0x0F, 0x0F, 0x0F, 0x02,
};

// ── Level layout constants ──────────────────────────────────────────────────

static const int16_t FLOOR_Y[4]  = {63, 50, 37, 20};
static const int16_t PLAT_L[4]   = {  0,  0,  5,  0};
static const int16_t PLAT_R[4]   = {127,122,127,127};
static const int8_t  BAR_DIR[4]  = {-1, +1, -1, +1};

// Two ladder layouts alternate each level
static const int16_t LAD_X_A[3] = {28, 95, 50};
static const int16_t LAD_X_B[3] = {18, 80, 60};

static constexpr float  BAR_SPD    = 30.0f;
static constexpr float  BAR_GRAV   = 280.0f;
static constexpr float  P_SPD      = 40.0f;
static constexpr float  JUMP_VY    = -65.0f;
static constexpr float  P_GRAV     = 210.0f;
static constexpr float  CLIMB_SPD  = 22.0f;
static constexpr float  FIRE_SPD   = 12.0f;
static constexpr int16_t DK_SX    = 2;
static constexpr int16_t PAUL_X   = 99;

// ── Helpers ────────────────────────────────────────────────────────────────

const int16_t* KongGame::getLadderX() const {
    return (_layout == 0) ? LAD_X_A : LAD_X_B;
}

// ── Sprite renderer ────────────────────────────────────────────────────────

void KongGame::drawSprite(Adafruit_SSD1306& d, int16_t sx, int16_t sy,
                          const uint8_t* spr, uint8_t frame) const {
    const uint8_t w     = pgm_read_byte(spr);
    const uint8_t h     = pgm_read_byte(spr + 1);
    const uint8_t pages = (h + 7u) >> 3u;
    const uint8_t* data = spr + 2 + (uint16_t)frame * pages * w;
    uint8_t* buf        = d.getBuffer();
    const uint8_t yShift = (uint8_t)((uint16_t)sy & 7u);

    for (uint8_t sp = 0; sp < pages; sp++) {
        const int16_t dp0 = (sy >> 3) + (int16_t)sp;
        const int16_t dp1 = dp0 + 1;
        for (uint8_t sc = 0; sc < w; sc++) {
            const int16_t dc = sx + (int16_t)sc;
            if (dc < 0 || dc >= 128) continue;
            const uint8_t bits = pgm_read_byte(data + sp * w + sc);
            if (!bits) continue;
            if (dp0 >= 0 && dp0 < 8) buf[(uint16_t)dp0 * 128u + (uint16_t)dc] |= bits << yShift;
            if (yShift && dp1 >= 0 && dp1 < 8)
                buf[(uint16_t)dp1 * 128u + (uint16_t)dc] |= bits >> (8u - yShift);
        }
    }
}

// ── Game lifecycle ─────────────────────────────────────────────────────────

void KongGame::begin() {
    _bestScore   = 0;
    _state       = WAITING;
    _lastBtn     = false;
    _titleBX     = -10.0f;
    _titleBFrame = 0;
    _titleBAcc   = 0.0f;
    resetRound(true);
}

void KongGame::resetRound(bool fullReset) {
    if (fullReset) {
        _score = 0;
        _level = 1;
        _lives = START_LIVES;
    }
    _layout     = (_level - 1) % 2;
    _stateTimer = 0.0f;
    _scoreAcc   = 0.0f;

    _px         = 107.0f;
    _py         = (float)FLOOR_Y[0];
    _pvy        = 0.0f;
    _pFloor     = 0;
    _jumpFloor  = 0;
    _onLadder   = false;
    _ladderIdx  = -1;
    _facingRight= false;
    _runFrame   = 0;
    _runAcc     = 0.0f;

    for (auto& b : _barrels) b.floor = -2;
    float spawnBase = 4.5f - _level * 0.25f;
    if (spawnBase < 1.8f) spawnBase = 1.8f;
    _spawnTimer = spawnBase;

    _dkFrame     = 0;
    _dkAnim      = 0.0f;
    _dkIdleStep  = 0;
    _dkIdleAcc   = 0.0f;

    spawnFires();
    spawnItems();
}

// ── Fire enemies ───────────────────────────────────────────────────────────

void KongGame::spawnFires() {
    _fires[0].active   = (_level >= 3);
    _fires[0].fx       = 30.0f;
    _fires[0].dir      = +1;
    _fires[0].frame    = 0;
    _fires[0].frameAcc = 0.0f;

    _fires[1].active   = (_level >= 5);
    _fires[1].fx       = 90.0f;
    _fires[1].dir      = -1;
    _fires[1].frame    = 2;
    _fires[1].frameAcc = 0.0f;
}

void KongGame::updateFires(float dt) {
    const float spd = FIRE_SPD + _level * 1.5f;
    for (auto& f : _fires) {
        if (!f.active) continue;
        f.fx += (float)f.dir * spd * dt;
        if (f.dir > 0 && f.fx + FIRE_W >= (float)PLAT_R[0]) f.dir = -1;
        else if (f.dir < 0 && f.fx <= (float)PLAT_L[0])      f.dir = +1;
        f.frameAcc += dt;
        if (f.frameAcc >= 0.12f) {
            f.frameAcc = 0.0f;
            f.frame    = (f.frame + 1) % 4;
        }
    }
}

bool KongGame::checkFireCollision() const {
    if (_pFloor != 0) return false;
    const int16_t px = (int16_t)_px;
    for (const auto& f : _fires) {
        if (!f.active) continue;
        const int16_t fx = (int16_t)f.fx;
        if (px + MARIO_W < fx || px > fx + FIRE_W) continue;
        return true;
    }
    return false;
}

// ── Collectible items ──────────────────────────────────────────────────────

void KongGame::spawnItems() {
    // Positions safe on both layouts: well within platform bounds
    _items[0] = {50,  1, false};
    _items[1] = {70,  2, false};
    _items[2] = {45,  1, false};
}

void KongGame::checkItems() {
    const int16_t cx = (int16_t)_px + MARIO_W / 2;
    for (auto& it : _items) {
        if (it.collected) continue;
        if (_pFloor != it.floor) continue;
        if (abs(cx - it.x) < 6) {
            it.collected = true;
            _score += 50;
        }
    }
}

// ── Barrel spawn ───────────────────────────────────────────────────────────

void KongGame::spawnBarrel() {
    for (auto& b : _barrels) {
        if (b.floor == -2) {
            b.bx          = (float)(DK_SX + GOR_W + 4);
            b.bTop        = (float)(FLOOR_Y[3] - BARREL_H);
            b.bvy         = 0.0f;
            b.floor       = 3;
            b.targetFloor = 2;
            b.dir         = BAR_DIR[3];
            b.frame       = 0;
            b.frameAcc    = 0.0f;
            return;
        }
    }
}

// ── Update DK ─────────────────────────────────────────────────────────────

void KongGame::updateDK(float dt) {
    _dkAnim += dt;
    if (_dkFrame >= 3 && _dkFrame <= 5) {
        // throw animation cycles forward
        if (_dkAnim >= 0.15f) { _dkAnim = 0.0f; _dkFrame++; }
    } else if (_dkFrame == 6) {
        if (_dkAnim >= 0.15f) { _dkAnim = 0.0f; _dkFrame = 0; }
    } else {
        // idle sway: 0 → 1 → 0 → 2 → 0 → ...
        _dkIdleAcc += dt;
        if (_dkIdleAcc >= 0.35f) {
            _dkIdleAcc = 0.0f;
            _dkIdleStep = (_dkIdleStep + 1) % 4;
            static const uint8_t seq[4] = {0, 1, 0, 2};
            _dkFrame = seq[_dkIdleStep];
        }
    }
}

// ── Update player ─────────────────────────────────────────────────────────

void KongGame::updatePlayer(float dt, bool btn, float gravX, float gravY) {
    const bool btnDown = btn && !_lastBtn;

    if (_onLadder) {
        const int8_t li     = _ladderIdx;
        const float  topY   = (float)FLOOR_Y[li + 1];
        const float  botY   = (float)FLOOR_Y[li];

        if (gravY > 0.35f)       _py -= CLIMB_SPD * dt;
        else if (gravY < -0.35f) _py += CLIMB_SPD * dt;

        _runAcc += dt;
        if (_runAcc >= 0.22f) { _runAcc = 0.0f; _runFrame ^= 1; }

        if (_py <= topY) { _py = topY; _pFloor = li + 1; _onLadder = false; _ladderIdx = -1; }
        if (_py >= botY) { _py = botY; _pFloor = li;     _onLadder = false; _ladderIdx = -1; }
        return;
    }

    if (_pFloor < 0) {
        _pvy += P_GRAV * dt;
        _py  += _pvy * dt;
        for (int8_t f = 0; f <= 3; f++) {
            if (f > _jumpFloor) continue;  // can't land on floors above jump origin
            const float fy = (float)FLOOR_Y[f];
            if (_pvy > 0.0f && _py >= fy &&
                _px + 6.0f >= (float)PLAT_L[f] && _px <= (float)PLAT_R[f]) {
                _py = fy; _pvy = 0.0f; _pFloor = f; _runFrame = 0;
                break;
            }
        }
        return;
    }

    // Jump
    if (btnDown && _pvy == 0.0f) {
        _jumpFloor = _pFloor;
        _pvy       = JUMP_VY;
        _pFloor    = -1;
        _runFrame  = 8;
        _runAcc    = 0.0f;
        return;
    }

    // Enter ladder descending
    const int16_t* lx = getLadderX();
    if (gravY < -0.35f) {
        const int8_t li = nearLadder();
        if (li >= 0 && _pFloor > 0 && li == _pFloor - 1) {
            _px = (float)lx[li] - 3.0f; _onLadder = true; _ladderIdx = li; return;
        }
    }
    // Enter ladder ascending
    if (gravY > 0.35f) {
        const int8_t li = nearLadder();
        if (li >= 0 && _pFloor == li) {
            _px = (float)lx[li] - 3.0f; _onLadder = true; _ladderIdx = li; return;
        }
    }

    // Horizontal movement
    const float speed = P_SPD * constrain(gravX, -1.0f, 1.0f);
    if (fabsf(speed) > 2.0f) {
        _facingRight = (speed > 0.0f);
        _px += speed * dt;
        if (_px < (float)PLAT_L[_pFloor]) _px = (float)PLAT_L[_pFloor];
        if (_px + MARIO_W > (float)PLAT_R[_pFloor] + 1)
            _px = (float)(PLAT_R[_pFloor] + 1 - MARIO_W);
        _runAcc += dt;
        if (_runAcc >= 0.12f) { _runAcc = 0.0f; _runFrame = (_runFrame + 1) % 3; }
    } else {
        _runFrame = 0;
    }
}

// ── Update barrels ─────────────────────────────────────────────────────────

void KongGame::updateBarrels(float dt) {
    for (auto& b : _barrels) {
        if (b.floor == -2) continue;

        if (b.floor == -1) {
            b.bvy  += BAR_GRAV * dt;
            b.bTop += b.bvy * dt;
            const float targetY = (float)FLOOR_Y[b.targetFloor];
            if (b.bTop + BARREL_H >= targetY) {
                b.bTop = targetY - BARREL_H;
                b.bvy  = 0.0f;
                b.floor = b.targetFloor;
                b.dir   = BAR_DIR[b.floor];
            }
            continue;
        }

        const float barSpeed = BAR_SPD + _level * 3.0f;
        b.bx += (float)b.dir * barSpeed * dt;
        b.frameAcc += dt;
        if (b.frameAcc >= 0.10f) { b.frameAcc = 0.0f; b.frame = (b.frame + 1) % 3; }

        const float cx = b.bx + BARREL_W / 2.0f;
        if (b.dir > 0 && cx >= (float)PLAT_R[b.floor]) {
            if (b.floor > 0) { b.targetFloor = b.floor - 1; b.floor = -1; b.bvy = 0.0f; }
            else b.floor = -2;
        } else if (b.dir < 0 && cx <= (float)PLAT_L[b.floor]) {
            if (b.floor > 0) { b.targetFloor = b.floor - 1; b.floor = -1; b.bvy = 0.0f; }
            else b.floor = -2;
        }
        if (b.bx < -20.0f || b.bx > 148.0f) b.floor = -2;
    }
}

// ── Collision ──────────────────────────────────────────────────────────────

int8_t KongGame::nearLadder() const {
    const int16_t cx = (int16_t)_px + 3;
    const int16_t* lx = getLadderX();
    for (int8_t i = 0; i < 3; i++) {
        if (abs(cx - lx[i]) < 5) return i;
    }
    return -1;
}

bool KongGame::checkCollision() const {
    if (_pFloor < 0) return false;
    const int16_t px = (int16_t)_px;
    const int16_t py = (int16_t)_py;
    for (const auto& b : _barrels) {
        if (b.floor != _pFloor) continue;
        const int16_t bx = (int16_t)b.bx;
        const int16_t by = (int16_t)b.bTop;
        if (bx + 8 < px || bx > px + 6) continue;
        if (by + 8 < py - 10 || by > py) continue;
        if (py <= by) continue;
        return true;
    }
    return false;
}

// ── Draw scene ─────────────────────────────────────────────────────────────

void KongGame::drawScene(Adafruit_SSD1306& d) const {
    const int16_t* lx = getLadderX();

    // Floors
    for (int8_t f = 0; f < 4; f++) {
        const int16_t fy = FLOOR_Y[f];
        d.drawFastHLine(PLAT_L[f], fy, PLAT_R[f] - PLAT_L[f] + 1, SSD1306_WHITE);
    }

    // Ladders
    for (int8_t i = 0; i < 3; i++) {
        const int16_t lxi   = lx[i];
        const int16_t ladTop = FLOOR_Y[i + 1];
        const int16_t ladBot = FLOOR_Y[i];
        d.drawFastVLine(lxi,     ladTop, ladBot - ladTop, SSD1306_WHITE);
        d.drawFastVLine(lxi + 3, ladTop, ladBot - ladTop, SSD1306_WHITE);
        for (int16_t ry = ladTop + 3; ry < ladBot; ry += 5)
            d.drawFastHLine(lxi, ry, 4, SSD1306_WHITE);
    }

    // DK
    drawSprite(d, DK_SX, 0, spr_gorilla, _dkFrame);

    // Pauline (tiny figure)
    {
        const int16_t px = PAUL_X;
        const int16_t py = FLOOR_Y[3] - 1;
        d.fillRect(px + 1, py - 9, 3, 3, SSD1306_WHITE);
        d.drawFastVLine(px + 2, py - 6, 4, SSD1306_WHITE);
        d.drawFastHLine(px, py - 5, 5, SSD1306_WHITE);
        d.drawFastVLine(px + 1, py - 2, 3, SSD1306_WHITE);
        d.drawFastVLine(px + 3, py - 2, 3, SSD1306_WHITE);
        d.drawPixel(px + 2, py - 11, SSD1306_WHITE);
    }

    // Items
    for (const auto& it : _items) {
        if (it.collected) continue;
        drawSprite(d, it.x - 2, FLOOR_Y[it.floor] - ITEM_H - 1, spr_item, 0);
    }

    // Fires (floor 0)
    for (const auto& f : _fires) {
        if (!f.active) continue;
        drawSprite(d, (int16_t)f.fx, FLOOR_Y[0] - FIRE_H, spr_fire, f.frame);
    }

    // Barrels
    for (const auto& b : _barrels) {
        if (b.floor == -2) continue;
        drawSprite(d, (int16_t)b.bx, (int16_t)b.bTop, spr_barrel, b.frame);
    }

    // Player
    uint8_t mariFrame;
    if (_onLadder) {
        mariFrame = 10 + _runFrame;
    } else if (_pFloor < 0) {
        mariFrame = _facingRight ? 9 : 8;
    } else if (_runFrame > 0) {
        const uint8_t rf = ((_runFrame - 1) % 3);
        mariFrame = _facingRight ? (3 + rf * 2) : (2 + rf * 2);
    } else {
        mariFrame = _facingRight ? 1 : 0;
    }
    drawSprite(d, (int16_t)_px, (int16_t)_py - MARIO_H, spr_mario, mariFrame);
}

// ── HUD ────────────────────────────────────────────────────────────────────

void KongGame::drawHUD(Adafruit_SSD1306& d) const {
    d.setTextSize(1);
    d.setTextColor(SSD1306_WHITE);
    d.setCursor(38, 3);
    d.print("S:");
    d.print(_score);
    d.setCursor(38, 12);
    d.print("L:");
    d.print(_level);
    // Lives as filled dots
    for (uint8_t i = 0; i < _lives; i++) {
        d.fillRect(90 + (int16_t)i * 7, 12, 4, 4, SSD1306_WHITE);
    }
}

// ── Main update ────────────────────────────────────────────────────────────

bool KongGame::update(float dt, bool btnPressed, bool shake,
                      float gravX, float gravY, Adafruit_SSD1306& d) {
    const bool btnDown = btnPressed && !_lastBtn;

    switch (_state) {

    case WAITING: {
        // Animate title barrel rolling left→right
        _titleBAcc += dt;
        if (_titleBAcc >= 0.10f) { _titleBAcc = 0.0f; _titleBFrame = (_titleBFrame + 1) % 3; }
        _titleBX += 30.0f * dt;
        if (_titleBX > 135.0f) _titleBX = -10.0f;

        d.clearDisplay();
        d.setTextSize(2);
        d.setTextColor(SSD1306_WHITE);
        d.setCursor(22, 4);
        d.print("KONG");
        d.setTextSize(1);
        d.setCursor(10, 26);
        d.print("Inclina: muovi Mario");
        d.setCursor(10, 36);
        d.print("Su/Giu: scala");
        d.setCursor(10, 46);
        d.print("Pulsante: salta");
        // Rolling barrel on title screen
        drawSprite(d, (int16_t)_titleBX, 55, spr_barrel, _titleBFrame);
        // Press to start (blinks)
        if ((int)(_titleBX * 0.1f) & 1) {
            d.setCursor(22, 55);
            d.print(">> Premi <<");
        }
        d.display();
        if (btnDown) {
            resetRound(true);
            _state = PLAYING;
        }
        break;
    }

    case PLAYING: {
        _stateTimer += dt;
        _scoreAcc   += dt;
        if (_scoreAcc >= 0.1f) { _scoreAcc = 0.0f; _score++; }

        _spawnTimer -= dt;
        if (_spawnTimer <= 0.0f) {
            spawnBarrel();
            float spawnBase = 4.5f - _level * 0.25f;
            if (spawnBase < 1.5f) spawnBase = 1.5f;
            _spawnTimer = spawnBase;
            _dkFrame = 3;
            _dkAnim  = 0.0f;
        }

        updateDK(dt);
        updateBarrels(dt);
        updateFires(dt);
        updatePlayer(dt, btnPressed, gravX, gravY);
        checkItems();

        // Win: reach Pauline
        if (_pFloor == 3 && _px + MARIO_W > PAUL_X - 2) {
            _score += 100 + _level * 50;
            if (_score > _bestScore) _bestScore = _score;
            _state = WIN;
            _stateTimer = 0.0f;
        }

        // Death: barrel or fire
        if (checkCollision() || checkFireCollision()) {
            _state = DYING;
            _stateTimer = 0.0f;
        }

        d.clearDisplay();
        drawScene(d);
        drawHUD(d);
        d.display();
        break;
    }

    case DYING: {
        _stateTimer += dt;

        if (_stateTimer >= 1.4f) {
            d.invertDisplay(false);
            if (_lives > 0) _lives--;
            if (_lives == 0) {
                if (_score > _bestScore) _bestScore = _score;
                _state = GAME_OVER;
                _stateTimer = 0.0f;
            } else {
                resetRound(false);
                _state = PLAYING;
            }
            break;
        }

        d.clearDisplay();
        drawScene(d);
        drawHUD(d);
        // Fast blink effect
        if ((int)(_stateTimer * 10.0f) & 1) d.invertDisplay(true);
        else                                  d.invertDisplay(false);
        d.display();
        break;
    }

    case WIN: {
        _stateTimer += dt;
        d.clearDisplay();
        drawScene(d);
        d.drawRect(18, 20, 92, 24, SSD1306_WHITE);
        d.fillRect(19, 21, 90, 22, SSD1306_BLACK);
        d.setTextSize(1);
        d.setTextColor(SSD1306_WHITE);
        d.setCursor(24, 24);
        d.print("LIVELLO ");
        d.print(_level);
        d.print(" OK!");
        d.setCursor(24, 34);
        d.print("SCORE: ");
        d.print(_score);
        d.display();
        if (_stateTimer > 2.5f || btnDown) {
            _level++;
            resetRound(false);
            _state = PLAYING;
        }
        break;
    }

    case GAME_OVER: {
        _stateTimer += dt;
        d.clearDisplay();
        d.drawRect(8, 10, 112, 44, SSD1306_WHITE);
        d.fillRect(9, 11, 110, 42, SSD1306_BLACK);
        d.setTextSize(2);
        d.setTextColor(SSD1306_WHITE);
        d.setCursor(16, 14);
        d.print("GAME");
        d.setCursor(16, 30);
        d.print("OVER");
        d.setTextSize(1);
        d.setCursor(72, 14);
        d.print("S:");
        d.print(_score);
        if (_bestScore > 0) {
            d.setCursor(72, 24);
            d.print("R:");
            d.print(_bestScore);
        }
        d.setCursor(20, 48);
        d.print("Premi per ricominciare");
        d.display();
        if (btnDown) {
            _lastBtn = true;
            return false;
        }
        break;
    }

    }

    _lastBtn = btnPressed;
    return true;
}
