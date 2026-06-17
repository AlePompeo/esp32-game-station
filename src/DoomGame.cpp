#include "DoomGame.h"
#include "DoomSprites.h"
#include "DoomLevel.h"
#include <string.h>
#include <math.h>
#include <stdlib.h>

// ── Local helpers ─────────────────────────────────────────────────────────────
template<class T> static inline void _sw(T& a, T& b) { T t=a; a=b; b=t; }
static inline double _sgn(double a, double b) { return a>b?1.0:(b>a?-1.0:0.0); }

// ── Public ────────────────────────────────────────────────────────────────────

void DoomGame::begin() {
    _scene     = SC_INTRO;
    _invertScr = false;
    _flashScr  = 0;
    _numEnts   = 0;
    _numSents  = 0;
    _gunFired  = false;
    _walkTog   = false;
    _gunPos    = 0;
    _viewH     = 0;
    _jogging   = 0;
    _fade      = GRAD_N - 1;
    _hudReady  = false;
    _delta     = 1.0;
    memset(_zbuf, 0xFF, ZBUF_SIZE);
}

bool DoomGame::update(float dt, float gravX, float gravY,
                      bool btnHeld, bool btnPressed, Adafruit_SSD1306& d) {
    _buf   = d.getBuffer();
    _d     = &d;
    _delta = (double)dt / 0.066666;

    if (_scene == SC_INTRO)
        loopIntro(btnPressed, d);
    else
        loopPlay(dt, gravX, gravY, btnHeld, btnPressed, d);

    if (_flashScr > 0) {
        _invertScr = !_invertScr;
        _flashScr--;
    } else if (_invertScr) {
        _invertScr = false;
    }
    d.invertDisplay(_invertScr);
    d.display();
    return false;
}

// ── Utility ───────────────────────────────────────────────────────────────────

uint8_t DoomGame::coordsDist(const Coords* a, const Coords* b) {
    double dx = a->x - b->x;
    double dy = a->y - b->y;
    return (uint8_t)(sqrt(dx*dx + dy*dy) * DIST_MULT);
}

uint8_t DoomGame::getBlock(const uint8_t* lvl, uint8_t x, uint8_t y) {
    if (x >= LEVEL_W || y >= LEVEL_H) return E_FLOOR;
    return pgm_read_byte(lvl + ((LEVEL_H - 1 - y) * LEVEL_W + x) / 2)
           >> (!(x % 2) * 4) & 0x0F;
}

void DoomGame::initLevel(const uint8_t* lvl) {
    _numEnts = 0;
    _numSents = 0;
    for (int y = LEVEL_H - 1; y >= 0; y--) {
        for (int x = 0; x < LEVEL_W; x++) {
            if (getBlock(lvl, x, y) == E_PLAYER) {
                _player.pos   = {x + 0.5, y + 0.5};
                _player.dir   = {1.0, 0.0};
                _player.plane = {0.0, -0.66};
                _player.velocity = 0;
                _player.health   = 100;
                _player.keys     = 0;
                return;
            }
        }
    }
}

// ── Entity management ─────────────────────────────────────────────────────────

DoomGame::Entity DoomGame::makeEntity(uint8_t type, uint8_t x, uint8_t y,
                                      uint8_t st, uint8_t hp) {
    Entity e;
    e.uid      = makeUID(type, x, y);
    e.pos      = {x + 0.5, y + 0.5};
    e.state    = st;
    e.health   = hp;
    e.distance = 0;
    e.timer    = 0;
    return e;
}

bool DoomGame::isSpawned(UID uid) {
    for (uint8_t i = 0; i < _numEnts; i++)
        if (_ent[i].uid == uid) return true;
    return false;
}

void DoomGame::spawnEnt(uint8_t type, uint8_t x, uint8_t y) {
    if (_numEnts >= MAX_ENTS) return;
    switch (type) {
        case E_ENEMY:  _ent[_numEnts++] = makeEntity(E_ENEMY,  x, y, S_STAND, 100); break;
        case E_KEY:    _ent[_numEnts++] = makeEntity(E_KEY,    x, y, S_STAND, 0);   break;
        case E_MEDKIT: _ent[_numEnts++] = makeEntity(E_MEDKIT, x, y, S_STAND, 0);   break;
    }
}

