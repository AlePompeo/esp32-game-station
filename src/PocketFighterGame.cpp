#include "PocketFighterGame.h"
#include <Arduino.h>
#include <EEPROM.h>
#include <string.h>

// ─── Sprite data (Arduboy/SSD1306 page-format) ───────────────────────────────
// Each byte = 1 column of 8 vertical pixels, bit0 = topmost pixel.
// Sprites from github.com/ArduboyCollection/pocket_fighter (MIT licence).

static const uint8_t PROGMEM SPR_HEART[] = {
    0x00, 0x1c, 0x3e, 0x7e, 0xfc, 0x5e, 0x26, 0x1c
};

static const uint8_t PROGMEM SPR_HEAD[2][8] = {
    { 0x7e, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x7e },  // player (filled)
    { 0x7e, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x7e }   // CPU (hollow)
};

static const uint8_t PROGMEM SPR_BODY_IDLE[2][8] = {
    { 0x8c, 0x8c, 0x80, 0x00, 0x00, 0x80, 0x8c, 0x8c },
    { 0x98, 0x98, 0x80, 0x00, 0x00, 0x80, 0x98, 0x98 }
};

static const uint8_t PROGMEM SPR_BODY_MOVE[2][8] = {
    { 0x48, 0x48, 0x40, 0x00, 0x00, 0x80, 0x8c, 0x8c },
    { 0x8c, 0x8c, 0x80, 0x00, 0x00, 0x40, 0x48, 0x48 }
};

// Punch facing right: 3 frames × 11 columns
static const uint8_t PROGMEM SPR_BODY_PUNCH0[3][11] = {
    { 0x8c, 0x8c, 0x80, 0x00, 0x00, 0x80, 0x8c, 0x8c, 0x00, 0x00, 0x00 },
    { 0x8c, 0x8c, 0x80, 0x00, 0x00, 0x80, 0x80, 0x80, 0x0c, 0x0c, 0x00 },
    { 0x80, 0x80, 0x80, 0x00, 0x00, 0x80, 0x8c, 0x8c, 0x00, 0x0c, 0x0c }
};
// Punch facing left: 3 frames × 11 columns
static const uint8_t PROGMEM SPR_BODY_PUNCH1[3][11] = {
    { 0x00, 0x00, 0x00, 0x8c, 0x8c, 0x80, 0x00, 0x00, 0x80, 0x8c, 0x8c },
    { 0x00, 0x0c, 0x0c, 0x80, 0x80, 0x80, 0x00, 0x00, 0x80, 0x8c, 0x8c },
    { 0x0c, 0x0c, 0x00, 0x8c, 0x8c, 0x80, 0x00, 0x00, 0x80, 0x80, 0x80 }
};

// Hadouken pose facing right: 2 frames × 11 columns
static const uint8_t PROGMEM SPR_BODY_SHOOT0[2][11] = {
    { 0x80, 0x80, 0x80, 0x00, 0x00, 0x80, 0x80, 0x80, 0x36, 0x36, 0x00 },
    { 0x80, 0x80, 0x80, 0x00, 0x00, 0x80, 0x80, 0x80, 0x36, 0x36, 0x00 }
};
// Hadouken pose facing left: 2 frames × 11 columns
static const uint8_t PROGMEM SPR_BODY_SHOOT1[2][11] = {
    { 0x00, 0x36, 0x36, 0x80, 0x80, 0x80, 0x00, 0x00, 0x80, 0x80, 0x80 },
    { 0x00, 0x36, 0x36, 0x80, 0x80, 0x80, 0x00, 0x00, 0x80, 0x80, 0x80 }
};

static const uint8_t PROGMEM SPR_BULLET[] = { 0x03, 0x03 };

