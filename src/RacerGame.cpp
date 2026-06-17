#include "RacerGame.h"
#include <Arduino.h>
#include <math.h>

// ─── Layout ───────────────────────────────────────────────────────────────────
constexpr int   R_HORIZON   = 14;
constexpr int   R_ROAD_BOT  = 56;
constexpr float R_MAX_HW    = 52.0f;

// ─── Velocità / sterzo ────────────────────────────────────────────────────────
constexpr float R_MAX_SPEED = 200.0f;
constexpr float R_MIN_SPEED =  35.0f;
constexpr float R_ACCEL     =  55.0f;
constexpr float R_DECEL     = 160.0f;
constexpr float R_STEER     =   0.006f;

// ─── Curve ────────────────────────────────────────────────────────────────────
constexpr float R_CURVE_AMP  = 35.0f;
constexpr float R_CURVE_FREQ =  0.13f;
constexpr float R_DRIFT      =   0.30f;
constexpr float R_CURVE_SPD  =   0.58f;
constexpr float R_OFFTRACK_F =   0.32f;

// roadBend: shift laterale del FONDO strada (crea la curva visibile nella parte bassa).
// Con la formula quadratica per rcx i bordi diventano parabole sullo schermo.
constexpr float R_BEND_PUSH  =   0.40f;
constexpr float R_BEND_BACK  =   0.45f;
constexpr float R_BEND_MAX   =  14.0f;

// ─── Ostacoli ─────────────────────────────────────────────────────────────────
constexpr float R_Z_SCALE   = 0.0045f;
constexpr float R_HIT_DIST  = 0.32f;
constexpr float R_HIT_Z     = 0.11f;

constexpr int   SUN_R       = 8;

// ─── Init ─────────────────────────────────────────────────────────────────────

void RacerGame::begin() {
    if (!_mutex) _mutex = xSemaphoreCreateMutex();
    _playerX    = 0.0f;
    _speed      = R_MIN_SPEED;
    _distance   = 0.0f;
    _curvePhase = 0.0f;
    _cameraX    = 0.0f;
    _roadBend   = 0.0f;
    _offTrack   = false;
    _score      = 0;
    _gstate     = PLAYING;
    _stateMs    = millis();

    // 2 ostacoli ben separati
    for (int i = 0; i < MAX_OBS; i++) {
        _obs[i].z      = 0.60f + i * 0.35f;   // 0.60, 0.95
        _obs[i].laneX  = (float)(random(0, 3) - 1) * 0.35f;
        _obs[i].active = true;
    }

    // 10 scenery: 7 palazzi (type=0) + 3 palme (type=1), spaziatura fitta
    static const uint8_t INIT_TYPE[] = { 0, 1, 0, 0, 1, 0, 0, 1, 0, 0 };
    static const int8_t  INIT_SIDE[] = { -1, 1, 1, -1, -1, 1, -1, 1, 1, -1 };
    for (int i = 0; i < MAX_SCENERY; i++) {
        _scenery[i].z    = 0.10f + i * 0.09f;  // 0.10..0.91
        _scenery[i].type = INIT_TYPE[i];
        _scenery[i].side = INIT_SIDE[i];
        _scenery[i].active = true;
    }
    copyToSnap();
}

void RacerGame::spawnObs(int i, float minZ) {
    _obs[i].z      = minZ + (float)random(20, 55) * 0.01f;
    _obs[i].laneX  = (float)(random(0, 3) - 1) * 0.35f;
    _obs[i].active = true;
}

void RacerGame::spawnScenery(int i, float minZ) {
    _scenery[i].z    = minZ + (float)random(0, 12) * 0.01f;
    // 66% palazzi, 33% palme
    _scenery[i].type = (random(0, 3) == 0) ? 1 : 0;
    _scenery[i].side = (random(0, 2) == 0) ? -1 : 1;
    _scenery[i].active = true;
}

// ─── Core 0: logica ───────────────────────────────────────────────────────────