void DoomGame::spawnFireball(double x, double y) {
    if (_numEnts >= MAX_ENTS) return;
    UID uid = makeUID(E_FIREBALL, (uint8_t)x, (uint8_t)y);
    if (isSpawned(uid)) return;
    int16_t dir = FB_ANGLES + (int16_t)(atan2(y - _player.pos.y,
                                              x - _player.pos.x) / M_PI * FB_ANGLES);
    if (dir < 0) dir += FB_ANGLES * 2;
    _ent[_numEnts++] = makeEntity(E_FIREBALL, (uint8_t)x, (uint8_t)y, S_STAND, (uint8_t)dir);
}

void DoomGame::removeEnt(UID uid) {
    uint8_t i = 0;
    bool found = false;
    while (i < _numEnts) {
        if (!found && _ent[i].uid == uid) { found = true; _numEnts--; }
        if (found) _ent[i] = _ent[i + 1];
        i++;
    }
}

// ── Collision / movement ──────────────────────────────────────────────────────

DoomGame::UID DoomGame::detectCol(const uint8_t* lvl, Coords* pos,
                                  double rx, double ry, bool wallsOnly) {
    uint8_t bx = (uint8_t)(pos->x + rx);
    uint8_t by = (uint8_t)(pos->y + ry);
    uint8_t block = getBlock(lvl, bx, by);

    if (block == E_WALL) {
        return makeUID(block, bx, by);
    }
    if (wallsOnly) return (UID)0;

    for (uint8_t i = 0; i < _numEnts; i++) {
        if (&_ent[i].pos == pos) continue;
        uint8_t type = uidType(_ent[i].uid);
        if (type != E_ENEMY || _ent[i].state == S_DEAD || _ent[i].state == S_HIDDEN) continue;
        Coords nc = {_ent[i].pos.x - rx, _ent[i].pos.y - ry};
        uint8_t dist = coordsDist(pos, &nc);
        if (dist < ENE_COL_DIST && dist < _ent[i].distance) return _ent[i].uid;
    }
    return (UID)0;
}

DoomGame::UID DoomGame::movePos(const uint8_t* lvl, Coords* pos,
                                double rx, double ry, bool wallsOnly) {
    UID cx = detectCol(lvl, pos, rx, 0, wallsOnly);
    UID cy = detectCol(lvl, pos, 0, ry, wallsOnly);
    if (!cx) pos->x += rx;
    if (!cy) pos->y += ry;
    return cx || cy ? (cx ? cx : cy) : (UID)0;
}

void DoomGame::fire() {
    for (uint8_t i = 0; i < _numEnts; i++) {
        if (uidType(_ent[i].uid) != E_ENEMY) continue;
        if (_ent[i].state == S_DEAD || _ent[i].state == S_HIDDEN) continue;
        Coords t = toView(&_ent[i].pos);
        if (fabs(t.x) < 20 && t.y > 0) {
            int d = (int)(fabs(t.x) * _ent[i].distance);
            uint8_t dmg = (d > 0) ? (uint8_t)min((int)GUN_MAX_DMG,
                                                  (int)(GUN_MAX_DMG / d / 5)) : GUN_MAX_DMG;
            if (dmg > 0) {
                _ent[i].health = (uint8_t)max(0, (int)_ent[i].health - dmg);
                _ent[i].state  = S_HIT;
                _ent[i].timer  = 4;
            }
        }
    }
}

