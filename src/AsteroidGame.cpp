#include "AsteroidGame.h"
#include <Arduino.h>
#include <math.h>

// ─── Constants ────────────────────────────────────────────────────────────────
constexpr int   AS_HUD_H            = 8;
constexpr float AS_BULLET_SPD       = 200.f;
constexpr float AS_FIRE_CDN         = 0.28f;
constexpr float AS_FIRE_CDN_RAPID   = 0.11f;
constexpr int   AS_SHIP_HIT_R       = 5;
constexpr float AS_ENEMY_SPD        = 38.f;
constexpr float AS_ENEMY_TRACK      = 22.f;   // px/s vertical tracking
constexpr float AS_ENEMY_FIRE_CDN   = 1.8f;
constexpr float AS_ENEMY_BULLET_SPD = 110.f;
constexpr float AS_PU_SPD           = 26.f;
constexpr int   AS_ENEMY_SCORE      = 3;

static constexpr int AS_LEVELS          = 5;
static constexpr int AS_ROCKS_PER_LEVEL = 6;

struct LevelCfg { int maxRocks; float baseSpd; float spawnSec; };
static const LevelCfg LCFG[AS_LEVELS] = {
    { 3, 32.f, 2.8f },
    { 3, 42.f, 2.3f },
    { 4, 53.f, 1.8f },
    { 4, 64.f, 1.4f },
    { 5, 76.f, 1.1f },
};

// ─── Begin ────────────────────────────────────────────────────────────────────

void AsteroidGame::begin() {
    if (!_mutex) _mutex = xSemaphoreCreateMutex();
    _shipX           = 18.f;
    _shipY           = 36.f;
    _score           = 0;
    _lives           = 3;
    _level           = 0;
    _rocksDestroyed  = 0;
    _spawnTimer      = 0.f;
    _fireTimer       = 0.f;
    _gstate          = PLAYING;
    _stateMs         = 0;
    _spawnInterval   = LCFG[0].spawnSec;
    _puSpawnTimer    = 0.f;
    _puInterval      = 10.f;
    _enemySpawnTimer = 0.f;
    _enemyInterval   = 14.f;
    _shieldTimer     = 0.f;
    _rapidFireTimer  = 0.f;
    _tripleShotTimer = 0.f;

    for (int i = 0; i < MAX_ROCKS;         i++) _rocks[i].active   = false;
    for (int i = 0; i < MAX_BULLETS;       i++) _bullets[i].active = false;
    for (int i = 0; i < MAX_POWERUPS;      i++) _powerups[i].active  = false;
    for (int i = 0; i < MAX_ENEMIES;       i++) _enemies[i].active   = false;
    for (int i = 0; i < MAX_ENEMY_BULLETS; i++) _eBullets[i].active  = false;

    for (int i = 0; i < NUM_STARS; i++) {
        _starX[i]  = (float)random(2, 126);
        _starY[i]  = (float)(AS_HUD_H + 2 + random(0, 54));
        _starVX[i] = 12.f + (float)random(0, 38);
    }

    copyToSnap();
}

void AsteroidGame::setLevel(int lv) {
    if (lv >= AS_LEVELS) lv = AS_LEVELS - 1;
    _level         = lv;
    _spawnInterval = LCFG[lv].spawnSec;
}

// ─── Spawn rock ───────────────────────────────────────────────────────────────

void AsteroidGame::spawnRock() {
    const LevelCfg& c = LCFG[_level];
    for (int i = 0; i < MAX_ROCKS; i++) {
        if (_rocks[i].active) continue;
        const int8_t r   = (int8_t)(3 + random(0, 3));
        const float  spd = c.baseSpd + (float)random(0, 16);
        const int    yMax = 62 - r - AS_HUD_H;
        _rocks[i].x      = 131.f;
        _rocks[i].y      = (float)(AS_HUD_H + r + (yMax > 0 ? random(0, yMax) : 0));
        _rocks[i].vx     = -spd;
        _rocks[i].vy     = (float)(random(-8, 9)) * 0.65f;
        _rocks[i].r      = r;
        _rocks[i].active = true;
        return;
    }
}