// Title screen: 64×32 pixels (4 pages × 64 bytes = 256 bytes)
static const uint8_t PROGMEM SPR_TITLE[] = {
    0x00, 0x00, 0x00, 0xfc, 0x02, 0x02, 0x00, 0xfe, 0x00, 0x02, 0xfc, 0x00, 0xfc, 0x00, 0x02, 0xfc,
    0x00, 0xfc, 0x00, 0x02, 0x04, 0x00, 0xfe, 0x00, 0x90, 0x0e, 0x00, 0xfe, 0x00, 0x02, 0x02, 0x00,
    0x02, 0x00, 0xfe, 0x02, 0x02, 0x00, 0x02, 0x02, 0x02, 0x02, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x02, 0x02, 0xfc, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0xff, 0x00, 0x00, 0x00, 0x7f, 0x00, 0x01, 0x00, 0x00, 0x3f, 0x00, 0x40, 0x3f,
    0x00, 0x3f, 0x00, 0x40, 0x20, 0x00, 0x7f, 0x00, 0x03, 0x7c, 0x00, 0x7f, 0x00, 0x41, 0x41, 0x00,
    0x00, 0x00, 0x7f, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xff, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0xff, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0xfe, 0x00, 0x82, 0x82, 0x00, 0x02, 0x00, 0xfe, 0x02, 0x00, 0xfc,
    0x00, 0x82, 0x84, 0x00, 0xfe, 0x00, 0x80, 0xfe, 0x00, 0x02, 0x00, 0xfe, 0x02, 0x02, 0x00, 0xfe,
    0x00, 0x02, 0x02, 0x00, 0xfe, 0x00, 0x02, 0xfc, 0x00, 0x00, 0x00, 0xff, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x3f, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40,
    0x40, 0x40, 0x40, 0x40, 0x00, 0x7f, 0x00, 0x00, 0x00, 0x00, 0x40, 0x00, 0x7f, 0x40, 0x00, 0x3f,
    0x00, 0x40, 0x7f, 0x00, 0x7f, 0x00, 0x00, 0x7f, 0x00, 0x00, 0x00, 0x7f, 0x00, 0x00, 0x00, 0x7f,
    0x00, 0x41, 0x41, 0x00, 0x7f, 0x00, 0x01, 0x7e, 0x00, 0x40, 0x40, 0x3f, 0x00, 0x00, 0x00, 0x00
};

// "NEXT LEVEL" screen: 64×32 pixels
static const uint8_t PROGMEM SPR_NEXT[] = {
    0x00, 0x00, 0x00, 0xfc, 0x02, 0x02, 0x00, 0xfe, 0x00, 0xe0, 0x00, 0xfe, 0x00, 0xfe, 0x00, 0x02,
    0x02, 0x00, 0x0e, 0x10, 0x80, 0x70, 0x0e, 0x00, 0x02, 0x00, 0xfe, 0x02, 0x02, 0x00, 0x00, 0xfe,
    0x00, 0x00, 0x00, 0x00, 0xfe, 0x00, 0x02, 0x02, 0x00, 0xfe, 0x00, 0x00, 0x00, 0xfe, 0x00, 0xfe,
    0x00, 0x02, 0x02, 0x00, 0xfe, 0x00, 0x00, 0x00, 0x00, 0x02, 0x02, 0xfc, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0xff, 0x00, 0x00, 0x00, 0x7f, 0x00, 0x00, 0x07, 0x7f, 0x00, 0x7f, 0x00, 0x41,
    0x41, 0x00, 0x78, 0x04, 0x00, 0x07, 0x78, 0x00, 0x00, 0x00, 0x7f, 0x00, 0x00, 0x00, 0x00, 0x7f,
    0x00, 0x40, 0x40, 0x00, 0x7f, 0x00, 0x41, 0x41, 0x00, 0x07, 0x00, 0x40, 0x38, 0x07, 0x00, 0x7f,
    0x00, 0x41, 0x41, 0x00, 0x7f, 0x00, 0x40, 0x40, 0x00, 0x00, 0x00, 0xff, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0xff, 0x00, 0x00, 0x00, 0xfe, 0x00, 0x02, 0xfc, 0x00, 0xfe, 0x00, 0x02, 0xfc,
    0x00, 0xfe, 0x00, 0x02, 0x02, 0x00, 0x7c, 0x00, 0x02, 0x04, 0x00, 0x7c, 0x00, 0x02, 0x04, 0x00,
    0x00, 0x00, 0x00, 0xfe, 0x00, 0x02, 0xfc, 0x00, 0xfc, 0x00, 0x02, 0xfc, 0x00, 0xfe, 0x00, 0xfe,
    0x00, 0xfe, 0x00, 0xfe, 0x00, 0xe0, 0x00, 0xfe, 0x00, 0x00, 0x00, 0xff, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x3f, 0x40, 0x40, 0x00, 0x7f, 0x00, 0x01, 0x00, 0x00, 0x7f, 0x00, 0x01, 0x7e,
    0x00, 0x7f, 0x00, 0x41, 0x41, 0x00, 0x20, 0x00, 0x41, 0x3e, 0x00, 0x20, 0x00, 0x41, 0x3e, 0x00,
    0x00, 0x00, 0x00, 0x7f, 0x00, 0x40, 0x3f, 0x00, 0x3f, 0x00, 0x40, 0x3f, 0x00, 0x3f, 0x00, 0x3f,
    0x40, 0x3f, 0x00, 0x7f, 0x00, 0x00, 0x07, 0x7f, 0x00, 0x40, 0x40, 0x3f, 0x00, 0x00, 0x00, 0x00
};