void DoomGame::updateEnts(const uint8_t* lvl) {
    uint8_t i = 0;
    while (i < _numEnts) {
        _ent[i].distance = coordsDist(&_player.pos, &_ent[i].pos);
        if (_ent[i].timer > 0) _ent[i].timer--;

        if (_ent[i].distance > MAX_ENT_DIST) {
            removeEnt(_ent[i].uid);
            continue;
        }
        if (_ent[i].state == S_HIDDEN) { i++; continue; }

        uint8_t type = uidType(_ent[i].uid);

        switch (type) {
            case E_ENEMY: {
                if (_ent[i].health == 0) {
                    if (_ent[i].state != S_DEAD) { _ent[i].state = S_DEAD; _ent[i].timer = 6; }
                } else if (_ent[i].state == S_HIT) {
                    if (_ent[i].timer == 0) { _ent[i].state = S_ALERT; _ent[i].timer = 40; }
                } else if (_ent[i].state == S_FIRING) {
                    if (_ent[i].timer == 0) { _ent[i].state = S_ALERT; _ent[i].timer = 40; }
                } else {
                    if (_ent[i].distance > ENE_MELEE_DIST && _ent[i].distance < MAX_ENE_VIEW) {
                        if (_ent[i].state != S_ALERT) {
                            _ent[i].state = S_ALERT;
                            _ent[i].timer = 20;
                        } else {
                            if (_ent[i].timer == 0) {
                                spawnFireball(_ent[i].pos.x, _ent[i].pos.y);
                                _ent[i].state = S_FIRING;
                                _ent[i].timer = 6;
                            } else {
                                movePos(lvl, &_ent[i].pos,
                                        _sgn(_player.pos.x, _ent[i].pos.x) * ENE_SPD * _delta,
                                        _sgn(_player.pos.y, _ent[i].pos.y) * ENE_SPD * _delta,
                                        true);
                            }
                        }
                    } else if (_ent[i].distance <= ENE_MELEE_DIST) {
                        if (_ent[i].state != S_MELEE) {
                            _ent[i].state = S_MELEE;
                            _ent[i].timer = 10;
                        } else if (_ent[i].timer == 0) {
                            _player.health = (uint8_t)max(0, (int)_player.health - ENE_MELEE_DMG);
                            _ent[i].timer  = 14;
                            _flashScr      = 1;
                            updateHud();
                        }
                    } else {
                        _ent[i].state = S_STAND;
                    }
                }
                break;
            }

            case E_FIREBALL: {
                if (_ent[i].distance < FB_COL_DIST) {
                    _player.health = (uint8_t)max(0, (int)_player.health - ENE_FB_DMG);
                    _flashScr = 1;
                    updateHud();
                    removeEnt(_ent[i].uid);
                    continue;
                } else {
                    UID col = movePos(
                        lvl, &_ent[i].pos,
                        cos((double)_ent[i].health / FB_ANGLES * M_PI) * FB_SPD,
                        sin((double)_ent[i].health / FB_ANGLES * M_PI) * FB_SPD,
                        true);
                    if (col) { removeEnt(_ent[i].uid); continue; }
                }
                break;
            }

            case E_MEDKIT: {
                if (_ent[i].distance < ITEM_COL_DIST) {
                    _ent[i].state  = S_HIDDEN;
                    _player.health = (uint8_t)min(100, (int)_player.health + 50);
                    _flashScr      = 1;
                    updateHud();
                }
                break;
            }

            case E_KEY: {
                if (_ent[i].distance < ITEM_COL_DIST) {
                    _ent[i].state = S_HIDDEN;
                    _player.keys++;
                    _flashScr = 1;
                    updateHud();
                }
                break;
            }
        }
        i++;
    }
}

// ── View transform ────────────────────────────────────────────────────────────

DoomGame::Coords DoomGame::toView(Coords* pos) {
    double sx = pos->x - _player.pos.x;
    double sy = pos->y - _player.pos.y;
    double inv = 1.0 / (_player.plane.x * _player.dir.y - _player.dir.x * _player.plane.y);
    return {
        inv * (_player.dir.y * sx - _player.dir.x * sy),
        inv * (-_player.plane.y * sx + _player.plane.x * sy)
    };
}

void DoomGame::sortEnts() {
    uint8_t gap = _numEnts;
    bool swapped = false;
    while (gap > 1 || swapped) {
        gap = (gap * 10) / 13;
        if (gap == 9 || gap == 10) gap = 11;
        if (gap < 1) gap = 1;
        swapped = false;
        for (uint8_t i = 0; i < _numEnts - gap; i++) {
            uint8_t j = i + gap;
            if (_ent[i].distance < _ent[j].distance) {
                _sw(_ent[i], _ent[j]);
                swapped = true;
            }
        }
    }
}