// ─── Spawn power-up ───────────────────────────────────────────────────────────

void AsteroidGame::spawnPowerUp() {
    for (int i = 0; i < MAX_POWERUPS; i++) {
        if (_powerups[i].active) continue;
        _powerups[i].x      = 131.f;
        _powerups[i].y      = (float)(AS_HUD_H + 5 + random(0, 48));
        _powerups[i].type   = (PowerUpType)random(0, 4);
        _powerups[i].active = true;
        return;
    }
}

// ─── Spawn enemy ship ─────────────────────────────────────────────────────────

void AsteroidGame::spawnEnemy() {
    for (int i = 0; i < MAX_ENEMIES; i++) {
        if (_enemies[i].active) continue;
        _enemies[i].x         = 131.f;
        _enemies[i].y         = (float)(AS_HUD_H + 6 + random(0, 48));
        _enemies[i].vy        = 0.f;
        _enemies[i].active    = true;
        _enemies[i].fireTimer = 1.0f + (float)random(0, 100) * 0.01f;
        return;
    }
}

// ─── Fire player bullet ───────────────────────────────────────────────────────

void AsteroidGame::fireBullet() {
    if (_tripleShotTimer > 0.f) {
        const float offsets[3] = { 0.f, -3.f, 3.f };
        int fired = 0;
        for (int i = 0; i < MAX_BULLETS && fired < 3; i++) {
            if (_bullets[i].active) continue;
            _bullets[i].x      = _shipX + 11.f;
            _bullets[i].y      = _shipY + offsets[fired];
            _bullets[i].active = true;
            fired++;
        }
        return;
    }
    for (int i = 0; i < MAX_BULLETS; i++) {
        if (_bullets[i].active) continue;
        _bullets[i].x      = _shipX + 11.f;
        _bullets[i].y      = _shipY;
        _bullets[i].active = true;
        return;
    }
}

// ─── Fire enemy bullet (leftward) ────────────────────────────────────────────

void AsteroidGame::fireEnemyBullet(int ei) {
    for (int i = 0; i < MAX_ENEMY_BULLETS; i++) {
        if (_eBullets[i].active) continue;
        _eBullets[i].x      = _enemies[ei].x - 6.f;
        _eBullets[i].y      = _enemies[ei].y;
        _eBullets[i].active = true;
        return;
    }
}

// ─── Core 0: logic step ───────────────────────────────────────────────────────