// "GAME OVER" screen: 64×32 pixels
static const uint8_t PROGMEM SPR_OVER[] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xc0, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20,
    0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20,
    0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20,
    0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0xc0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0xc0, 0x60, 0x20, 0xe0, 0x20, 0x60, 0xc0, 0x00, 0xfc, 0x00, 0x82, 0x84, 0x00, 0x00, 0x00, 0x06,
    0x78, 0x80, 0x00, 0xfe, 0x00, 0x80, 0x7c, 0xfe, 0x00, 0xfe, 0x00, 0x02, 0x02, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0xfc, 0x00, 0x02, 0xfc, 0x00, 0xfe, 0x00, 0x00, 0x00, 0xfe, 0x00, 0xfe,
    0x00, 0x02, 0x02, 0x00, 0xfe, 0x00, 0x02, 0xfc, 0x00, 0xc0, 0x60, 0x20, 0xe0, 0x20, 0x60, 0xc0,
    0x01, 0x03, 0x01, 0x03, 0x01, 0x03, 0x01, 0x00, 0x3f, 0x00, 0x40, 0x7f, 0x00, 0x7f, 0x00, 0x04,
    0x04, 0x7f, 0x00, 0x7f, 0x00, 0x7f, 0x00, 0x7f, 0x00, 0x7f, 0x00, 0x41, 0x41, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x3f, 0x00, 0x40, 0x3f, 0x00, 0x07, 0x00, 0x40, 0x38, 0x07, 0x00, 0x7f,
    0x00, 0x41, 0x41, 0x00, 0x7f, 0x00, 0x01, 0x7e, 0x00, 0x01, 0x03, 0x01, 0x03, 0x01, 0x03, 0x01,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x03, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04,
    0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04,
    0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04,
    0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

// ─── Page-format bitmap renderer ─────────────────────────────────────────────
// Same algorithm as DinoGame — handles any (x,y) incl. sub-page-aligned coords.
void PocketFighterGame::drawPageBitmap(Adafruit_SSD1306& d, int16_t x, int16_t y,
                                        const uint8_t* bmp, uint8_t w, uint8_t h) const {
    if (x + w <= 0 || x >= 128 || y + h <= 0 || y >= 64) return;
    uint8_t* buf = d.getBuffer();
    const uint8_t srcPages = (h + 7) >> 3;
    const uint8_t yShift   = (uint8_t)(y & 7);
    for (uint8_t sp = 0; sp < srcPages; sp++) {
        const int16_t dp0 = (y >> 3) + (int16_t)sp;
        const int16_t dp1 = dp0 + 1;
        for (uint8_t col = 0; col < w; col++) {
            const int16_t dc = x + (int16_t)col;
            if (dc < 0 || dc >= 128) continue;
            const uint8_t bits = pgm_read_byte(&bmp[sp * w + col]);
            if (!bits) continue;
            if (dp0 >= 0 && dp0 < 8) buf[dp0 * 128 + dc] |= (uint8_t)(bits << yShift);
            if (yShift > 0 && dp1 >= 0 && dp1 < 8)
                buf[dp1 * 128 + dc] |= (uint8_t)(bits >> (8 - yShift));
        }
    }
}

// ─── Collision helpers ────────────────────────────────────────────────────────
bool PocketFighterGame::pointInRect(int16_t px, int16_t py,
                                     int16_t rx, int16_t ry, int16_t rw, int16_t rh) {
    return px >= rx && px < rx + rw && py >= ry && py < ry + rh;
}
bool PocketFighterGame::rectsOverlap(int16_t x1, int16_t y1, int16_t w1, int16_t h1,
                                      int16_t x2, int16_t y2, int16_t w2, int16_t h2) {
    return x1 < x2 + w2 && x1 + w1 > x2 && y1 < y2 + h2 && y1 + h1 > y2;
}

// ─── Fighter action helpers ───────────────────────────────────────────────────
void PocketFighterGame::setFighterAction(Fighter& f, Actions act) {
    f.action = act;
    if (f.actionCache == act) f.actionCache = ACT_NONE;
    f.frameTicks = 0;
    f.frame = 0;
}

bool PocketFighterGame::isFighterAction(const Fighter& f, Actions act) const {
    return f.action == act;
}