void RacerGame::logicStep(float steerInput, float dt) {
    if (_gstate != PLAYING) return;

    _playerX += steerInput * R_STEER * dt;

    _curvePhase += R_CURVE_FREQ * dt * (1.0f + _distance * 0.00018f);
    const float curveTarget = sinf(_curvePhase) * R_CURVE_AMP;
    _cameraX += (curveTarget - _cameraX) * 3.5f * dt;

    // roadBend accumula lo shift reale del fondo strada.
    // Spring-back verso 0 nei rettilinei.
    _roadBend -= _cameraX * R_BEND_PUSH * dt;
    _roadBend -= _roadBend * R_BEND_BACK * dt;
    if (_roadBend < -R_BEND_MAX) _roadBend = -R_BEND_MAX;
    if (_roadBend >  R_BEND_MAX) _roadBend =  R_BEND_MAX;

    _playerX += (_cameraX / R_CURVE_AMP) * R_DRIFT * dt;
    if (_playerX < -1.6f) _playerX = -1.6f;
    if (_playerX >  1.6f) _playerX =  1.6f;
    _offTrack = (fabsf(_playerX) > 1.0f);

    const float curveSev    = fabsf(_cameraX) / R_CURVE_AMP;
    const float targetSpeed = _offTrack
        ? R_MAX_SPEED * R_OFFTRACK_F
        : R_MAX_SPEED * (R_CURVE_SPD + (1.0f - R_CURVE_SPD) * (1.0f - curveSev));
    if (_speed > targetSpeed) _speed -= R_DECEL * dt;
    else                      _speed += R_ACCEL * dt;
    if (_speed < R_MIN_SPEED) _speed = R_MIN_SPEED;
    if (_speed > R_MAX_SPEED) _speed = R_MAX_SPEED;

    _distance += _speed * dt;
    _score     = (int)(_distance * 0.1f);

    for (int i = 0; i < MAX_OBS; i++) {
        if (!_obs[i].active) continue;
        _obs[i].z -= R_Z_SCALE * _speed * dt;
        if (_obs[i].z > 0.0f && _obs[i].z < R_HIT_Z &&
                fabsf(_playerX - _obs[i].laneX * 2.0f) < R_HIT_DIST) {
            _gstate = RESULT; _stateMs = millis(); copyToSnap(); return;
        }
        if (_obs[i].z < -0.02f) spawnObs(i, 0.97f);
    }
    for (int i = 0; i < MAX_SCENERY; i++) {
        if (!_scenery[i].active) continue;
        _scenery[i].z -= R_Z_SCALE * _speed * dt;
        if (_scenery[i].z < -0.05f) spawnScenery(i, 0.88f);
    }
    copyToSnap();
}

// ─── Snapshot ─────────────────────────────────────────────────────────────────

void RacerGame::copyToSnap() {
    if (!_mutex || xSemaphoreTake(_mutex, 0) != pdTRUE) return;
    _snap.playerX  = _playerX;
    _snap.speed    = _speed;
    _snap.distance = _distance;
    _snap.cameraX  = _cameraX;
    _snap.roadBend = _roadBend;
    _snap.offTrack = _offTrack;
    _snap.score    = _score;
    _snap.gstate   = _gstate;
    _snap.stateMs  = _stateMs;
    for (int i = 0; i < MAX_OBS; i++) {
        _snap.obs[i].z      = _obs[i].z;
        _snap.obs[i].laneX  = _obs[i].laneX;
        _snap.obs[i].active = _obs[i].active;
    }
    for (int i = 0; i < MAX_SCENERY; i++) {
        _snap.scenery[i].z      = _scenery[i].z;
        _snap.scenery[i].type   = _scenery[i].type;
        _snap.scenery[i].side   = _scenery[i].side;
        _snap.scenery[i].active = _scenery[i].active;
    }
    xSemaphoreGive(_mutex);
}

// ─── Sky ──────────────────────────────────────────────────────────────────────

void RacerGame::drawSky(Adafruit_SSD1306& d, int vanishX) {
    for (int dy = 0; dy <= SUN_R; dy++) {
        const int y  = R_HORIZON - dy;
        if (y < 0) continue;
        const int dx = (int)sqrtf((float)(SUN_R * SUN_R - dy * dy));
        if ((dy >> 1) % 2 == 0)
            d.drawFastHLine(vanishX - dx, y, 2 * dx + 1, SSD1306_WHITE);
    }
    d.drawFastHLine(0, R_HORIZON, 128, SSD1306_WHITE);
}