// ── Draw primitives ───────────────────────────────────────────────────────────

bool DoomGame::gradPixel(uint8_t x, uint8_t y, uint8_t i) {
    if (i == 0) return false;
    if (i >= GRAD_N - 1) return true;
    uint8_t ci = (uint8_t)max(0, min((int)(GRAD_N - 1), (int)i));
    uint8_t idx = ci * DOOM_GRAD_W * DOOM_GRAD_H
                  + y * DOOM_GRAD_W % (DOOM_GRAD_W * DOOM_GRAD_H)
                  + x / DOOM_GRAD_H % DOOM_GRAD_W;
    return doom_read_bit(pgm_read_byte(doom_gradient + idx), x % 8);
}

void DoomGame::drawByte(uint8_t x, uint8_t y, uint8_t b) {
    _buf[(y / 8) * SCREEN_W + x] = b;
}

void DoomGame::drawPx(int8_t x, int8_t y, bool col, bool viewport) {
    if (x < 0 || x >= SCREEN_W || y < 0 || y >= (viewport ? RENDER_H : SCREEN_H)) return;
    if (col) _buf[x + (y / 8) * SCREEN_W] |=  (1 << (y & 7));
    else     _buf[x + (y / 8) * SCREEN_W] &= ~(1 << (y & 7));
}

void DoomGame::drawVLine(uint8_t x, int8_t sy, int8_t ey, uint8_t intensity) {
    int8_t ly  = (int8_t)max((int)min(sy, ey), 0);
    int8_t hy  = (int8_t)min((int)max(sy, ey), (int)(RENDER_H - 1));
    uint8_t bp = 0;

    for (uint8_t c = 0; c < RES_DIV; c++) {
        int8_t  y = ly;
        uint8_t b = 0;
        while (y <= hy) {
            bp = y % 8;
            if (gradPixel(x + c, y, intensity)) b |= (1 << bp);
            if (bp == 7) { drawByte(x + c, y, b); b = 0; }
            y++;
        }
        if (bp != 7) drawByte(x + c, y - 1, b);
    }
}

void DoomGame::drawSprite(int8_t x, int8_t y,
                          const uint8_t* bmp, const uint8_t* msk,
                          int16_t w, int16_t h, uint8_t frame, double dist) {
    uint8_t tw   = (uint8_t)(w / dist);
    uint8_t th   = (uint8_t)(h / dist);
    uint8_t bw   = w / 8;
    uint8_t psz  = (uint8_t)max(1.0, 1.0 / dist);
    uint16_t sof = bw * h * frame;

    int zi = min(max((int)x, 0), ZBUF_SIZE - 1) / ZDIV;
    if (_zbuf[zi] < (uint8_t)(dist * DIST_MULT)) return;

    for (uint8_t ty = 0; ty < th; ty += psz) {
        if (y + ty < 0 || y + ty >= RENDER_H) continue;
        uint8_t spy = (uint8_t)(ty * dist);
        for (uint8_t tx = 0; tx < tw; tx += psz) {
            uint8_t spx = (uint8_t)(tx * dist);
            uint16_t bo = sof + spy * bw + spx / 8;
            if (x + tx < 0 || x + tx >= SCREEN_W) continue;
            if (doom_read_bit(pgm_read_byte(msk + bo), spx % 8)) {
                bool px = doom_read_bit(pgm_read_byte(bmp + bo), spx % 8);
                for (uint8_t ox = 0; ox < psz; ox++)
                    for (uint8_t oy = 0; oy < psz; oy++)
                        drawPx(x + tx + ox, y + ty + oy, px, true);
            }
        }
    }
}

