#pragma once
#include <Adafruit_SSD1306.h>
#include "Ball.h"

class ArkanoidGame {
public:
    static constexpr int COLS = 9;
    static constexpr int ROWS = 5;

    void begin();
    bool update(float dt, float gravX, bool btnPressed, Adafruit_SSD1306& d);

private:
    enum GState : uint8_t { LAUNCH, PLAYING, LEVEL_DONE, GAME_OVER };
    enum PUType : uint8_t { PU_WIDE = 0, PU_SLOW = 1, PU_LIFE = 2 };

    struct PowerUp { float x, y; bool active; PUType type; };

    static constexpr int   AK_HUD_H      = 8;
    static constexpr float AK_BALL_R     = 2.f;
    static constexpr int   AK_BRK_W      = 12;
    static constexpr int   AK_BRK_H      = 4;
    static constexpr int   AK_BRK_GX     = 2;
    static constexpr int   AK_BRK_GY     = 1;
    static constexpr int   AK_BRK_X0     = 2;
    static constexpr int   AK_BRK_Y0     = 10;
    static constexpr float AK_PAD_Y      = 57.f;
    static constexpr int   AK_PAD_H      = 3;
    static constexpr int   AK_PAD_W_NRM  = 22;
    static constexpr int   AK_PAD_W_WIDE = 32;
    static constexpr int   MAX_PU        = 3;
    static constexpr float AK_BALL_SPD   = 80.f;
    static constexpr float AK_MAX_SPD    = 200.f;
    static constexpr float AK_PAD_VEL    = 0.70f;
    static constexpr float AK_PU_SPD     = 28.f;

    Ball     _ball;
    float    _paddleX, _paddleW;
    uint8_t  _bricks[ROWS][COLS];
    PowerUp  _pus[MAX_PU];
    int      _score, _lives, _level;
    float    _ballSpd, _wideTimer, _slowTimer;
    GState   _state;
    uint32_t _stateMs;
    bool     _lastBtn;

    void  resetBricks();
    void  resetBall();
    void  spawnPU(float x, float y);
    bool  ballHitBrick(int col, int row);
    float currentSpeed() const;
    void  drawHUD(Adafruit_SSD1306& d) const;
};
