#pragma once
#include <Adafruit_SSD1306.h>
#include <stdint.h>

// Flappy Bird inspired by FlappyBall for Arduboy (MLXXXp / Scott Allen)
// Original: BSD-3-Clause  |  Ported to ESP32+SSD1306 with float physics.
class FlappyGame {
public:
    void begin();
    bool update(float dt, bool btnPressed, bool shake, Adafruit_SSD1306& d);

private:
    enum State : uint8_t { READY, PLAYING, DEAD };

    static constexpr int    MAX_PIPES     = 4;
    static constexpr int    NUM_STARS     = 10;

    // Ball (x fixed at 32, matching original BALL_X)
    static constexpr int    FL_BALL_X     = 32;
    static constexpr int    FL_BALL_R     = 4;

    // Physics – converted from fixed-point: accel=0.25 px/f², jump=-2.25 px/f @ 30 fps
    static constexpr float  FL_GRAVITY    = 225.0f;  // 0.25 * 30^2
    static constexpr float  FL_JUMP_VY    = -67.5f;  // -2.25 * 30

    // Field
    static constexpr int    FL_FLOOR_Y    = 63;      // HEIGHT - 1

    // Pipes – direct port from FlappyBall defines
    static constexpr int    FL_PIPE_W     = 12;
    static constexpr int    FL_CAP_W      = 2;
    static constexpr int    FL_CAP_H      = 3;
    static constexpr int    FL_PIPE_MIN_H = 6;
    static constexpr int    FL_GAP_MAX    = 30;
    static constexpr int    FL_GAP_MIN    = 18;
    static constexpr int    FL_GAP_REDUCE = 5;       // pipes before gap shrinks by 1
    static constexpr float  FL_PIPE_SPEED = 60.0f;   // 2 px/frame × 30 fps
    static constexpr float  FL_SPAWN_SECS = 32.0f / 30.0f;  // 32 frames at 30 fps

    static constexpr uint32_t FL_DEAD_MS  = 2500;

    struct Pipe { float x; int8_t gapY; bool active; bool scored; };

    // Ball
    float  _birdY, _birdVY;
    float  _prevBirdY;
    int8_t _wingLen;   // animates 4→3→2→1→0→4…

    // Pipes
    Pipe   _pipes[MAX_PIPES];
    int8_t _pipeGap;
    int8_t _gapReduce;

    // Floating score popup ("+N" rises away from the ball when scoring)
    int16_t _floatX, _floatY;
    int8_t  _floatFrames;
    int     _floatScore;

    // Stars background
    float  _starX[NUM_STARS], _starY[NUM_STARS];

    float    _spawnTimer;
    int      _score, _hiScore;
    int      _pipesPassed;
    State    _state;
    uint32_t _deadMs;
    bool     _lastBtn, _lastShake;

    void spawnPipe();
    void drawBall(Adafruit_SSD1306& d) const;
    void drawPipes(Adafruit_SSD1306& d) const;
    void drawStars(Adafruit_SSD1306& d) const;
    bool ballHitsPipe() const;
};
