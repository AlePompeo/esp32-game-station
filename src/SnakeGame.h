#pragma once
#include <Adafruit_SSD1306.h>
#include <stdint.h>

class SnakeGame {
public:
    void begin();
    bool update(float dt, float gravX, float gravY, bool btnPressed, Adafruit_SSD1306& d);

private:
    enum State : uint8_t { READY, PLAYING, DEAD };
    enum Dir   : uint8_t { RIGHT = 0, DOWN = 1, LEFT = 2, UP = 3 };

    static constexpr int SN_MAX_LEN = 64;

    struct Pos { int8_t col, row; };

    Pos      _body[SN_MAX_LEN];
    int      _len;
    Dir      _dir;
    Dir      _nextDir;
    Pos      _food;
    int      _score;
    int      _hiScore  = 0;   // persists across games
    State    _state;
    uint32_t _lastMoveMs;
    uint32_t _stateMs;
    int      _foodEaten;

    void     spawnFood();
    bool     bodyOccupies(int col, int row, int skipTail) const;
    void     drawHud(Adafruit_SSD1306& d) const;
    void     drawField(Adafruit_SSD1306& d) const;
    uint32_t moveInterval() const;
};
