#pragma once
#include <Adafruit_SSD1306.h>
#include <stdint.h>

class LabyrinthGame {
public:
    void begin();   // full reset (called from menu on game select)
    bool update(float dt, float gravX, float gravY, bool btnPressed, Adafruit_SSD1306& d);

private:
    // Maze grid: 20 cols × 9 rows, cell 5 px, wall 1 px → step 6 px
    static constexpr int   LB_COLS = 20;
    static constexpr int   LB_ROWS = 9;
    static constexpr int   LB_CELL = 5;
    static constexpr int   LB_STEP = 6;
    static constexpr int   LB_X0   = 3;
    static constexpr int   LB_Y0   = 9;
    static constexpr float TILT    = 20.f;
    static constexpr uint32_t WIN_MS = 3000;

    static constexpr uint8_t WALL_N = 1, WALL_S = 2, WALL_E = 4, WALL_W = 8;

    enum State : uint8_t { READY, PLAYING, WIN };

    uint8_t  _maze[LB_ROWS][LB_COLS];
    int8_t   _px = 0, _py = 0;
    uint32_t _startMs    = 0;
    uint32_t _stateMs    = 0;
    uint32_t _lastMoveMs = 0;
    uint32_t _bestSec    = 0;  // persists across rounds (best time overall)
    uint8_t  _level      = 1;  // 1-indexed, persists across rounds
    uint32_t _moveMs     = 160; // decreases with level for difficulty scaling
    State    _state      = READY;

    void generateMaze();
    void newRound();  // partial reset: keeps _level and _bestSec
    void drawMaze(Adafruit_SSD1306& d) const;
    void drawHud(Adafruit_SSD1306& d) const;
};