void DoomGame::drawChar(int8_t x, int8_t y, char ch) {
    const char* cm = DOOM_CHAR_MAP;
    uint8_t c = 0;
    while (cm[c] != ch && cm[c] != '\0') c++;
    uint8_t boff = c / 2;
    for (uint8_t line = 0; line < DOOM_CHAR_H; line++) {
        uint8_t b = pgm_read_byte(doom_bmp_font + line * DOOM_FONT_W + boff);
        for (uint8_t n = 0; n < DOOM_CHAR_W; n++) {
            if (doom_read_bit(b, (c % 2 == 0 ? 0 : 4) + n))
                drawPx(x + n, y + line, true, false);
        }
    }
}

void DoomGame::drawStr(int8_t x, int8_t y, const char* s) {
    int8_t px = x;
    while (*s) {
        drawChar(px, y, *s++);
        px += DOOM_CHAR_W + 1;
        if (px >= SCREEN_W) return;
    }
}

void DoomGame::drawNum(int8_t x, int8_t y, uint8_t n) {
    char buf[4];
    itoa(n, buf, 10);
    drawStr(x, y, buf);
}

void DoomGame::fadeScreen(uint8_t intensity, bool col) {
    for (uint8_t x = 0; x < SCREEN_W; x++)
        for (uint8_t fy = 0; fy < SCREEN_H; fy++)
            if (gradPixel(x, fy, intensity))
                drawPx(x, fy, col, false);
}

// ── Scene rendering ───────────────────────────────────────────────────────────

void DoomGame::renderMap(const uint8_t* lvl, double vh) {
    UID last_uid = 0;

    for (uint8_t col = 0; col < SCREEN_W; col += RES_DIV) {
        double cam_x  = 2.0 * col / SCREEN_W - 1.0;
        double ray_x  = _player.dir.x + _player.plane.x * cam_x;
        double ray_y  = _player.dir.y + _player.plane.y * cam_x;
        uint8_t map_x = (uint8_t)_player.pos.x;
        uint8_t map_y = (uint8_t)_player.pos.y;
        Coords  mc    = _player.pos;

        double delta_x = fabs(ray_x < 1e-10 && ray_x > -1e-10 ? 1e10 : 1.0 / ray_x);
        double delta_y = fabs(ray_y < 1e-10 && ray_y > -1e-10 ? 1e10 : 1.0 / ray_y);

        int8_t step_x, step_y;
        double side_x, side_y;

        if (ray_x < 0) { step_x = -1; side_x = (_player.pos.x - map_x) * delta_x; }
        else           { step_x =  1; side_x = (map_x + 1.0 - _player.pos.x) * delta_x; }
        if (ray_y < 0) { step_y = -1; side_y = (_player.pos.y - map_y) * delta_y; }
        else           { step_y =  1; side_y = (map_y + 1.0 - _player.pos.y) * delta_y; }

        uint8_t depth = 0;
        bool hit = false, side = false;

        while (!hit && depth < MAX_DEPTH) {
            if (side_x < side_y) { side_x += delta_x; map_x += step_x; side = false; }
            else                 { side_y += delta_y; map_y += step_y; side = true;  }

            uint8_t block = getBlock(lvl, map_x, map_y);

            if (block == E_WALL) {
                hit = true;
            } else {
                if (block == E_ENEMY || (block & 0x08)) {
                    if (coordsDist(&_player.pos, &mc) < MAX_ENT_DIST) {
                        UID uid = makeUID(block, map_x, map_y);
                        if (last_uid != uid && !isSpawned(uid)) {
                            spawnEnt(block, map_x, map_y);
                            last_uid = uid;
                        }
                    }
                }
            }
            depth++;
        }

        if (hit) {
            double distance;
            if (!side) distance = max(1.0, (map_x - _player.pos.x + (1 - step_x) / 2.0) / ray_x);
            else       distance = max(1.0, (map_y - _player.pos.y + (1 - step_y) / 2.0) / ray_y);

            _zbuf[col / ZDIV] = (uint8_t)min(distance * DIST_MULT, 255.0);

            uint8_t lh = (uint8_t)(RENDER_H / distance);
            uint8_t gi = (uint8_t)max(0.0, GRAD_N - distance / MAX_DEPTH * GRAD_N - side * 2.0);

            drawVLine(col,
                      (int8_t)(vh / distance - lh / 2.0 + RENDER_H / 2),
                      (int8_t)(vh / distance + lh / 2.0 + RENDER_H / 2),
                      gi);
        }
    }
}