void AsteroidGame::logicStep(float shipX, float shipY, bool fire, float dt) {
    if (_gstate != PLAYING) return;

    _shipX = shipX;
    _shipY = shipY;

    // Power-up timer countdown
    if (_shieldTimer     > 0.f) _shieldTimer     -= dt;
    if (_rapidFireTimer  > 0.f) _rapidFireTimer  -= dt;
    if (_tripleShotTimer > 0.f) _tripleShotTimer -= dt;

    // Fire
    const float fireCDN = (_rapidFireTimer > 0.f) ? AS_FIRE_CDN_RAPID : AS_FIRE_CDN;
    _fireTimer -= dt;
    if (fire && _fireTimer <= 0.f) {
        fireBullet();
        _fireTimer = fireCDN;
    }

    // Move player bullets
    for (int i = 0; i < MAX_BULLETS; i++) {
        if (!_bullets[i].active) continue;
        _bullets[i].x += AS_BULLET_SPD * dt;
        if (_bullets[i].x > 133.f) _bullets[i].active = false;
    }

    // Spawn + move rocks
    _spawnTimer += dt;
    if (_spawnTimer >= _spawnInterval) {
        _spawnTimer = 0.f;
        spawnRock();
    }
    for (int i = 0; i < MAX_ROCKS; i++) {
        if (!_rocks[i].active) continue;
        _rocks[i].x += _rocks[i].vx * dt;
        _rocks[i].y += _rocks[i].vy * dt;
        const float minY = AS_HUD_H + _rocks[i].r;
        const float maxY = 63.f - _rocks[i].r;
        if (_rocks[i].y < minY) { _rocks[i].y = minY; _rocks[i].vy =  fabsf(_rocks[i].vy); }
        if (_rocks[i].y > maxY) { _rocks[i].y = maxY; _rocks[i].vy = -fabsf(_rocks[i].vy); }
        if (_rocks[i].x < -12.f) _rocks[i].active = false;
    }

    // Scroll star parallax
    for (int i = 0; i < NUM_STARS; i++) {
        _starX[i] -= _starVX[i] * dt;
        if (_starX[i] < 0.f) {
            _starX[i] = 128.f + (float)random(0, 16);
            _starY[i] = (float)(AS_HUD_H + 2 + random(0, 54));
        }
    }

    // Spawn + move power-ups
    for (int i = 0; i < MAX_POWERUPS; i++) {
        if (!_powerups[i].active) continue;
        _powerups[i].x -= AS_PU_SPD * dt;
        if (_powerups[i].x < -6.f) _powerups[i].active = false;
    }
    _puSpawnTimer += dt;
    if (_puSpawnTimer >= _puInterval) {
        _puSpawnTimer = 0.f;
        _puInterval   = 10.f + (float)random(0, 8);
        spawnPowerUp();
    }

    // Spawn + move enemies (track player Y, fire periodically)
    for (int i = 0; i < MAX_ENEMIES; i++) {
        if (!_enemies[i].active) continue;
        _enemies[i].x -= AS_ENEMY_SPD * dt;
        const float dy = _shipY - _enemies[i].y;
        _enemies[i].y += (dy > 0 ? 1.f : -1.f) * AS_ENEMY_TRACK * dt;
        if (_enemies[i].y < (float)(AS_HUD_H + 4)) _enemies[i].y = (float)(AS_HUD_H + 4);
        if (_enemies[i].y > 60.f)                  _enemies[i].y = 60.f;
        _enemies[i].fireTimer -= dt;
        if (_enemies[i].fireTimer <= 0.f) {
            fireEnemyBullet(i);
            _enemies[i].fireTimer = AS_ENEMY_FIRE_CDN;
        }
        if (_enemies[i].x < -12.f) _enemies[i].active = false;
    }
    _enemySpawnTimer += dt;
    if (_enemySpawnTimer >= _enemyInterval) {
        _enemySpawnTimer = 0.f;
        _enemyInterval   = 12.f + (float)random(0, 10);
        spawnEnemy();
    }

    // Move enemy bullets
    for (int i = 0; i < MAX_ENEMY_BULLETS; i++) {
        if (!_eBullets[i].active) continue;
        _eBullets[i].x -= AS_ENEMY_BULLET_SPD * dt;
        if (_eBullets[i].x < -5.f) _eBullets[i].active = false;
    }

    checkCollisions();

    if (_rocksDestroyed >= (_level + 1) * AS_ROCKS_PER_LEVEL)
        setLevel(_level + 1);

    if (_lives <= 0) { _gstate = GAME_OVER; _stateMs = millis(); }

    copyToSnap();
}

// ─── Collisions ───────────────────────────────────────────────────────────────

