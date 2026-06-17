#pragma once
#include <Adafruit_SSD1306.h>
#include <stdint.h>

class PacmanGame {
public:
    void begin();
    bool update(float dt, bool btnPressed, float gravX, float gravY, Adafruit_SSD1306& d);

private:
    enum State : uint8_t { READY, PLAYING, DEAD, WIN, GAME_OVER };
    static constexpr int NG = 2;

    int8_t   _pCol, _pRow, _pDir, _nDir;
    int8_t   _gCol[NG], _gRow[NG], _gDir[NG];
    uint16_t _dots[7];   // 1 bit per cell, bit15 = col 0
    int      _score, _lives, _dotsLeft;
    State    _state;
    uint32_t _stateMs, _frightMs;
    uint8_t  _moveTick, _anim;

    bool isWall(int c, int r) const;
    bool hasDot(int c, int r) const;
    void clrDot(int c, int r);
    void initDots();
    void moveGhost(int i);
    void render(Adafruit_SSD1306& d);
};