void PocketFighterGame::setFighterActionCache(Fighter& f, Actions act) {
    f.actionCache = act;
}

// Called when an attack animation finishes; resumes movement or idle.
void PocketFighterGame::setFighterProperAction(Fighter& f, bool isSelf, float gravX) {
    if (f.dy == 0) {
        if (isSelf && (f.actionCache == ACT_LEFT || gravX < -MOVE_THRESH)) {
            addPlayerRecord(ACT_LEFT);
            setFighterAction(f, ACT_LEFT);
        } else if (isSelf && (f.actionCache == ACT_RIGHT || gravX > MOVE_THRESH)) {
            addPlayerRecord(ACT_RIGHT);
            setFighterAction(f, ACT_RIGHT);
        } else {
            f.action = ACT_IDLE;
        }
    } else {
        f.action = ACT_JUMP;
    }
    f.frameTicks = 0;
}

// ─── Jump helpers ─────────────────────────────────────────────────────────────
bool PocketFighterGame::isFighterJumping(const Fighter& f) const { return f.dy != 0; }

void PocketFighterGame::setFighterJump(Fighter& f) {
    uint8_t dy = f.dy + 2;
    if (dy > 5) dy = 5;
    f.dy = dy;
    f.jumpTicks = 0;
}

void PocketFighterGame::tickFighterJump(Fighter& f) {
    if (f.dy == 0) return;
    if (++f.jumpTicks < JMP_TICKS) {
        f.x += f.dx;
        f.y -= f.dy;
        if (f.y < BORDER_TOP) f.y = BORDER_TOP;
    } else if (++f.jumpTicks >= JMP_TICKS) {  // double increment mirrors original
        f.x += f.dx;
        f.y += f.dy;
        if (f.y >= BORDER_BOTTOM) {
            f.y = BORDER_BOTTOM;
            f.jumpTicks = 0;
            f.dx = f.dy = 0;
            if (f.action == ACT_JUMP) setFighterAction(f, ACT_IDLE);
        }
    }
}

// ─── Hadouken helpers ─────────────────────────────────────────────────────────
bool PocketFighterGame::isFighterShooting(const Fighter& f) const { return f.hadoukenLife != 0; }

void PocketFighterGame::fighterShoot(Fighter& f) {
    f.hadoukenLife = HDK_TICKS;
    if (f.dir == 0) { f.bx = f.x + 3; f.by = f.y - 6; }
    else             { f.bx = f.x - 3; f.by = f.y - 6; }
    f.dbx = f.dir;
}

void PocketFighterGame::finishFighterShoot(Fighter& f) { f.hadoukenLife = 0; }

void PocketFighterGame::tickFighterShoot(Fighter& f) {
    if (!isFighterShooting(f)) return;
    if (--f.hadoukenLife == 0) return;
    if (f.dbx == 0) {
        if (++f.bx >= SCREEN_RIGHT) finishFighterShoot(f);
        else if (++f.bx >= SCREEN_RIGHT) finishFighterShoot(f);
    } else {
        if (f.bx <= SCREEN_LEFT) finishFighterShoot(f);
        else if (--f.bx <= SCREEN_LEFT) finishFighterShoot(f);
        else if (--f.bx <= SCREEN_LEFT) finishFighterShoot(f);
    }
}

// ─── Record helpers ───────────────────────────────────────────────────────────
PocketFighterGame::Record& PocketFighterGame::getPlayerRecord() { return _r[_c0]; }
PocketFighterGame::Record& PocketFighterGame::getCpuRecord()    { return _r[_c1]; }

void PocketFighterGame::tickPlayerRecord(bool force) {
    if (++_t0 >= 1u || force) {
        _t0 = 0;
        uint16_t ts = getPlayerRecord().timeStamp++;
        if (getPlayerRecord().timeStamp < ts) getPlayerRecord().timeStamp = ts;
    }
}

void PocketFighterGame::addPlayerRecord(Actions act) {
    tickPlayerRecord(true);
    if (++_c0 >= RECORD_HALF) _c0 = 0;
    _r[_c0].action    = act;
    _r[_c0].timeStamp = 0;
    _t0 = 0;
}

void PocketFighterGame::nextCpuRecord() {
    if (++_c1 >= RECORD_TOTAL) _c1 = RECORD_HALF;
    else if (_r[_c1].action == ACT_NONE) _c1 = RECORD_HALF;
    _t1 = 0;
}