// ─── Strada con formula QUADRATICA ────────────────────────────────────────────
//
// Vecchia formula:  rcx = base + cameraX*(1-t)     → bordi rettilinei
// Nuova formula:    rcx = base + cameraX*(1-t²)    → bordi PARABOLICI
//
// Con t²: a profondità intermedia il centro strada si sposta MOLTO più che
// linearmente → i bordi della carreggiata disegnano una curva sullo schermo,
// non una retta. Questo è il meccanismo visivo delle curve in OutRun.

void RacerGame::drawRoad(Adafruit_SSD1306& d, const Snap& s) {
    const int   DEPTH = R_ROAD_BOT - R_HORIZON;
    const float base  = 64.0f + s.roadBend;

    for (int y = R_HORIZON + 1; y <= R_ROAD_BOT; y++) {
        const float t   = (float)(y - R_HORIZON) / DEPTH;
        const float hw  = R_MAX_HW * t;
        // Formula quadratica: produce bordi parabolici (curve visibili)
        const int   rcx = (int)(base + s.cameraX * (1.0f - t * t));
        const int   lx  = rcx - (int)hw;
        const int   rx  = rcx + (int)hw;

        const float worldZ = 1.0f / (t + 0.005f);
        const int   seg    = (int)(s.distance * 0.28f + worldZ) & 1;

        for (int x = 0; x < lx && x < 128; x++)
            if ((x ^ y) & 1) d.drawPixel(x, y, SSD1306_WHITE);
        for (int x = (rx + 1 > 0 ? rx + 1 : 0); x < 128; x++)
            if ((x ^ y) & 1) d.drawPixel(x, y, SSD1306_WHITE);

        // Bordo 2px solido su ogni lato — contrasto netto con l'erba
        if (lx   >= 0 && lx   < 128) d.drawPixel(lx,   y, SSD1306_WHITE);
        if (lx+1 >= 0 && lx+1 < 128) d.drawPixel(lx+1, y, SSD1306_WHITE);
        if (rx   >= 0 && rx   < 128) d.drawPixel(rx,   y, SSD1306_WHITE);
        if (rx-1 >= 0 && rx-1 < 128) d.drawPixel(rx-1, y, SSD1306_WHITE);
        if (seg && rcx >= 0 && rcx < 128)
            d.drawPixel(rcx, y, SSD1306_WHITE);
    }
}

// ─── Scenery: palazzi imponenti e palme al bordo ──────────────────────────────

void RacerGame::drawSceneryObj(Adafruit_SSD1306& d, int ox, int oy, float t, uint8_t type) {
    if (t < 0.07f || oy > R_ROAD_BOT + 2) return;

    if (type == 0) {
        // Palazzo: molto alto, fuoriesce dallo schermo, finestre a griglia
        const int h = max(6,  (int)(95 * t));
        const int w = max(3,  (int)(26 * t));
        int top = oy - h;
        if (top < 0) top = 0;
        const int draw_h = oy - top;
        // Bordo nero 1px tutto intorno — stacca il palazzo dal checkerboard
        d.drawRect(ox - w / 2 - 1, top - 1, w + 2, draw_h + 2, SSD1306_BLACK);
        d.fillRect(ox - w / 2, top, w, draw_h, SSD1306_WHITE);
        // Finestre (pixel neri su 2 colonne verticali)
        if (t > 0.18f && draw_h > 8 && w > 4) {
            for (int wy = top + 2; wy < oy - 1; wy += 3) {
                if (ox - 2 >= 0 && ox - 2 < 128) d.drawPixel(ox - 2, wy, SSD1306_BLACK);
                if (ox + 1 >= 0 && ox + 1 < 128) d.drawPixel(ox + 1, wy, SSD1306_BLACK);
            }
        }
    } else {
        // Palma: tronco sottile + foglie a ventaglio
        const int trunk_h = max(2, (int)(22 * t));
        const int ty      = oy - trunk_h;
        // Strisce nere ai lati del tronco — lo staccano dal checkerboard
        if (ox >= 1 && ox < 127) d.drawFastVLine(ox - 1, ty, trunk_h + 1, SSD1306_BLACK);
        if (ox >= 0 && ox < 126) d.drawFastVLine(ox + 1, ty, trunk_h + 1, SSD1306_BLACK);
        d.drawFastVLine(ox, ty, trunk_h, SSD1306_WHITE);
        if (t > 0.09f) {
            const int lf = max(2, (int)(9 * t));
            d.drawLine(ox, ty, ox - lf,     ty - lf / 2, SSD1306_WHITE);
            d.drawLine(ox, ty, ox + lf,     ty - lf / 2, SSD1306_WHITE);
            d.drawLine(ox, ty, ox - lf / 2, ty - lf,     SSD1306_WHITE);
            d.drawLine(ox, ty, ox + lf / 2, ty - lf,     SSD1306_WHITE);
            d.drawLine(ox, ty, ox,           ty - lf,     SSD1306_WHITE);
        }
    }
}