void AsteroidGame::checkCollisions() {
    // Player bullets vs rocks
    for (int bi = 0; bi < MAX_BULLETS; bi++) {
        if (!_bullets[bi].active) continue;
        for (int ri = 0; ri < MAX_ROCKS; ri++) {
            if (!_rocks[ri].active) continue;
            const float dx  = _bullets[bi].x - _rocks[ri].x;
            const float dy  = _bullets[bi].y - _rocks[ri].y;
            const float thr = _rocks[ri].r + 2.f;
            if (dx*dx + dy*dy <= thr*thr) {
                _bullets[bi].active = false;
                _rocks[ri].active   = false;
                _score++;
                _rocksDestroyed++;
            }
        }
    }

    // Player bullets vs enemy ships
    for (int bi = 0; bi < MAX_BULLETS; bi++) {
        if (!_bullets[bi].active) continue;
        for (int ei = 0; ei < MAX_ENEMIES; ei++) {
            if (!_enemies[ei].active) continue;
            const float dx = _bullets[bi].x - _enemies[ei].x;
            const float dy = _bullets[bi].y - _enemies[ei].y;
            if (dx*dx + dy*dy <= 8.f*8.f) {
                _bullets[bi].active = false;
                _enemies[ei].active = false;
                _score += AS_ENEMY_SCORE;
            }
        }
    }

    // Rocks vs ship (shield blocks damage)
    for (int ri = 0; ri < MAX_ROCKS; ri++) {
        if (!_rocks[ri].active) continue;
        const float dx  = _rocks[ri].x - _shipX;
        const float dy  = _rocks[ri].y - _shipY;
        const float thr = _rocks[ri].r + AS_SHIP_HIT_R;
        if (dx*dx + dy*dy <= thr*thr) {
            _rocks[ri].active = false;
            if (_shieldTimer <= 0.f) _lives--;
        }
    }

    // Enemy ships vs player (ram — shield blocks)
    for (int ei = 0; ei < MAX_ENEMIES; ei++) {
        if (!_enemies[ei].active) continue;
        const float dx  = _enemies[ei].x - _shipX;
        const float dy  = _enemies[ei].y - _shipY;
        const float thr = 6.f + AS_SHIP_HIT_R;
        if (dx*dx + dy*dy <= thr*thr) {
            _enemies[ei].active = false;
            if (_shieldTimer <= 0.f) _lives--;
        }
    }

    // Enemy bullets vs ship (shield blocks)
    if (_shieldTimer <= 0.f) {
        for (int i = 0; i < MAX_ENEMY_BULLETS; i++) {
            if (!_eBullets[i].active) continue;
            const float dx = _eBullets[i].x - _shipX;
            const float dy = _eBullets[i].y - _shipY;
            if (dx*dx + dy*dy <= (float)(AS_SHIP_HIT_R * AS_SHIP_HIT_R)) {
                _eBullets[i].active = false;
                _lives--;
            }
        }
    }

    // Ship collects power-ups
    const float puThr = (float)((AS_SHIP_HIT_R + 5) * (AS_SHIP_HIT_R + 5));
    for (int i = 0; i < MAX_POWERUPS; i++) {
        if (!_powerups[i].active) continue;
        const float dx = _powerups[i].x - _shipX;
        const float dy = _powerups[i].y - _shipY;
        if (dx*dx + dy*dy <= puThr) {
            _powerups[i].active = false;
            switch (_powerups[i].type) {
                case PU_SHIELD:     _shieldTimer     = 4.f;                break;
                case PU_RAPIDFIRE:  _rapidFireTimer  = 5.f;                break;
                case PU_TRIPLESHOT: _tripleShotTimer = 5.f;                break;
                case PU_EXTRALIFE:  if (_lives < 5) _lives++;              break;
            }
        }
    }
}

// ─── Snapshot ─────────────────────────────────────────────────────────────────

void AsteroidGame::copyToSnap() {
    if (!_mutex || xSemaphoreTake(_mutex, 0) != pdTRUE) return;
    _snap.shipX           = _shipX;
    _snap.shipY           = _shipY;
    _snap.score           = _score;
    _snap.lives           = _lives;
    _snap.level           = _level;
    _snap.gstate          = _gstate;
    _snap.stateMs         = _stateMs;
    _snap.shieldTimer     = _shieldTimer;
    _snap.rapidFireTimer  = _rapidFireTimer;
    _snap.tripleShotTimer = _tripleShotTimer;
    for (int i = 0; i < MAX_ROCKS;         i++) _snap.rocks[i]    = _rocks[i];
    for (int i = 0; i < MAX_BULLETS;       i++) _snap.bullets[i]  = _bullets[i];
    for (int i = 0; i < NUM_STARS;         i++) {
        _snap.starX[i] = _starX[i];
        _snap.starY[i] = _starY[i];
    }
    for (int i = 0; i < MAX_POWERUPS;      i++) _snap.powerups[i] = _powerups[i];
    for (int i = 0; i < MAX_ENEMIES;       i++) _snap.enemies[i]  = _enemies[i];
    for (int i = 0; i < MAX_ENEMY_BULLETS; i++) _snap.eBullets[i] = _eBullets[i];
    xSemaphoreGive(_mutex);
}

