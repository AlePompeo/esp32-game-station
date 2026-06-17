#pragma once
#include <Adafruit_SSD1306.h>
#include <stdint.h>
#include <math.h>

class DoomGame {
public:
    void begin();
    // btnHeld: for continuous fire; btnPressed: edge events (intro skip, restart)
    bool update(float dt, float gravX, float gravY,
                bool btnHeld, bool btnPressed, Adafruit_SSD1306& d);

private:
    // ── Types ─────────────────────────────────────────────────────────────────
    struct Coords { double x, y; };
    using UID   = uint16_t;
    using EType = uint8_t;

    struct Player {
        Coords  pos, dir, plane;
        double  velocity;
        uint8_t health, keys;
    };
    struct Entity {
        UID     uid;
        Coords  pos;
        uint8_t state, health, distance, timer;
    };
    struct StaticEntity {
        UID     uid;
        uint8_t x, y;
        bool    active;
    };

    // ── Game constants ────────────────────────────────────────────────────────
    static constexpr uint8_t SCREEN_W        = 128;
    static constexpr uint8_t SCREEN_H        = 64;
    static constexpr uint8_t HALF_W          = 64;
    static constexpr uint8_t RENDER_H        = 56;   // 3-D viewport; HUD is 56-63
    static constexpr uint8_t HALF_H          = 32;

    static constexpr int     RES_DIV         = 2;    // horizontal resolution divider
    static constexpr int     ZDIV            = 2;    // z-buffer resolution divider
    static constexpr int     ZBUF_SIZE       = SCREEN_W / ZDIV;
    static constexpr int     DIST_MULT       = 20;
    static constexpr int     MAX_DEPTH       = 12;
    static constexpr int     MAX_SPR_DEPTH   = 8;

    static constexpr int     LW_BASE         = 6;
    static constexpr int     LEVEL_W         = 1 << LW_BASE;  // 64
    static constexpr int     LEVEL_H         = 57;

    static constexpr double  ROT_SPD         = 0.12;
    static constexpr double  MOV_SPD         = 0.2;
    static constexpr int     MOV_SPD_INV     = 5;
    static constexpr double  JOG_SPD         = 0.005;
    static constexpr double  ENE_SPD         = 0.02;
    static constexpr double  FB_SPD          = 0.2;
    static constexpr int     FB_ANGLES       = 45;

    static constexpr int     MAX_ENTS        = 10;
    static constexpr int     MAX_SENTS       = 28;

    static constexpr int     MAX_ENT_DIST    = 200;
    static constexpr int     MAX_ENE_VIEW    = 80;
    static constexpr int     ITEM_COL_DIST   = 6;
    static constexpr int     ENE_COL_DIST    = 4;
    static constexpr int     FB_COL_DIST     = 2;
    static constexpr int     ENE_MELEE_DIST  = 6;
    static constexpr double  WALL_COL_DIST   = 0.2;

    static constexpr int     ENE_MELEE_DMG   = 8;
    static constexpr int     ENE_FB_DMG      = 20;
    static constexpr int     GUN_MAX_DMG     = 15;

    static constexpr uint8_t GUN_TARGET      = 18;
    static constexpr uint8_t GUN_SHOT        = GUN_TARGET + 4;

    // Entity types
    static constexpr uint8_t E_FLOOR=0x0, E_WALL=0xF, E_PLAYER=0x1;
    static constexpr uint8_t E_ENEMY=0x2, E_DOOR=0x4, E_LDOOR=0x5, E_EXIT=0x7;
    static constexpr uint8_t E_MEDKIT=0x8, E_KEY=0x9, E_FIREBALL=0xA;

    // Entity states
    static constexpr uint8_t S_STAND=0, S_ALERT=1, S_FIRING=2, S_MELEE=3;
    static constexpr uint8_t S_HIT=4, S_DEAD=5, S_HIDDEN=6;

    // Scenes
    static constexpr uint8_t SC_INTRO = 0, SC_PLAY = 1;

