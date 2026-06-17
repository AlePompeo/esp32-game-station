#pragma once
#include <Adafruit_SSD1306.h>
#include <freertos/semphr.h>
#include <stdint.h>

class SpaceInvaders {
public:
    // Maximum grid dimensions (arrays are sized to worst-case difficulty)
    static constexpr int MAX_ROWS    = 4;
    static constexpr int MAX_COLS    = 8;
    static constexpr int MAX_PB      = 2;
    static constexpr int MAX_IB      = 4;
    static constexpr int NUM_BUNKERS = 3;
    static constexpr int BNK_ROWS    = 6;    // bunker pixel height
    static constexpr int BNK_COLS    = 12;   // bunker pixel width

    struct Bullet { float x, y; bool active; };

    // Bunker: pixel-perfect bitmask.  bit 11 = leftmost column (col 0).
    struct Bunker {
        int16_t  x, y;
        uint16_t mask[BNK_ROWS];
    };

    void begin();   // show difficulty-select screen; safe to call from Core 1

    // Core 0: pure game logic (runs only during PLAYING state)
    void logicStep(float shipX, bool firePending, float dt);

    // Core 1: render + UI; returns false when game wants a restart
    bool renderFrame(Adafruit_SSD1306& d, bool btnPressed, bool shake);

private:
    enum GState : uint8_t { DIFF_SELECT, PLAYING, WIN, LOSE };

    // ── Per-difficulty config ─────────────────────────────────────────────────
    struct DiffCfg {
        int   rows, cols, lives, maxIb;
        float baseInterval, minInterval, fireInterval, ibSpeed;
        const char* name;
    };
    static const DiffCfg DIFF[5];
    static constexpr int NUM_DIFF = 5;

    // ── Active difficulty parameters ──────────────────────────────────────────
    int   _rows, _cols, _maxIb;
    float _baseInterval, _minInterval, _fireInterval, _ibSpeed;
    float _blockBaseX;   // centred start x, recalculated per difficulty

    // ── Internal game state (Core 0 writes) ───────────────────────────────────
    bool     _alive[MAX_ROWS][MAX_COLS];
    int      _aliveCount;
    float    _blockX, _blockY, _dirX;
    float    _moveTimer, _moveInterval, _fireTimer;
    Bullet   _pb[MAX_PB];
    Bullet   _ib[MAX_IB];
    Bunker   _bunkers[NUM_BUNKERS];
    float    _shipX;
    int      _score, _lives;
    GState   _gstate;
    uint32_t _stateMs;
    int      _diffIdx;   // 0-2: current difficulty (during DIFF_SELECT: highlighted)

    // ── Snapshot for Core 1 ───────────────────────────────────────────────────
    struct Snap {
        bool     alive[MAX_ROWS][MAX_COLS];
        int      rows, cols;
        float    blockX, blockY, blockBaseX;
        Bullet   pb[MAX_PB];
        Bullet   ib[MAX_IB];
        Bunker   bunkers[NUM_BUNKERS];
        float    shipX;
        int      score, lives, diffIdx;
        GState   gstate;
        uint32_t stateMs;
    } _snap;

    SemaphoreHandle_t _mutex     = nullptr;
    bool              _rLastShake = false; // renderFrame edge detection (Core 1 only)

    void startGame(int diffIdx);   // initialise state for chosen difficulty
    void recalcSpeed();
    void firePlayer(float x);
    void fireInvader();
    void checkCollisions();
    void copyToSnap();

    // Bunker helpers
    static bool    bunkerPixel(const Bunker& b, int row, int col);
    static void    erasePixels(Bunker& b, int row, int col);  // 3×3 area
    static bool    bulletHitsBunker(const Bullet& bul, Bunker& b); // erase on hit

    static void drawInvader(Adafruit_SSD1306& d, int16_t x, int16_t y, int row);
    static void drawShip   (Adafruit_SSD1306& d, int16_t cx, int16_t y);
    static void drawBunker (Adafruit_SSD1306& d, const Bunker& b);
};
