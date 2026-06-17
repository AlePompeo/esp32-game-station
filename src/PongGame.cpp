#include "PongGame.h"
#include <Arduino.h>
#include <math.h>

static const char* const DIFF_NAMES[PongGame::DIFF_COUNT] = { "FACILE", "MEDIO", "DIFFICILE" };

// ─── Difficulty parameters ────────────────────────────────────────────────────

void PongGame::applyDifficulty() {
    switch (_diff) {
        case DIFF_EASY:
            _aiSpeed       = 24.f;
            _aiDzone       = 6.f;
            _aiAlwaysTracks = false;
            break;
        case DIFF_MEDIUM:
            _aiSpeed       = 42.f;
            _aiDzone       = 2.f;
            _aiAlwaysTracks = false;
            break;
        case DIFF_HARD:
            _aiSpeed       = 68.f;
            _aiDzone       = 0.f;
            _aiAlwaysTracks = true;   // never drifts to center
            break;
        default: break;
    }
}

// ─── Begin ────────────────────────────────────────────────────────────────────
// _diff is NOT reset here so the selector reopens on the last-chosen level.

void PongGame::begin() {
    _scorePlayer  = 0;
    _scoreAI      = 0;
    _playerY      = (PG_FIELD_TOP + PG_FIELD_BOT) * 0.5f;
    _aiY          = _playerY;
    _state        = DIFF_SELECT;
    _stateMs      = millis();
    _lastBtn      = false;
    _lastShake    = false;
    _playerServes = true;
    applyDifficulty();
    resetBall();
}

// Ball at center; direction toward server, fixed launch angle with random vertical sign
void PongGame::resetBall() {
    _ball.x  = 64.f;
    _ball.y  = (PG_FIELD_TOP + PG_FIELD_BOT) * 0.5f;
    const float vy_sign = (random(0, 2) == 0) ? 1.f : -1.f;
    const float angle   = 0.35f;
    _ball.vx = (_playerServes ? -1.f : 1.f) * PG_BALL_SPD * cosf(angle);
    _ball.vy = vy_sign * PG_BALL_SPD * sinf(angle);
}

// ─── Physics ──────────────────────────────────────────────────────────────────

void PongGame::stepPhysics(float dt) {
    _ball.x += _ball.vx * dt;
    _ball.y += _ball.vy * dt;

    // Top / bottom wall reflection
    if (_ball.y - PG_BALL_R <= PG_FIELD_TOP) {
        _ball.y  = PG_FIELD_TOP + PG_BALL_R;
        _ball.vy = fabsf(_ball.vy);
    } else if (_ball.y + PG_BALL_R >= PG_FIELD_BOT) {
        _ball.y  = PG_FIELD_BOT - PG_BALL_R;
        _ball.vy = -fabsf(_ball.vy);
    }

    // Player paddle — angle depends on relative hit position (-1..+1 → up to ±63°)
    if (_ball.vx < 0.f &&
        _ball.x - PG_BALL_R <= PG_PLAYER_FACE &&
        _ball.x - PG_BALL_R >= PG_PLAYER_LX - 1.f &&
        _ball.y >= _playerY - PG_HALF_PAD &&
        _ball.y <= _playerY + PG_HALF_PAD)
    {
        _ball.x  = PG_PLAYER_FACE + PG_BALL_R;
        const float rel   = (_ball.y - _playerY) / PG_HALF_PAD;
        const float angle = rel * 1.1f;
        float spd = sqrtf(_ball.vx*_ball.vx + _ball.vy*_ball.vy) * PG_SPEED_UP;
        if (spd > PG_MAX_SPD) spd = PG_MAX_SPD;
        _ball.vx =  spd * cosf(fabsf(angle));
        _ball.vy =  spd * sinf(angle);
    }

    // AI paddle
    if (_ball.vx > 0.f &&
        _ball.x + PG_BALL_R >= PG_AI_FACE &&
        _ball.x + PG_BALL_R <= PG_AI_LX + PG_PADDLE_W + 1.f &&
        _ball.y >= _aiY - PG_HALF_PAD &&
        _ball.y <= _aiY + PG_HALF_PAD)
    {
        _ball.x  = PG_AI_FACE - PG_BALL_R;
        const float rel   = (_ball.y - _aiY) / PG_HALF_PAD;
        const float angle = rel * 1.1f;
        float spd = sqrtf(_ball.vx*_ball.vx + _ball.vy*_ball.vy) * PG_SPEED_UP;
        if (spd > PG_MAX_SPD) spd = PG_MAX_SPD;
        _ball.vx = -spd * cosf(fabsf(angle));
        _ball.vy =  spd * sinf(angle);
    }
}

// ─── AI ───────────────────────────────────────────────────────────────────────

void PongGame::stepAI(float dt) {
    // Easy/Medium: drift to center when ball retreats (exploitable gap).
    // Hard: always track ball Y.
    const float target = (_aiAlwaysTracks || _ball.vx > 0.f)
        ? _ball.y
        : (PG_FIELD_TOP + PG_FIELD_BOT) * 0.5f;

    const float dy = target - _aiY;
    if (fabsf(dy) > _aiDzone)
        _aiY += (dy > 0 ? 1.f : -1.f) * _aiSpeed * dt;

    _aiY = constrain(_aiY, PG_FIELD_TOP + PG_HALF_PAD, PG_FIELD_BOT - PG_HALF_PAD);
}

// ─── Draw ─────────────────────────────────────────────────────────────────────

