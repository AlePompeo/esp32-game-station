#pragma once
#include <Adafruit_SSD1306.h>
#include <stdint.h>

class KongGame {
public:
    void begin();
    bool update(float dt, bool btnPressed, bool shake,
                float gravX, float gravY, Adafruit_SSD1306& d);

private:
    enum State : uint8_t { WAITING, PLAYING, DYING, WIN, GAME_OVER };

    static constexpr uint8_t MAX_BARRELS = 5;
    static constexpr uint8_t MAX_FIRES   = 2;
    static constexpr uint8_t MAX_ITEMS   = 3;
    static constexpr int16_t MARIO_W     = 7;
    static constexpr int16_t MARIO_H     = 11;
    static constexpr int16_t BARREL_W    = 9;
    static constexpr int16_t BARREL_H    = 9;
    static constexpr int16_t GOR_W       = 20;
    static constexpr int16_t GOR_H       = 20;
    static constexpr int16_t FIRE_W      = 5;
    static constexpr int16_t FIRE_H      = 8;
    static constexpr int16_t ITEM_W      = 5;
    static constexpr int16_t ITEM_H      = 5;
    static constexpr uint8_t START_LIVES = 3;

    // ── game state ─────────────────────────────────────
    State    _state;
    uint16_t _score, _bestScore;
    uint8_t  _level;
    uint8_t  _lives;
    float    _stateTimer;
    bool     _lastBtn;
    uint8_t  _layout;   // 0 or 1, changes each level

    // ── player ─────────────────────────────────────────
    float   _px, _py;
    float   _pvy;
    int8_t  _pFloor;
    int8_t  _jumpFloor;  // floor where last jump began (prevents landing higher)
    bool    _onLadder;
    int8_t  _ladderIdx;
    bool    _facingRight;
    uint8_t _runFrame;
    float   _runAcc;
    float   _scoreAcc;

    // ── barrels ─────────────────────────────────────────
    struct Barrel {
        float   bx, bTop;
        float   bvy;
        int8_t  floor;
        int8_t  targetFloor;
        int8_t  dir;
        uint8_t frame;
        float   frameAcc;
    } _barrels[MAX_BARRELS];
    float _spawnTimer;

    // ── fire enemies (patrol floor 0) ─────────────────────
    struct Fire {
        float   fx;
        int8_t  dir;
        uint8_t frame;
        float   frameAcc;
        bool    active;
    } _fires[MAX_FIRES];

    // ── collectible items ────────────────────────────────
    struct Item {
        int16_t x;
        int8_t  floor;
        bool    collected;
    } _items[MAX_ITEMS];

    // ── DK ──────────────────────────────────────────────
    uint8_t _dkFrame;
    float   _dkAnim;
    uint8_t _dkIdleStep;
    float   _dkIdleAcc;

    // ── title screen animation ──────────────────────────
    float   _titleBX;
    uint8_t _titleBFrame;
    float   _titleBAcc;

    // ── helpers ─────────────────────────────────────────
    void  resetRound(bool fullReset);
    void  spawnBarrel();
    void  spawnFires();
    void  spawnItems();
    void  updatePlayer(float dt, bool btn, float gravX, float gravY);
    void  updateBarrels(float dt);
    void  updateDK(float dt);
    void  updateFires(float dt);
    void  checkItems();
    bool  checkCollision() const;
    bool  checkFireCollision() const;
    int8_t nearLadder() const;
    const int16_t* getLadderX() const;
    void  drawSprite(Adafruit_SSD1306& d, int16_t sx, int16_t sy,
                     const uint8_t* spr, uint8_t frame) const;
    void  drawScene(Adafruit_SSD1306& d) const;
    void  drawHUD(Adafruit_SSD1306& d) const;
};