// ─── Sprites ──────────────────────────────────────────────────────────────────
//
// Player ship pointing RIGHT (unchanged from original)

void AsteroidGame::drawShip(Adafruit_SSD1306& d, int16_t cx, int16_t cy) {
    d.fillRect(cx - 6, cy - 1, 13, 3, SSD1306_WHITE);
    d.fillRect(cx + 7, cy - 1, 2,  3, SSD1306_WHITE);
    d.drawFastHLine(cx + 9, cy, 2, SSD1306_WHITE);
    d.fillRect(cx + 1, cy - 2, 4, 1, SSD1306_WHITE);
    d.drawLine(cx - 2, cy - 1, cx - 5, cy - 4, SSD1306_WHITE);
    d.drawFastHLine(cx - 7, cy - 4, 3, SSD1306_WHITE);
    d.drawLine(cx - 2, cy + 1, cx - 5, cy + 4, SSD1306_WHITE);
    d.drawFastHLine(cx - 7, cy + 4, 3, SSD1306_WHITE);
    d.drawPixel(cx - 6, cy - 2, SSD1306_WHITE);
    d.drawPixel(cx - 6, cy + 2, SSD1306_WHITE);
}

void AsteroidGame::drawRock(Adafruit_SSD1306& d, int16_t cx, int16_t cy, int8_t r) {
    d.fillCircle(cx, cy, r, SSD1306_WHITE);
    if (r >= 3) {
        d.drawPixel(cx + r,     cy - r + 2, SSD1306_BLACK);
        d.drawPixel(cx - r + 1, cy + r - 1, SSD1306_BLACK);
        d.drawPixel(cx + r - 1, cy + r,     SSD1306_BLACK);
    }
    if (r >= 4) {
        d.drawPixel(cx - r, cy - r + 2, SSD1306_BLACK);
        d.drawPixel(cx - 1, cy - 1,     SSD1306_BLACK);
    }
}

// Enemy ship pointing LEFT (mirror of player)
void AsteroidGame::drawEnemy(Adafruit_SSD1306& d, int16_t cx, int16_t cy) {
    d.fillRect(cx - 4, cy - 1, 9,  3, SSD1306_WHITE);
    d.fillRect(cx - 7, cy - 1, 2,  3, SSD1306_WHITE);
    d.drawFastHLine(cx - 9, cy, 2, SSD1306_WHITE);
    d.fillRect(cx - 4, cy - 2, 4,  1, SSD1306_WHITE);
    d.drawLine(cx + 2, cy - 1, cx + 5, cy - 4, SSD1306_WHITE);
    d.drawFastHLine(cx + 5, cy - 4, 3, SSD1306_WHITE);
    d.drawLine(cx + 2, cy + 1, cx + 5, cy + 4, SSD1306_WHITE);
    d.drawFastHLine(cx + 5, cy + 4, 3, SSD1306_WHITE);
    d.drawPixel(cx + 5, cy - 2, SSD1306_WHITE);
    d.drawPixel(cx + 5, cy + 2, SSD1306_WHITE);
}

// Power-up: 9x9 box with type-specific symbol inside
void AsteroidGame::drawPowerUp(Adafruit_SSD1306& d, int16_t cx, int16_t cy, PowerUpType t) {
    d.drawRect(cx - 4, cy - 4, 9, 9, SSD1306_WHITE);
    switch (t) {
        case PU_SHIELD:      // circle = bubble shield
            d.drawCircle(cx, cy, 2, SSD1306_WHITE);
            break;
        case PU_RAPIDFIRE:   // arrow head = rapid fire
            d.drawFastHLine(cx - 2, cy, 5, SSD1306_WHITE);
            d.drawPixel(cx + 2, cy - 1, SSD1306_WHITE);
            d.drawPixel(cx + 2, cy + 1, SSD1306_WHITE);
            break;
        case PU_TRIPLESHOT:  // three lines = triple shot
            d.drawFastHLine(cx - 2, cy - 2, 5, SSD1306_WHITE);
            d.drawFastHLine(cx - 2, cy,     5, SSD1306_WHITE);
            d.drawFastHLine(cx - 2, cy + 2, 5, SSD1306_WHITE);
            break;
        case PU_EXTRALIFE:   // plus sign = extra life
            d.drawFastVLine(cx,     cy - 2, 5, SSD1306_WHITE);
            d.drawFastHLine(cx - 2, cy,     5, SSD1306_WHITE);
            break;
    }
}