// ─── CPU flip ─────────────────────────────────────────────────────────────────
void PocketFighterGame::flipCpu(Fighter& f) {
    f.dir    = (f.dir    == 0) ? 1 : 0;
    f.mirror = (f.mirror == 0) ? 1 : 0;
}

// ─── Collision detection ──────────────────────────────────────────────────────
void PocketFighterGame::detectCollision(Fighter& attacker, Fighter& defender) {
    // Hadouken vs body
    if (isFighterShooting(attacker)) {
        if (pointInRect(attacker.bx, attacker.by,
                        (int16_t)(defender.x - 3), (int16_t)(defender.y - 14), 8, 15)) {
            if (defender.hp > 0) --defender.hp;
            finishFighterShoot(attacker);
            return;
        }
    }
    // Punch fist vs body
    if (attacker.tookPunch == 0 && isFighterAction(attacker, ACT_PUNCH)) {
        int16_t fx = (attacker.dir == 0) ? (int16_t)(attacker.x + 3)
                                         : (int16_t)(attacker.x - 2);
        if (rectsOverlap(fx, (int16_t)(attacker.y - 6), 5, 4,
                         (int16_t)(defender.x - 3), (int16_t)(defender.y - 14), 8, 15)) {
            if (defender.hp > 0) --defender.hp;
            attacker.tookPunch = 1;
        }
    }
}

// ─── Draw fighter ─────────────────────────────────────────────────────────────
// Returns true when attack animation just finished (triggers action resume).
bool PocketFighterGame::drawFighter(Fighter& f, bool isSelf, Adafruit_SSD1306& d) {
    bool atkFinished = false;
    // Head
    drawPageBitmap(d, (int16_t)(f.x - 3), (int16_t)(f.y - 14),
                   SPR_HEAD[isSelf ? 0 : 1], 8, 8);
    // Body
    switch (f.action) {
        case ACT_LEFT:
        case ACT_RIGHT:
        case ACT_JUMP:
            if (f.frame >= 2) f.frame = 0;
            drawPageBitmap(d, (int16_t)(f.x - 3), (int16_t)(f.y - 7),
                           SPR_BODY_MOVE[f.frame], 8, 8);
            break;
        case ACT_IDLE:
        default:
            if (f.frame >= 2) f.frame = 0;
            drawPageBitmap(d, (int16_t)(f.x - 3), (int16_t)(f.y - 7),
                           SPR_BODY_IDLE[f.frame], 8, 8);
            break;
        case ACT_PUNCH:
            if (f.dir == 0) {
                if (f.frame >= 3) { f.frame = 0; f.tookPunch = 0; atkFinished = true; }
                if (!atkFinished)
                    drawPageBitmap(d, (int16_t)(f.x - 3), (int16_t)(f.y - 7),
                                   SPR_BODY_PUNCH0[f.frame], 11, 8);
            } else {
                if (f.frame >= 3) { f.frame = 0; f.tookPunch = 0; atkFinished = true; }
                if (!atkFinished)
                    drawPageBitmap(d, (int16_t)(f.x - 6), (int16_t)(f.y - 7),
                                   SPR_BODY_PUNCH1[f.frame], 11, 8);
            }
            break;
        case ACT_SHOOT:
            if (f.dir == 0) {
                if (f.frame >= 2) { f.frame = 0; atkFinished = true; }
                if (!atkFinished)
                    drawPageBitmap(d, (int16_t)(f.x - 3), (int16_t)(f.y - 7),
                                   SPR_BODY_SHOOT0[f.frame], 11, 8);
            } else {
                if (f.frame >= 2) { f.frame = 0; atkFinished = true; }
                if (!atkFinished)
                    drawPageBitmap(d, (int16_t)(f.x - 6), (int16_t)(f.y - 7),
                                   SPR_BODY_SHOOT1[f.frame], 11, 8);
            }
            break;
    }
    // Draw hadouken if active
    if (isFighterShooting(f))
        drawPageBitmap(d, (int16_t)f.bx, (int16_t)f.by, SPR_BULLET, 2, 8);
    return atkFinished;
}

// ─── HUD ──────────────────────────────────────────────────────────────────────
void PocketFighterGame::drawHUD(Adafruit_SSD1306& d) {
    for (uint8_t i = 0; i < _f[0].hp; ++i)
        drawPageBitmap(d, (int16_t)(8 + 8 * i), 0, SPR_HEART, 8, 8);
    for (uint8_t i = 0; i < _f[1].hp; ++i)
        drawPageBitmap(d, (int16_t)(110 - 8 * i), 0, SPR_HEART, 8, 8);
    d.drawFastHLine(0, BORDER_BOTTOM + 1, 128, SSD1306_WHITE);
    drawProgress(d);
}

