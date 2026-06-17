#pragma once
#include <Adafruit_SSD1306.h>
#include <stdint.h>

class PinballGame {
public:
    void begin();
    bool update(float dt, bool btnHeld, bool btnPressed,
                bool shake, float gravX, float gravY, Adafruit_SSD1306& d);

private:
    enum State : uint8_t { READY, PLAYING, BALL_LOST, GAME_OVER };

    static constexpr int   LIVES_MAX   = 3;
    static constexpr int   NUM_BUMP    = 4;   // diamond formation
    static constexpr int   NUM_TARG    = 3;
    static constexpr int   NUM_LANE    = 3;
    static constexpr int   NUM_KICKER  = 2;
    static constexpr int   TRAIL_LEN   = 4;   // ball ghost trail positions
    static constexpr float BALL_R      = 3.0f;
    static constexpr int   MULT_MAX    = 5;
    static constexpr int   NUDGES_MAX  = 3;

    // Ball
    float _bx, _by, _bvx, _bvy;

    // Ball ghost trail
    float _trailX[TRAIL_LEN];
    float _trailY[TRAIL_LEN];
    int   _trailHead;

    // Flippers
    float _leftAngle, _rightAngle;

    // Bumpers
    int  _bumperFlash[NUM_BUMP];
    bool _bumperHit[NUM_BUMP];

    // Drop targets
    bool _targetUp[NUM_TARG];
    int  _targetFlash[NUM_TARG];
    int  _targetClears;   // how many times all targets cleared this ball

    // Rollover lanes
    bool _laneHit[NUM_LANE];
    int  _laneFlash[NUM_LANE];

    // Slingshot
    int  _slingFlash[2];

    // Spinner
    int  _spinTicks;
    int  _spinState;
    int  _spinTimer;

    // Side wall kickers
    int  _kickerFlash[NUM_KICKER];

    // Center post
    int  _postFlash;

    // Jackpot bumper (lights up after JACK_CLEARS target clears)
    bool _jackpotReady;
    int  _jackpotBumper;  // index 0-3 of which bumper is lit as jackpot
    int  _jackpotFlash;

    // Spiral loop (center between slingshots)
    bool  _loopMode;   // true while ball is on scripted orbit
    float _loopEntry;  // angle at which ball entered the loop
    float _loopAngle;  // current orbit angle (advances clockwise)
    int   _loopFlash;  // post-loop SPIRALE! flash counter

    // Kickback (one-shot left outlane save)
    bool _kickbackAvail;
    int  _kickbackFlash;

    // Ball save
    bool     _ballSave;
    uint32_t _ballSaveMs;

    // Skill shot
    bool _skillShotAvail;
    int  _skillFlash;

    // Tilt
    bool     _tilted;
    uint32_t _tiltMs;

    // Scoring
    int  _score;
    int  _hiScore;
    int  _prevHiScore;   // hi-score at start of this game (for RECORD! detection)
    int  _ballScore;     // score earned on current ball
    int  _lives;
    int  _mult;
    int  _nudgesLeft;
    int  _multFlash;
    int  _bonusFlash;

    State    _state;
    uint32_t _stateMs;
    bool     _lastShake;

    void resetBall();
    void resetTargets();
    void resetLanes();
    void stepPhysics(float dt, bool leftUp, bool rightUp);
    void render(Adafruit_SSD1306& d);
    void awardPoints(int base);
    void checkMultiplier();
};