// ─── Core 1: render frame ─────────────────────────────────────────────────────

bool AsteroidGame::renderFrame(Adafruit_SSD1306& d, bool btnPressed, bool shake) {
    Snap s;
    xSemaphoreTake(_mutex, portMAX_DELAY);
    s = _snap;
    xSemaphoreGive(_mutex);

    if (s.gstate == GAME_OVER) {
        if (btnPressed || shake || millis() - s.stateMs > 4000UL) return false;
        d.clearDisplay();
        d.setTextSize(2); d.setTextColor(SSD1306_WHITE);
        d.setCursor(14, 6);  d.print("GAME OVER");
        d.setTextSize(1);
        d.setCursor(10, 28); d.print("PUNTEGGIO: "); d.print(s.score);
        d.setCursor(10, 38); d.print("LIVELLO:   "); d.print(s.level + 1);
        d.setCursor(4,  52); d.print("BTN/SCUOTI: rivinci");
        d.display();
        return true;
    }

    d.clearDisplay();

    // Stars
    for (int i = 0; i < NUM_STARS; i++)
        d.drawPixel((int16_t)s.starX[i], (int16_t)s.starY[i], SSD1306_WHITE);

    // Power-ups
    for (int i = 0; i < MAX_POWERUPS; i++)
        if (s.powerups[i].active)
            drawPowerUp(d, (int16_t)s.powerups[i].x, (int16_t)s.powerups[i].y,
                        s.powerups[i].type);

    // Rocks
    for (int i = 0; i < MAX_ROCKS; i++)
        if (s.rocks[i].active)
            drawRock(d, (int16_t)s.rocks[i].x, (int16_t)s.rocks[i].y, s.rocks[i].r);

    // Enemy ships
    for (int i = 0; i < MAX_ENEMIES; i++)
        if (s.enemies[i].active)
            drawEnemy(d, (int16_t)s.enemies[i].x, (int16_t)s.enemies[i].y);

    // Player bullets
    for (int i = 0; i < MAX_BULLETS; i++)
        if (s.bullets[i].active)
            d.drawFastHLine((int16_t)s.bullets[i].x, (int16_t)s.bullets[i].y,
                            5, SSD1306_WHITE);

    // Enemy bullets (rightward dash going left)
    for (int i = 0; i < MAX_ENEMY_BULLETS; i++)
        if (s.eBullets[i].active)
            d.drawFastHLine((int16_t)s.eBullets[i].x, (int16_t)s.eBullets[i].y,
                            4, SSD1306_WHITE);

    // Shield ring around ship
    if (s.shieldTimer > 0.f)
        d.drawCircle((int16_t)s.shipX, (int16_t)s.shipY, AS_SHIP_HIT_R + 3, SSD1306_WHITE);

    // Ship
    drawShip(d, (int16_t)s.shipX, (int16_t)s.shipY);

    // HUD
    d.drawFastHLine(0, AS_HUD_H, 128, SSD1306_WHITE);
    d.setTextSize(1); d.setTextColor(SSD1306_WHITE);
    d.setCursor(0,  0); d.print("SC:"); d.print(s.score);
    d.setCursor(46, 0); d.print("LV:"); d.print(s.level + 1);
    // Active power-up indicators (S = shield, R = rapid, T = triple)
    int px = 76;
    if (s.shieldTimer     > 0.f) { d.setCursor(px, 0); d.print("S"); px += 6; }
    if (s.rapidFireTimer  > 0.f) { d.setCursor(px, 0); d.print("R"); px += 6; }
    if (s.tripleShotTimer > 0.f) { d.setCursor(px, 0); d.print("T"); px += 6; }
    // Lives (small squares, right-aligned)
    for (int i = 0; i < s.lives; i++)
        d.fillRect(122 - i * 6, 1, 5, 5, SSD1306_WHITE);

    d.display();
    return true;
}