// ─── Sprite macchina giocatore (20×9 px, Piskel custom) ──────────────────────

static const uint8_t PLAYER_CAR_BMP[] PROGMEM = {
    0x00, 0x00, 0x00,   // r0: ....................
    0x0F, 0xFF, 0x00,   // r1: ....############....
    0x08, 0x01, 0x00,   // r2: ....#..........#....
    0x33, 0xFC, 0xC0,   // r3: ..##..########..##..
    0xC4, 0x62, 0x30,   // r4: ##...#...##...#...##
    0xBE, 0x07, 0xD0,   // r5: #.#####......#####.#
    0x8F, 0xFF, 0x10,   // r6: #...############...#
    0xF7, 0x9E, 0xF0,   // r7: ####.####..####.####
    0xF1, 0xF8, 0xF0,   // r8: ####...######...####
};

void RacerGame::drawPlayerCar(Adafruit_SSD1306& d, int16_t cx, int16_t bot) {
    d.drawBitmap(cx - 10, bot - 8, PLAYER_CAR_BMP, 20, 9, SSD1306_WHITE);
}

// ─── Sprite macchina avversaria (14×9 px, Piskel custom) ─────────────────────

static const uint8_t OBS_CAR_BMP[] PROGMEM = {
    0x3F, 0xF0,   // r0: ..##########..
    0x20, 0x10,   // r1: ..#........#..
    0x40, 0x08,   // r2: .#..........#.
    0xA0, 0x14,   // r3: #.#........#.#
    0xFF, 0xFC,   // r4: ##############
    0xFF, 0xFC,   // r5: ##############
    0xFF, 0xFC,   // r6: ##############
    0xCF, 0xCC,   // r7: ##..######..##
    0xC0, 0x0C,   // r8: ##..........##
};

// Nearest-neighbor scaled bitmap — Adafruit GFX non supporta scaling nativo
static void drawScaledBmp(Adafruit_SSD1306& d,
                          int16_t x, int16_t y,
                          const uint8_t* bmp,
                          int sw, int sh, int dw, int dh) {
    for (int dy = 0; dy < dh; dy++) {
        const int sy = dy * sh / dh;
        for (int dx = 0; dx < dw; dx++) {
            const int sx  = dx * sw / dw;
            const int bit = sy * ((sw + 7) / 8) * 8 + sx;
            if (pgm_read_byte(bmp + bit / 8) & (0x80 >> (bit % 8)))
                d.drawPixel(x + dx, y + dy, SSD1306_WHITE);
        }
    }
}

void RacerGame::drawObsCar(Adafruit_SSD1306& d, int16_t cx, int16_t bot, float scale) {
    if (scale < 0.08f) return;
    const int bw = max(2, (int)(14 * scale));
    const int bh = max(1, (int)( 9 * scale));
    drawScaledBmp(d, cx - bw / 2, bot - bh, OBS_CAR_BMP, 14, 9, bw, bh);
}

// ─── Core 1: render frame ─────────────────────────────────────────────────────

