#pragma once
#include <Adafruit_SSD1306.h>
#include "Ball.h"

class PongGame {
public:
    static constexpr int SCORE_TO_WIN = 7;

    enum DiffLevel : uint8_t { DIFF_EASY = 0, DIFF_MEDIUM, DIFF_HARD, DIFF_COUNT };

    void begin();
    bool update(float dt, float gravY, bool btnPressed, bool shake, Adafruit_SSD1306& d);

private:
    enum GState : uint8_t { DIFF_SELECT, SERVE, PLAYING, POINT_PAUSE, GAME_OVER };

    Ball      _ball;
    float     _playerY;
    float     _aiY;
    int       _scorePlayer, _scoreAI;
    GState    _state;
    uint32_t  _stateMs;
    bool      _lastBtn;
    bool      _lastShake;
    bool      _playerServes;

    // Persists across restarts so the selector opens on the last-used level
    DiffLevel _diff = DIFF_MEDIUM;

    // Runtime AI parameters (set from _diff on game start)
    float _aiSpeed;
    float _aiDzone;
    bool  _aiAlwaysTracks;  // hard: AI tracks even when ball moves away

    // ── Field geometry ────────────────────────────────────────────────────────
    static constexpr float PG_FIELD_TOP   = 8.f;
    static constexpr float PG_FIELD_BOT   = 63.f;
    static constexpr float PG_HALF_PAD    = 7.f;
    static constexpr int   PG_PADDLE_W    = 3;
    static constexpr int   PG_PADDLE_H    = 14;
    static constexpr float PG_PLAYER_LX   = 2.f;
    static constexpr float PG_AI_LX       = 123.f;
    static constexpr float PG_PLAYER_FACE = 5.f;
    static constexpr float PG_AI_FACE     = 123.f;
    static constexpr float PG_BALL_R      = 2.f;

    // ── Physics parameters ────────────────────────────────────────────────────
    static constexpr float PG_BALL_SPD    = 65.f;
    static constexpr float PG_MAX_SPD     = 190.f;
    static constexpr float PG_SPEED_UP    = 1.05f;
    static constexpr float PG_PLAYER_SCL  = 0.55f;

    void applyDifficulty();
    void resetBall();
    void stepPhysics(float dt);
    void stepAI(float dt);
    void drawField(Adafruit_SSD1306& d) const;
    void drawDiffSelect(Adafruit_SSD1306& d) const;
};