void PongGame::drawDiffSelect(Adafruit_SSD1306& d) const {
    d.clearDisplay();
    d.setTextSize(1); d.setTextColor(SSD1306_WHITE);

    d.setCursor(16, 0); d.print("DIFFICOLTA' BOT");
    d.drawFastHLine(0, 9, 128, SSD1306_WHITE);

    for (int i = 0; i < DIFF_COUNT; i++) {
        const int16_t y = 14 + i * 13;
        if (i == (int)_diff) {
            d.fillRect(0, y - 1, 128, 10, SSD1306_WHITE);
            d.setTextColor(SSD1306_BLACK);
        } else {
            d.setTextColor(SSD1306_WHITE);
        }
        // Center the name
        const int16_t tx = (128 - (int16_t)(strlen(DIFF_NAMES[i]) * 6)) / 2;
        d.setCursor(tx, y); d.print(DIFF_NAMES[i]);
    }

    d.setTextColor(SSD1306_WHITE);
    d.drawFastHLine(0, 53, 128, SSD1306_WHITE);
    d.setCursor(4, 55); d.print("BTN: scorri  SCUOTI: gioca");
    d.display();
}

void PongGame::drawField(Adafruit_SSD1306& d) const {
    d.setTextSize(1); d.setTextColor(SSD1306_WHITE);
    d.setCursor(2,  0); d.print("P:"); d.print(_scorePlayer);
    d.setCursor(94, 0); d.print("AI:"); d.print(_scoreAI);
    d.drawFastHLine(0, 7, 128, SSD1306_WHITE);

    // Dashed center line
    for (int y = (int)PG_FIELD_TOP + 1; y < (int)PG_FIELD_BOT; y += 4)
        d.drawPixel(64, y, SSD1306_WHITE);

    // Paddles
    d.fillRect((int16_t)PG_PLAYER_LX,
               (int16_t)(_playerY - PG_HALF_PAD),
               PG_PADDLE_W, PG_PADDLE_H, SSD1306_WHITE);
    d.fillRect((int16_t)PG_AI_LX,
               (int16_t)(_aiY - PG_HALF_PAD),
               PG_PADDLE_W, PG_PADDLE_H, SSD1306_WHITE);

    // Ball
    d.fillRect((int16_t)(_ball.x - PG_BALL_R), (int16_t)(_ball.y - PG_BALL_R),
               (int16_t)(PG_BALL_R * 2), (int16_t)(PG_BALL_R * 2), SSD1306_WHITE);
}

// ─── Update ───────────────────────────────────────────────────────────────────

bool PongGame::update(float dt, float gravY, bool btnPressed, bool shake, Adafruit_SSD1306& d) {
    const bool btnEdge   = btnPressed && !_lastBtn;
    const bool shakeEdge = shake && !_lastShake;
    _lastBtn   = btnPressed;
    _lastShake = shake;

    // ── Difficulty selector ───────────────────────────────────────────────────
    if (_state == DIFF_SELECT) {
        if (btnEdge) {
            _diff = (DiffLevel)(((int)_diff + 1) % DIFF_COUNT);
            applyDifficulty();
        }
        if (shakeEdge) {
            _state = SERVE;
            resetBall();
        }
        drawDiffSelect(d);
        return true;
    }

    switch (_state) {

        case SERVE:
            _playerY = constrain(
                _playerY + gravY * PG_PLAYER_SCL * dt,
                PG_FIELD_TOP + PG_HALF_PAD, PG_FIELD_BOT - PG_HALF_PAD);
            if (btnEdge) { _state = PLAYING; _stateMs = millis(); }
            break;

        case PLAYING: {
            _playerY = constrain(
                _playerY + gravY * PG_PLAYER_SCL * dt,
                PG_FIELD_TOP + PG_HALF_PAD, PG_FIELD_BOT - PG_HALF_PAD);
            stepAI(dt);
            stepPhysics(dt);

            const bool aiScores     = (_ball.x + PG_BALL_R < 0.f);
            const bool playerScores = (_ball.x - PG_BALL_R > 128.f);
            if (aiScores || playerScores) {
                if (aiScores)     { _scoreAI++;     _playerServes = false; }
                if (playerScores) { _scorePlayer++; _playerServes = true;  }
                const bool over = (_scoreAI >= SCORE_TO_WIN || _scorePlayer >= SCORE_TO_WIN);
                _state   = over ? GAME_OVER : POINT_PAUSE;
                _stateMs = millis();
                if (!over) resetBall();
            }
            break;
        }

        case POINT_PAUSE:
            _playerY = constrain(
                _playerY + gravY * PG_PLAYER_SCL * dt,
                PG_FIELD_TOP + PG_HALF_PAD, PG_FIELD_BOT - PG_HALF_PAD);
            if (millis() - _stateMs > 1200UL) _state = SERVE;
            break;

        case GAME_OVER: {
            d.clearDisplay();
            d.setTextSize(2); d.setTextColor(SSD1306_WHITE);
            const bool playerWon = (_scorePlayer >= SCORE_TO_WIN);
            d.setCursor(4, 8);  d.print(playerWon ? "HAI VINTO!" : "HAI PERSO!");
            d.setTextSize(1);
            d.setCursor(24, 32); d.print("P:"); d.print(_scorePlayer);
            d.print("   AI:"); d.print(_scoreAI);
            // Show what difficulty was played
            d.setCursor(24, 42); d.print("Diff: "); d.print(DIFF_NAMES[_diff]);
            d.setCursor(4,  54); d.print("BTN: nuova partita");
            d.display();
            if (btnEdge || millis() - _stateMs > 5000UL) return false;
            return true;
        }

        default: break;
    }

    // Normal game frame
    d.clearDisplay();
    drawField(d);
    if (_state == SERVE) {
        d.setTextSize(1); d.setTextColor(SSD1306_WHITE);
        d.setCursor(34, 28); d.print("BTN: servi");
    }
    d.display();
    return true;
}