bool RacerGame::renderFrame(Adafruit_SSD1306& d, bool btnPressed, bool shake) {
    Snap s;
    xSemaphoreTake(_mutex, portMAX_DELAY);
    s = _snap;
    xSemaphoreGive(_mutex);

    if (s.gstate == RESULT) {
        if (btnPressed || shake || millis() - s.stateMs > 4000UL) return false;
        d.clearDisplay();
        d.setTextSize(2); d.setTextColor(SSD1306_WHITE);
        d.setCursor(10, 6);  d.print("GAME OVER");
        d.setTextSize(1);
        d.setCursor(14, 28); d.print("PUNTEGGIO: "); d.print(s.score);
        d.setCursor(8,  44); d.print("BTN/SCUOTI: rivinci");
        d.display();
        return true;
    }

    d.clearDisplay();

    // Sole: segue parzialmente il punto di fuga reale
    const int vanishX = (int)(64.0f + (s.roadBend + s.cameraX) * 0.45f);
    drawSky (d, vanishX);
    drawRoad(d, s);

    // Per ostacoli e scenery usiamo la stessa formula quadratica di drawRoad.
    // Con z = 1-t:  cameraX*(1-t²) = cameraX*(1-(1-z)²) = cameraX*z*(2-z)
    const float base = 64.0f + s.roadBend;

    // ── Scenario back-to-front ────────────────────────────────────────────────
    int sord[MAX_SCENERY];
    for (int i = 0; i < MAX_SCENERY; i++) sord[i] = i;
    for (int i = 1; i < MAX_SCENERY; i++)
        for (int j = i; j > 0 && s.scenery[sord[j-1]].z < s.scenery[sord[j]].z; j--)
            { int tmp = sord[j]; sord[j] = sord[j-1]; sord[j-1] = tmp; }

    for (int ii = 0; ii < MAX_SCENERY; ii++) {
        const int i = sord[ii];
        if (!s.scenery[i].active) continue;
        const float z  = s.scenery[i].z;
        const float t  = 1.0f - z;
        if (t < 0.07f) continue;
        const float hw  = R_MAX_HW * t;
        const float rcx = base + s.cameraX * z * (2.0f - z);  // formula quad.
        // Palme esattamente sul bordo strada, palazzi ben arretrati
        const int off = (s.scenery[i].type == 1)
            ? (int)hw
            : (int)hw + max(12, (int)(22 * t));
        const int ox  = (int)rcx + s.scenery[i].side * off;
        const int oy  = R_HORIZON + (int)(t * (R_ROAD_BOT - R_HORIZON));
        drawSceneryObj(d, ox, oy, t, s.scenery[i].type);
    }

    // ── Ostacoli back-to-front ────────────────────────────────────────────────
    int ord[MAX_OBS];
    for (int i = 0; i < MAX_OBS; i++) ord[i] = i;
    for (int i = 1; i < MAX_OBS; i++)
        for (int j = i; j > 0 && s.obs[ord[j-1]].z < s.obs[ord[j]].z; j--)
            { int tmp = ord[j]; ord[j] = ord[j-1]; ord[j-1] = tmp; }

    for (int ii = 0; ii < MAX_OBS; ii++) {
        const int i = ord[ii];
        if (!s.obs[i].active) continue;
        const float z   = s.obs[i].z;
        const float t   = 1.0f - z;
        if (t < 0.08f || t > 1.0f) continue;
        const float hw  = R_MAX_HW * t;
        const float rcx = base + s.cameraX * z * (2.0f - z);
        const int   sx  = (int)(rcx + s.obs[i].laneX * hw * 2.0f);
        const int   sy  = R_HORIZON + (int)(t * (R_ROAD_BOT - R_HORIZON));
        drawObsCar(d, (int16_t)sx, (int16_t)sy, t);
    }

    // ── Macchina giocatore ────────────────────────────────────────────────────
    const int playerSX = (int)base + (int)(s.playerX * R_MAX_HW);
    drawPlayerCar(d, (int16_t)playerSX, R_ROAD_BOT);

    // ── HUD ──────────────────────────────────────────────────────────────────
    d.drawFastHLine(0, 57, 128, SSD1306_WHITE);
    d.setTextSize(1); d.setTextColor(SSD1306_WHITE);
    d.setCursor(1, 59); d.print("V");
    const int barW = (int)(44.0f * s.speed / R_MAX_SPEED);
    d.drawRect(8, 59, 44, 4, SSD1306_WHITE);
    if (barW > 0) d.fillRect(8, 59, barW, 4, SSD1306_WHITE);
    d.setCursor(58, 59); d.print("SC:"); d.print(s.score);
    if (s.offTrack) { d.setCursor(100, 59); d.print("OUT"); }

    d.display();
    return true;
}
