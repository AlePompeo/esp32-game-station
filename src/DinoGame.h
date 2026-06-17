#pragma once
#include <Adafruit_SSD1306.h>
#include <stdint.h>

class DinoGame {
public:
    void begin();
    bool update(float dt, bool btnPressed, bool shake, Adafruit_SSD1306& d);

private:
    enum State : uint8_t { WAITING, RUNNING, DEAD };

    // Screen layout
    static constexpr int16_t DINO_X    = 4;   // dino left edge
    static constexpr int16_t DINO_BASE = 40;  // dino body top-y when on ground
    static constexpr int16_t GROUND_Y  = 60;  // ground line

    // Jump: simulation runs at 60 fps equivalent (original Arduboy rate)
    static constexpr float JUMP_INTERVAL = 1.0f / 60.0f;
    // Collision: dino safe when _jumpH >= SAFE_HEIGHT (matches original d_jump_t>=14)
    static constexpr int16_t SAFE_HEIGHT = 38;

    State    _state;
    uint16_t _score;
    uint16_t _bestScore;  // persists across games
    float    _speed;      // px/s for obstacles and score
    float    _scoreAcc;   // fractional score accumulator

    // Jump physics (reproduced exactly from the Arduboy source at 60 fps)
    int16_t  _jumpH;      // pixels above DINO_BASE (0 = on ground)
    uint8_t  _jumpT;      // jump phase counter (0 = grounded)
    float    _jumpAcc;    // dt accumulator for 60-fps jump simulation

    // Leg animation
    uint8_t  _legFrame;   // 0 = still, 1/2 alternate while running
    float    _legAcc;

    // Obstacle
    float    _cactX;
    uint8_t  _cactType;   // 0 = single, 1 = double

    // Cloud (parallax)
    float    _cloudX;
    int8_t   _cloudY;

    // Death sequence
    float    _deathTimer;

    bool     _lastBtn;
    bool     _lastShake;

    void advanceJump();
    void drawPageBitmap(Adafruit_SSD1306& d, int16_t x, int16_t y,
                        const uint8_t* bmp, uint8_t w, uint8_t h) const;
    void renderScene(Adafruit_SSD1306& d, bool tumble) const;
};
