#pragma once
#include <Adafruit_SSD1306.h>
#include <stdint.h>

class PocketFighterGame {
public:
    void begin();
    // Returns true always (game manages its own MENU→FIGHT→NEXT/OVER lifecycle)
    bool update(float dt, float gravX, float gravY, bool shake,
                bool btnPressed, Adafruit_SSD1306& d);

private:
    static constexpr uint8_t  BORDER_LEFT   = 3;
    static constexpr uint8_t  BORDER_RIGHT  = 123;
    static constexpr uint8_t  BORDER_TOP    = 7;
    static constexpr uint8_t  BORDER_BOTTOM = 54;
    static constexpr uint8_t  SCREEN_LEFT   = 0;
    static constexpr uint8_t  SCREEN_RIGHT  = 127;
    static constexpr uint8_t  JMP_TICKS = 9;   // original 18 @ 60fps, halved for 30fps
    static constexpr uint8_t  HDK_TICKS = 10;  // original 19 @ 60fps
    static constexpr uint8_t  AFPT      = 5;   // animation frame period (original 10)
    static constexpr float    MOVE_THRESH = 3.0f;
    static constexpr float    JUMP_THRESH = 5.0f;
    static constexpr uint16_t RECORD_TOTAL = 300;
    static constexpr uint16_t RECORD_HALF  = 150;
    static constexpr uint8_t  EEPROM_ADDR  = 50;

    enum Actions : uint8_t {
        ACT_NONE = 0, ACT_LEFT, ACT_RIGHT, ACT_IDLE,
        ACT_PUNCH, ACT_SHOOT, ACT_JUMP, ACT_RESERVED
    };

    enum State : uint8_t {
        STATE_MENU, STATE_FIGHT, STATE_WIN, STATE_LOSE, STATE_NEXT, STATE_GAMEOVER
    };

    struct Record {
        uint8_t  action;
        uint16_t timeStamp;
    };

    struct Fighter {
        uint8_t hp, dir, mirror;
        uint8_t x, y, dx, dy;
        uint8_t bx, by, dbx;
        uint8_t hadoukenLife, tookPunch, jumpTicks;
        uint8_t action, actionCache;
        uint8_t frameTicks, frame;
    };

    State    _state = STATE_MENU;
    uint8_t  _level = 0;
    uint8_t  _best  = 1;
    Fighter  _f[2];
    Record   _r[RECORD_TOTAL];
    uint8_t  _c0 = 0;      // player record write cursor (0..RECORD_HALF-1)
    uint16_t _c1 = 0;      // CPU record read cursor (RECORD_HALF..RECORD_TOTAL-1)
    uint8_t  _t0 = 0;      // player tick counter
    uint16_t _t1 = 0;      // CPU tick counter
    uint16_t _stateTimer = 0;

    bool _prevLeft  = false;
    bool _prevRight = false;
    bool _prevJump  = false;
    bool _prevShake = false;

    void prepare();
    void addPlayerRecord(Actions act);
    void setFighterAction(Fighter& f, Actions act);
    bool isFighterAction(const Fighter& f, Actions act) const;
    void setFighterProperAction(Fighter& f, bool isSelf, float gravX);
    void setFighterActionCache(Fighter& f, Actions act);
    bool isFighterShooting(const Fighter& f) const;
    void fighterShoot(Fighter& f);
    void finishFighterShoot(Fighter& f);
    void tickFighterShoot(Fighter& f);
    bool isFighterJumping(const Fighter& f) const;
    void setFighterJump(Fighter& f);
    void tickFighterJump(Fighter& f);
    Record& getPlayerRecord();
    void    tickPlayerRecord(bool force);
    Record& getCpuRecord();
    void    nextCpuRecord();
    void    flipCpu(Fighter& f);
    void    detectCollision(Fighter& attacker, Fighter& defender);
    // Returns true when an attack animation just finished
    bool    drawFighter(Fighter& f, bool isSelf, Adafruit_SSD1306& d);
    void    drawHUD(Adafruit_SSD1306& d);
    void    drawProgress(Adafruit_SSD1306& d);

    void doMenu(bool btnEdge, Adafruit_SSD1306& d);
    void doFight(float gravX, float gravY,
                 bool tiltLeft, bool tiltRight,
                 bool justLeft, bool justRight, bool justJump,
                 bool leftRel, bool rightRel,
                 bool shakeEdge, bool btnEdge, Adafruit_SSD1306& d);
    void doWin(Adafruit_SSD1306& d);
    void doLose(Adafruit_SSD1306& d);
    void doNext(bool btnEdge, Adafruit_SSD1306& d);
    void doOver(bool btnEdge, Adafruit_SSD1306& d);

    void drawPageBitmap(Adafruit_SSD1306& d, int16_t x, int16_t y,
                        const uint8_t* bmp, uint8_t w, uint8_t h) const;
    static bool pointInRect(int16_t px, int16_t py,
                            int16_t rx, int16_t ry, int16_t rw, int16_t rh);
    static bool rectsOverlap(int16_t x1, int16_t y1, int16_t w1, int16_t h1,
                             int16_t x2, int16_t y2, int16_t w2, int16_t h2);
};