    // Tilt threshold for IMU input (px/s²; full-tilt ≈ 35)
    static constexpr float   TILT     = 12.0f;

    // Gradient
    static constexpr int     GRAD_W   = 2;
    static constexpr int     GRAD_H   = 8;
    static constexpr int     GRAD_N   = 8;

    // ── Game state ────────────────────────────────────────────────────────────
    uint8_t  _scene      = SC_INTRO;
    bool     _invertScr  = false;
    uint8_t  _flashScr   = 0;

    Player       _player;
    Entity       _ent[MAX_ENTS];
    StaticEntity _sent[MAX_SENTS];
    uint8_t      _numEnts  = 0;
    uint8_t      _numSents = 0;

    // Per-frame play state
    bool     _gunFired  = false;
    bool     _walkTog   = false;
    uint8_t  _gunPos    = 0;
    double   _viewH     = 0;
    double   _jogging   = 0;
    uint8_t  _fade      = 0;
    bool     _hudReady  = false;
    double   _delta     = 1.0;

    uint8_t  _zbuf[ZBUF_SIZE];
    uint8_t* _buf = nullptr;           // points to d.getBuffer() each frame
    Adafruit_SSD1306* _d = nullptr;    // display pointer, valid within update()

    // ── Internal helpers ──────────────────────────────────────────────────────
    inline UID    makeUID(EType t, uint8_t x, uint8_t y)
        { return (UID)(((y << LW_BASE) | x) << 4 | t); }
    inline EType  uidType(UID u) { return u & 0x0F; }
    uint8_t       coordsDist(const Coords* a, const Coords* b);

    // Level
    uint8_t  getBlock(const uint8_t* lvl, uint8_t x, uint8_t y);
    void     initLevel(const uint8_t* lvl);

    // Entity management
    Entity   makeEntity(uint8_t type, uint8_t x, uint8_t y, uint8_t st, uint8_t hp);
    bool     isSpawned(UID uid);
    void     spawnEnt(uint8_t type, uint8_t x, uint8_t y);
    void     spawnFireball(double x, double y);
    void     removeEnt(UID uid);

    // Collision / movement
    UID      detectCol(const uint8_t* lvl, Coords* pos,
                       double rx, double ry, bool wallsOnly = false);
    UID      movePos(const uint8_t* lvl, Coords* pos,
                     double rx, double ry, bool wallsOnly = false);
    void     fire();
    void     updateEnts(const uint8_t* lvl);

    // View
    Coords   toView(Coords* pos);
    void     sortEnts();

    // Pixel-level draw (uses _buf directly)
    bool     gradPixel(uint8_t x, uint8_t y, uint8_t i);
    void     drawByte(uint8_t x, uint8_t y, uint8_t b);
    void     drawPx(int8_t x, int8_t y, bool col, bool viewport = false);
    void     drawVLine(uint8_t x, int8_t sy, int8_t ey, uint8_t intensity);
    void     drawSprite(int8_t x, int8_t y,
                        const uint8_t* bmp, const uint8_t* mask,
                        int16_t w, int16_t h, uint8_t frame, double dist);
    void     drawChar(int8_t x, int8_t y, char ch);
    void     drawStr(int8_t x, int8_t y, const char* s);
    void     drawNum(int8_t x, int8_t y, uint8_t n);
    void     fadeScreen(uint8_t intensity, bool col = false);

    // Scene renders
    void     renderMap(const uint8_t* lvl, double vh);
    void     renderEnts(double vh);
    void     renderGun(Adafruit_SSD1306& d, uint8_t gpos, double jog);
    void     renderHud();
    void     updateHud();

    // Scene loops (called from update())
    bool     loopIntro(bool btnPressed, Adafruit_SSD1306& d);
    bool     loopPlay(float dt, float gravX, float gravY,
                      bool btnHeld, bool btnPressed, Adafruit_SSD1306& d);
};