void DoomGame::renderEnts(double vh) {
    sortEnts();

    for (uint8_t i = 0; i < _numEnts; i++) {
        if (_ent[i].state == S_HIDDEN) continue;

        Coords t  = toView(&_ent[i].pos);
        if (t.y <= 0.1 || t.y > MAX_SPR_DEPTH) continue;

        int16_t sx = (int16_t)(HALF_W * (1.0 + t.x / t.y));
        int8_t  sy = (int8_t)(RENDER_H / 2.0 + vh / t.y);
        uint8_t tp = uidType(_ent[i].uid);

        if (sx < -HALF_W || sx > SCREEN_W + HALF_W) continue;

        switch (tp) {
            case E_ENEMY: {
                uint8_t fr = 0;
                if      (_ent[i].state == S_ALERT)  fr = (uint8_t)((millis() / 500) % 2);
                else if (_ent[i].state == S_FIRING)  fr = 2;
                else if (_ent[i].state == S_HIT)     fr = 3;
                else if (_ent[i].state == S_MELEE)   fr = _ent[i].timer > 10 ? 2 : 1;
                else if (_ent[i].state == S_DEAD)    fr = _ent[i].timer > 0  ? 3 : 4;
                drawSprite((int8_t)(sx - DOOM_IMP_W * 0.5 / t.y),
                           (int8_t)(sy - 8.0 / t.y),
                           doom_bmp_imp, doom_bmp_imp_mask,
                           DOOM_IMP_W, DOOM_IMP_H, fr, t.y);
                break;
            }
            case E_FIREBALL: {
                drawSprite((int8_t)(sx - DOOM_FB_W / 2.0 / t.y),
                           (int8_t)(sy - DOOM_FB_H / 2.0 / t.y),
                           doom_bmp_fireball, doom_bmp_fireball_mask,
                           DOOM_FB_W, DOOM_FB_H, 0, t.y);
                break;
            }
            case E_MEDKIT: {
                drawSprite((int8_t)(sx - DOOM_ITEM_W / 2.0 / t.y),
                           (int8_t)(sy + 5.0 / t.y),
                           doom_bmp_items, doom_bmp_items_mask,
                           DOOM_ITEM_W, DOOM_ITEM_H, 0, t.y);
                break;
            }
            case E_KEY: {
                drawSprite((int8_t)(sx - DOOM_ITEM_W / 2.0 / t.y),
                           (int8_t)(sy + 5.0 / t.y),
                           doom_bmp_items, doom_bmp_items_mask,
                           DOOM_ITEM_W, DOOM_ITEM_H, 1, t.y);
                break;
            }
        }
    }
}

void DoomGame::renderGun(Adafruit_SSD1306& d, uint8_t gpos, double jog) {
    int8_t gx = (int8_t)(48 + sin((double)millis() * JOG_SPD) * 10 * jog);
    int8_t gy = (int8_t)(RENDER_H - gpos + fabs(cos((double)millis() * JOG_SPD)) * 8 * jog);

    if (gpos > GUN_SHOT - 2) {
        d.drawBitmap(gx + 6, gy - 11, doom_bmp_fire, DOOM_FIRE_W, DOOM_FIRE_H, SSD1306_WHITE);
    }

    int clh = min((int)(gy + DOOM_GUN_H), (int)RENDER_H) - gy;
    if (clh <= 0) return;
    uint8_t clip = (uint8_t)max(0, clh);
    d.drawBitmap(gx, gy, doom_bmp_gun_mask, DOOM_GUN_W, clip, SSD1306_BLACK);
    d.drawBitmap(gx, gy, doom_bmp_gun,      DOOM_GUN_W, clip, SSD1306_WHITE);
}

void DoomGame::renderHud() {
    drawChar(2,                   58, '{');
    drawChar(2 + DOOM_CHAR_W,     58, '}');
    drawChar(40,                  58, '[');
    drawChar(40 + DOOM_CHAR_W,    58, ']');
    updateHud();
    _hudReady = true;
}