void PocketFighterGame::drawProgress(Adafruit_SSD1306& d) {
    d.setTextSize(1);
    d.setTextColor(SSD1306_WHITE);
    d.setCursor(4, BORDER_BOTTOM + 3);
    d.print("LV ");
    d.print((int)_level);
    d.print(" BEST ");
    d.print((int)_best);
}

// ─── prepare() ────────────────────────────────────────────────────────────────
void PocketFighterGame::prepare() {
    if (++_level > _best) {
        _best = _level;
        EEPROM.put(EEPROM_ADDR, _best);
        EEPROM.commit();
    }
    uint8_t i = 0, j = 1;
    if ((_level % 2) == 0) { i = 1; j = 0; }

    auto initFighter = [](Fighter& f, uint8_t d_, uint8_t x_) {
        f.dir = d_; f.mirror = 0;
        f.x = x_;  f.y = BORDER_BOTTOM;
        f.dx = f.dy = 0;
        f.bx = f.by = f.dbx = 0;
        f.hadoukenLife = f.tookPunch = f.jumpTicks = 0;
        f.action = ACT_IDLE; f.actionCache = ACT_NONE;
        f.frameTicks = f.frame = 0;
    };
    initFighter(_f[i], 0, BORDER_LEFT + 16);
    initFighter(_f[j], 1, BORDER_RIGHT - 16);

    if (_f[0].hp < 3) ++_f[0].hp;
    _f[1].hp = 3;

    _c0 = 0;
    _c1 = RECORD_HALF;
    _t0 = _t1 = 0;
    memmove(_r + RECORD_HALF, _r, RECORD_HALF * sizeof(Record));
    memset(_r, 0, RECORD_HALF * sizeof(Record));
}

// ─── State: MENU ──────────────────────────────────────────────────────────────
void PocketFighterGame::doMenu(bool btnEdge, Adafruit_SSD1306& d) {
    if (btnEdge) {
        prepare();
        _state = STATE_FIGHT;
        _stateTimer = 0;
        return;
    }
    ++_stateTimer;
    // Title logo (64×32) at (16, 14)
    drawPageBitmap(d, 16, 14, SPR_TITLE, 64, 32);
    // Animated fighter on top of title
    uint8_t fr = (_stateTimer % (uint16_t)(AFPT * 2)) < AFPT ? 0 : 1;
    drawPageBitmap(d, 62, 16, SPR_HEAD[0], 8, 8);
    drawPageBitmap(d, 62, 24, SPR_BODY_IDLE[fr], 8, 8);
    // Controls hint
    d.setTextSize(1);
    d.setTextColor(SSD1306_WHITE);
    d.setCursor(4, 50);
    d.print("BTN:COMBATTI");
    // Best level
    if (_best > 1) {
        char buf[12];
        snprintf(buf, sizeof(buf), "BEST:%d", (int)_best);
        d.setCursor(80, 50);
        d.print(buf);
    }
}

