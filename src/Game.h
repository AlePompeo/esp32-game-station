#pragma once
#include "Physics.h"
#include "IMUReader.h"
#include "Renderer.h"
#include "Button.h"

class Game {
public:
    void begin(Physics& physics);

    // Returns true when physics must run on Core 0.
    // Game runs forever (WIN/LOSE/COMPLETE → restart, no exit to menu).
    bool update(const Ball* balls, int ballCount, Physics& physics,
                const IMUReader& imu, const Button& btn, Renderer& renderer);

private:
    enum State { SPLASH, PLAYING, WIN, LOSE, COMPLETE };

    State    _state       = SPLASH;
    uint8_t  _levelIdx    = 0;
    int      _ballsInDest = 0;
    uint32_t _stateMs     = 0;
    bool     _lastShake   = false;

    void setState(State s);
    void loadLevel(uint8_t idx, Physics& physics);
};
