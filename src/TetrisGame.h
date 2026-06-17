#pragma once
#include <Adafruit_SSD1306.h>
#include <stdint.h>

class TetrisGame {
public:
    void begin();
    bool update(float dt, float gravX, float gravY, bool btnPressed, bool shake, Adafruit_SSD1306& d);

private:
    enum State : uint8_t { PLAYING, LINE_CLEAR, GAME_OVER };

    static constexpr int      TT_COLS           = 10;
    static constexpr int      TT_ROWS           = 16;
    static constexpr int      TT_CELL           = 4;
    static constexpr float    TT_CLEAR_PAUSE    = 0.30f;
    static constexpr float    TT_OVER_TIMEOUT   = 5.f;
    static constexpr int      TT_DROP_SCORE     = 2;
    static constexpr float    TT_TILT_THRESHOLD = 25.f;
    static constexpr float    TT_SOFT_THRESHOLD = 50.f;
    static constexpr uint32_t TT_SOFT_INTERVAL  = 80u;
    static constexpr float    TT_MOVE_INTERVAL  = 0.12f;
    static constexpr uint32_t TT_HOLD_MS        = 400u;  // long-press threshold for hold

    uint8_t  _board[TT_ROWS][TT_COLS];
    uint8_t  _type, _rot;
    int8_t   _px, _py;
    uint8_t  _nextType;
    uint8_t  _heldType;   // 0xFF = empty
    bool     _hasHeld;    // allowed to hold once per piece
    uint32_t _score;
    uint16_t _lines;
    uint8_t  _level;
    float    _fallTimer, _moveTimer, _stateTimer;
    uint16_t _clearMask;
    State    _state;
    bool     _lastBtn, _lastShake;
    uint32_t _btnDownMs;

    bool     canPlace(int8_t px, int8_t py, uint8_t type, uint8_t rot) const;
    void     lockPiece();
    int      clearLines();
    void     collapseLines();
    void     spawnPiece();
    void     holdPiece();
    void     addScore(int cleared);
    uint32_t fallIntervalMs() const;
    void     renderBoard(Adafruit_SSD1306& d) const;
    void     renderGhost(Adafruit_SSD1306& d) const;
    void     renderActivePiece(Adafruit_SSD1306& d) const;
    void     renderHUD(Adafruit_SSD1306& d) const;
    void     renderGameOver(Adafruit_SSD1306& d) const;
};