// ─── State: FIGHT ─────────────────────────────────────────────────────────────
void PocketFighterGame::doFight(float gravX, float /*gravY*/,
                                 bool tiltLeft,  bool tiltRight,
                                 bool justLeft,  bool justRight, bool justJump,
                                 bool leftRel,   bool rightRel,
                                 bool shakeEdge, bool btnEdge,
                                 Adafruit_SSD1306& d) {
    Fighter& p = _f[0];  // player
    Fighter& c = _f[1];  // CPU

    // ── Player input ──────────────────────────────────────────────────────────
    bool notAttacking = !isFighterAction(p, ACT_PUNCH) && !isFighterAction(p, ACT_SHOOT);

    if (btnEdge && notAttacking) {
        addPlayerRecord(ACT_PUNCH);
        setFighterAction(p, ACT_PUNCH);
        if (tiltLeft)  setFighterActionCache(p, ACT_LEFT);
        else if (tiltRight) setFighterActionCache(p, ACT_RIGHT);
    } else if (shakeEdge && notAttacking && !isFighterShooting(p)) {
        addPlayerRecord(ACT_SHOOT);
        setFighterAction(p, ACT_SHOOT);
        fighterShoot(p);
        if (tiltLeft)  setFighterActionCache(p, ACT_LEFT);
        else if (tiltRight) setFighterActionCache(p, ACT_RIGHT);
    } else if (justLeft && notAttacking) {
        addPlayerRecord(ACT_LEFT);
        setFighterAction(p, ACT_LEFT);
    } else if (justRight && notAttacking) {
        addPlayerRecord(ACT_RIGHT);
        setFighterAction(p, ACT_RIGHT);
    } else if (justJump && !isFighterJumping(p) && notAttacking) {
        addPlayerRecord(ACT_JUMP);
        setFighterAction(p, ACT_JUMP);
        setFighterJump(p);
        if (tiltLeft)  { addPlayerRecord(ACT_LEFT);  setFighterAction(p, ACT_LEFT);  }
        else if (tiltRight) { addPlayerRecord(ACT_RIGHT); setFighterAction(p, ACT_RIGHT); }
    }

    if (leftRel) {
        if (isFighterAction(p, ACT_LEFT)) {
            addPlayerRecord(ACT_IDLE);
            setFighterAction(p, ACT_IDLE);
        }
        setFighterActionCache(p, ACT_NONE);
    } else if (rightRel) {
        if (isFighterAction(p, ACT_RIGHT)) {
            addPlayerRecord(ACT_IDLE);
            setFighterAction(p, ACT_IDLE);
        }
        setFighterActionCache(p, ACT_NONE);
    }

    tickPlayerRecord(false);

    // ── CPU record playback ───────────────────────────────────────────────────
    if (_t1 == 0) {
        ++_t1;
        Actions act = (Actions)getCpuRecord().action;
        switch (act) {
            case ACT_NONE:  nextCpuRecord(); break;
            case ACT_LEFT:
            case ACT_RIGHT: setFighterAction(c, act); break;
            case ACT_IDLE:  setFighterAction(c, act); break;
            case ACT_PUNCH: setFighterAction(c, act); break;
            case ACT_SHOOT:
                if (isFighterShooting(c)) { _t1 = 0; }
                else { setFighterAction(c, act); fighterShoot(c); }
                break;
            case ACT_JUMP:
                setFighterAction(c, act);
                setFighterJump(c);
                break;
            default: break;
        }
    } else if (++_t1 >= (uint16_t)getCpuRecord().timeStamp) {
        _t1 = 0;
        nextCpuRecord();
    }

    // ── Movement ──────────────────────────────────────────────────────────────
    if (isFighterAction(p, ACT_LEFT)) {
        p.dir = 1;
        if (p.x > BORDER_LEFT) --p.x;
    } else if (isFighterAction(p, ACT_RIGHT)) {
        p.dir = 0;
        if (p.x < BORDER_RIGHT) ++p.x;
    }

    bool cpuMoving = isFighterAction(c, ACT_LEFT) || isFighterAction(c, ACT_RIGHT);
    if (cpuMoving) {
        if (isFighterAction(c, ACT_LEFT)) {
            c.dir = c.mirror ? 0 : 1;
        } else {
            c.dir = c.mirror ? 1 : 0;
        }
        if (c.dir == 1) {
            if (c.x <= BORDER_LEFT) { c.x = BORDER_LEFT; flipCpu(c); }
            else --c.x;
        } else {
            if (c.x >= BORDER_RIGHT) { c.x = BORDER_RIGHT; flipCpu(c); }
            else ++c.x;
        }
    }

    // ── Physics tick ──────────────────────────────────────────────────────────
    for (uint8_t i = 0; i < 2; ++i) {
        tickFighterShoot(_f[i]);
        tickFighterJump(_f[i]);
    }

    // ── Collision ─────────────────────────────────────────────────────────────
    detectCollision(p, c);
    detectCollision(c, p);

    // ── Check win/lose ────────────────────────────────────────────────────────
    if (c.hp == 0) {
        _state = STATE_WIN;
        _stateTimer = 0;
    } else if (p.hp == 0) {
        _state = STATE_LOSE;
        _stateTimer = 0;
    }

    // ── Animate frames ────────────────────────────────────────────────────────
    for (uint8_t i = 0; i < 2; ++i) {
        if (++_f[i].frameTicks >= AFPT) {
            _f[i].frameTicks = 0;
            ++_f[i].frame;
        }
        bool atkDone = drawFighter(_f[i], i == 0, d);
        if (atkDone) setFighterProperAction(_f[i], i == 0, gravX);
    }
    drawHUD(d);
}

// ─── State: WIN ───────────────────────────────────────────────────────────────
void PocketFighterGame::doWin(Adafruit_SSD1306& d) {
    if (++_stateTimer >= 20u) {  // ~0.67s at 30fps
        _state = STATE_NEXT;
        _stateTimer = 0;
        return;
    }
    for (uint8_t i = 0; i < 2; ++i) {
        if (++_f[i].frameTicks >= AFPT) { _f[i].frameTicks = 0; ++_f[i].frame; }
        drawFighter(_f[i], i == 0, d);
    }
    drawHUD(d);
}