void DoomGame::updateHud() {
    if (!_d) return;
    _d->fillRect(12, 58, 20, 6, SSD1306_BLACK);
    _d->fillRect(50, 58, 10, 6, SSD1306_BLACK);
    drawNum(12, 58, _player.health);
    drawNum(50, 58, _player.keys);
}

// ── Scene loops ───────────────────────────────────────────────────────────────

bool DoomGame::loopIntro(bool btnPressed, Adafruit_SSD1306& d) {
    d.clearDisplay();
    d.drawBitmap(
        (SCREEN_W - DOOM_LOGO_W) / 2,
        (SCREEN_H - DOOM_LOGO_H) / 3,
        doom_bmp_logo, DOOM_LOGO_W, DOOM_LOGO_H, SSD1306_WHITE);

    drawStr(39, 51, "PRESS FIRE");

    if (btnPressed) {
        _numEnts  = 0;
        _numSents = 0;
        _gunPos   = 0;
        _viewH    = 0;
        _jogging  = 0;
        _fade     = GRAD_N - 1;
        _hudReady = false;
        _gunFired = false;
        memset(_zbuf, 0xFF, ZBUF_SIZE);
        initLevel(doom_level_1);
        _scene = SC_PLAY;
    }
    return false;
}

bool DoomGame::loopPlay(float dt, float gravX, float gravY,
                        bool btnHeld, bool btnPressed, Adafruit_SSD1306& d) {
    // Clear 3D viewport only (7 pages × 128 bytes = 896 bytes)
    memset(_buf, 0, 128 * 7);

    if (_player.health > 0) {
        // Forward / backward via Y tilt
        if (gravY < -TILT) {
            _player.velocity += (MOV_SPD - _player.velocity) * 0.4;
        } else if (gravY > TILT) {
            _player.velocity += (-MOV_SPD - _player.velocity) * 0.4;
        } else {
            _player.velocity *= 0.5;
        }
        _jogging = fabs(_player.velocity) * MOV_SPD_INV;

        // Left / right rotation via X tilt
        if (gravX > TILT || gravX < -TILT) {
            double rs  = ROT_SPD * _delta * (gravX > 0 ? -1.0 : 1.0);
            double odx = _player.dir.x;
            _player.dir.x   = _player.dir.x   * cos(rs) - _player.dir.y   * sin(rs);
            _player.dir.y   = odx              * sin(rs) + _player.dir.y   * cos(rs);
            double opx = _player.plane.x;
            _player.plane.x = _player.plane.x  * cos(rs) - _player.plane.y * sin(rs);
            _player.plane.y = opx              * sin(rs) + _player.plane.y * cos(rs);
        }

        _viewH = fabs(sin((double)millis() * JOG_SPD)) * 6 * _jogging;

        // Gun animation
        if      (_gunPos > GUN_TARGET)          { _gunPos--; }
        else if (_gunPos < GUN_TARGET)           { _gunPos += 2; }
        else if (!_gunFired && btnHeld)          { _gunPos = GUN_SHOT; _gunFired = true; fire(); }
        else if (_gunFired && !btnHeld)          { _gunFired = false; }
    } else {
        // Dead — camera sinks
        if (_viewH > -10) _viewH--;
        else if (btnPressed) { begin(); return false; }
        if (_gunPos > 1) _gunPos -= 2;
    }

    // Move player
    if (fabs(_player.velocity) > 0.003) {
        movePos(doom_level_1, &_player.pos,
                _player.dir.x * _player.velocity * _delta,
                _player.dir.y * _player.velocity * _delta);
    } else {
        _player.velocity = 0;
    }

    updateEnts(doom_level_1);
    renderMap(doom_level_1, _viewH);
    renderEnts(_viewH);
    renderGun(d, _gunPos, _jogging);

    if (_fade > 0) {
        fadeScreen(_fade, false);
        _fade--;
        if (_fade == 0) renderHud();
    }

    return false;
}