// ─── State: LOSE ──────────────────────────────────────────────────────────────
void PocketFighterGame::doLose(Adafruit_SSD1306& d) {
    if (++_stateTimer >= 20u) {
        _state = STATE_GAMEOVER;
        _stateTimer = 0;
        return;
    }
    for (uint8_t i = 0; i < 2; ++i) {
        if (++_f[i].frameTicks >= AFPT) { _f[i].frameTicks = 0; ++_f[i].frame; }
        drawFighter(_f[i], i == 0, d);
    }
    drawHUD(d);
}

// ─── State: NEXT ──────────────────────────────────────────────────────────────
void PocketFighterGame::doNext(bool btnEdge, Adafruit_SSD1306& d) {
    if (btnEdge) {
        prepare();
        _state = STATE_FIGHT;
        _stateTimer = 0;
        return;
    }
    drawPageBitmap(d, 32, 16, SPR_NEXT, 64, 32);
    drawProgress(d);
    // Blink hint
    if ((_stateTimer++ / 8) % 2 == 0) {
        d.setTextSize(1);
        d.setTextColor(SSD1306_WHITE);
        d.setCursor(24, 52);
        d.print("BTN: CONTINUA");
    }
}

// ─── State: GAMEOVER ──────────────────────────────────────────────────────────
void PocketFighterGame::doOver(bool btnEdge, Adafruit_SSD1306& d) {
    if (btnEdge) {
        memset(_r, 0, sizeof(_r));
        _c0 = 0; _t0 = _t1 = 0;
        _f[0].hp = _f[1].hp = 3;
        _level = 0;
        _state = STATE_MENU;
        _stateTimer = 0;
        return;
    }
    drawPageBitmap(d, 32, 16, SPR_OVER, 64, 32);
    drawProgress(d);
    if ((_stateTimer++ / 8) % 2 == 0) {
        d.setTextSize(1);
        d.setTextColor(SSD1306_WHITE);
        d.setCursor(30, 52);
        d.print("BTN: RICOMINCIA");
    }
}

// ─── Public interface ─────────────────────────────────────────────────────────
void PocketFighterGame::begin() {
    _state = STATE_MENU;
    _level = 0;
    _best  = 1;
    memset(_r, 0, sizeof(_r));
    _f[0].hp = _f[1].hp = 3;
    _c0 = 0; _c1 = RECORD_HALF; _t0 = _t1 = 0;
    _stateTimer = 0;
    _prevLeft = _prevRight = _prevJump = _prevShake = false;

    EEPROM.begin(256);
    uint8_t saved = 1;
    EEPROM.get(EEPROM_ADDR, saved);
    if (saved >= 1 && saved < 200) _best = saved;
}

bool PocketFighterGame::update(float /*dt*/, float gravX, float gravY,
                                bool shake, bool btnPressed, Adafruit_SSD1306& d) {
    // ── Input edge detection ──────────────────────────────────────────────────
    const bool tiltLeft  = (gravX < -MOVE_THRESH);
    const bool tiltRight = (gravX >  MOVE_THRESH);
    const bool tiltJump  = (gravY < -JUMP_THRESH);

    const bool justLeft  = tiltLeft  && !_prevLeft;
    const bool justRight = tiltRight && !_prevRight;
    const bool justJump  = tiltJump  && !_prevJump;
    const bool leftRel   = !tiltLeft  && _prevLeft;
    const bool rightRel  = !tiltRight && _prevRight;
    const bool shakeEdge = shake && !_prevShake;

    _prevLeft  = tiltLeft;
    _prevRight = tiltRight;
    _prevJump  = tiltJump;
    _prevShake = shake;

    d.clearDisplay();
    switch (_state) {
        case STATE_MENU:
            doMenu(btnPressed, d);
            break;
        case STATE_FIGHT:
            doFight(gravX, gravY,
                    tiltLeft, tiltRight,
                    justLeft, justRight, justJump,
                    leftRel, rightRel,
                    shakeEdge, btnPressed, d);
            break;
        case STATE_WIN:
            doWin(d);
            break;
        case STATE_LOSE:
            doLose(d);
            break;
        case STATE_NEXT:
            doNext(btnPressed, d);
            break;
        case STATE_GAMEOVER:
            doOver(btnPressed, d);
            break;
    }
    d.display();
    return true;
}
