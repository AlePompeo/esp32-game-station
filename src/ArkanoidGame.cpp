#include "ArkanoidGame.h"
#include <Arduino.h>
#include <math.h>

// ─── begin ────────────────────────────────────────────────────────────────────

void ArkanoidGame::begin() {
    _score     = 0;
    _lives     = 3;
    _level     = 1;
    _ballSpd   = AK_BALL_SPD;
    _paddleX   = (128.f - AK_PAD_W_NRM) * 0.5f;
    _paddleW   = (float)AK_PAD_W_NRM;
    _wideTimer = 0.f;
    _slowTimer = 0.f;
    _state     = LAUNCH;
    _stateMs   = millis();
    _lastBtn   = false;
    for (int i = 0; i < MAX_PU; i++) _pus[i].active = false;
    resetBricks();
    resetBall();
}

void ArkanoidGame::resetBricks() {
    for (int r = 0; r < ROWS; r++)
        for (int c = 0; c < COLS; c++)
            _bricks[r][c] = (r < 2 && _level >= 3) ? 2 : 1;
}

void ArkanoidGame::resetBall() {
    _ball.x  = _paddleX + _paddleW * 0.5f;
    _ball.y  = AK_PAD_Y - AK_BALL_R - 1.f;
    _ball.vx = 0.f;
    _ball.vy = 0.f;
}

float ArkanoidGame::currentSpeed() const {
    return _ballSpd * (_slowTimer > 0.f ? 0.65f : 1.f);
}

// ─── Power-up spawn (20% chance, 50/30/20 type weights) ─────────────────────

void ArkanoidGame::spawnPU(float x, float y) {
    if (random(0, 5) != 0) return;
    for (int i = 0; i < MAX_PU; i++) {
        if (_pus[i].active) continue;
        _pus[i].x = x;
        _pus[i].y = y;
        const int r = random(0, 10);
        _pus[i].type = (r < 5) ? PU_WIDE : (r < 8) ? PU_SLOW : PU_LIFE;
        _pus[i].active = true;
        return;
    }
}

// ─── AABB-circle brick collision ─────────────────────────────────────────────

bool ArkanoidGame::ballHitBrick(int col, int row) {
    if (!_bricks[row][col]) return false;
    const float bx = (float)(AK_BRK_X0 + col * (AK_BRK_W + AK_BRK_GX));
    const float by = (float)(AK_BRK_Y0 + row * (AK_BRK_H + AK_BRK_GY));
    const float cx = constrain(_ball.x, bx, bx + AK_BRK_W);
    const float cy = constrain(_ball.y, by, by + AK_BRK_H);
    const float dx = _ball.x - cx;
    const float dy = _ball.y - cy;
    if (dx*dx + dy*dy > AK_BALL_R * AK_BALL_R) return false;
    if (fabsf(dx) >= fabsf(dy)) _ball.vx = -_ball.vx;
    else                         _ball.vy = -_ball.vy;
    return true;
}

// ─── HUD ─────────────────────────────────────────────────────────────────────

void ArkanoidGame::drawHUD(Adafruit_SSD1306& d) const {
    d.setTextSize(1); d.setTextColor(SSD1306_WHITE);
    d.setCursor(0, 0); d.print("SC:"); d.print(_score);
    d.setCursor(54, 0); d.print("LV:"); d.print(_level);
    // Lives as small filled squares on the right
    for (int i = 0; i < _lives && i < 5; i++)
        d.fillRect(122 - i*6, 1, 5, 5, SSD1306_WHITE);
    d.drawFastHLine(0, AK_HUD_H, 128, SSD1306_WHITE);
}

// ─── update ──────────────────────────────────────────────────────────────────

bool ArkanoidGame::update(float dt, float gravX, bool btnPressed, Adafruit_SSD1306& d) {
    const bool btnEdge = btnPressed && !_lastBtn;
    _lastBtn = btnPressed;

    // Power-up timers
    if (_wideTimer > 0.f) {
        _wideTimer -= dt;
        if (_wideTimer <= 0.f) { _wideTimer = 0.f; _paddleW = (float)AK_PAD_W_NRM; }
    }
    if (_slowTimer > 0.f) {
        _slowTimer -= dt;
        if (_slowTimer <= 0.f) _slowTimer = 0.f;
    }

    // ── State machine ─────────────────────────────────────────────────────────
    switch (_state) {

        case LAUNCH: {
            _paddleX = constrain(_paddleX + gravX * AK_PAD_VEL * dt, 0.f, 128.f - _paddleW);
            _ball.x  = _paddleX + _paddleW * 0.5f;
            _ball.y  = AK_PAD_Y - AK_BALL_R - 1.f;
            if (btnEdge) {
                const float rel = (_ball.x - 64.f) / 64.f;
                const float spd = currentSpeed();
                _ball.vx = spd * sinf(rel * 0.7f);
                _ball.vy = -sqrtf(fmaxf(1.f, spd*spd - _ball.vx*_ball.vx));
                _state = PLAYING;
            }
            break;
        }

        case PLAYING: {
            // Move paddle
            _paddleX = constrain(_paddleX + gravX * AK_PAD_VEL * dt, 0.f, 128.f - _paddleW);

            // Move ball
            _ball.x += _ball.vx * dt;
            _ball.y += _ball.vy * dt;

            // Wall bounces
            if (_ball.x - AK_BALL_R <= 0.f)   { _ball.x = AK_BALL_R;          _ball.vx =  fabsf(_ball.vx); }
            if (_ball.x + AK_BALL_R >= 127.f)  { _ball.x = 127.f - AK_BALL_R; _ball.vx = -fabsf(_ball.vx); }
            if (_ball.y - AK_BALL_R <= (float)AK_HUD_H) {
                _ball.y = (float)AK_HUD_H + AK_BALL_R;
                _ball.vy = fabsf(_ball.vy);
            }

            // Ball-paddle bounce (angle depends on hit position)
            if (_ball.vy > 0.f &&
                _ball.y + AK_BALL_R >= AK_PAD_Y &&
                _ball.y - AK_BALL_R <= AK_PAD_Y + AK_PAD_H &&
                _ball.x >= _paddleX - 1.f &&
                _ball.x <= _paddleX + _paddleW + 1.f)
            {
                _ball.y = AK_PAD_Y - AK_BALL_R;
                const float rel   = (_ball.x - (_paddleX + _paddleW * 0.5f)) / (_paddleW * 0.5f);
                const float angle = rel * 1.1f;
                const float spd   = currentSpeed();
                _ball.vx = spd * sinf(angle);
                _ball.vy = -spd * cosf(fabsf(angle));
                if (fabsf(_ball.vy) < 10.f) _ball.vy = -10.f;  // prevent flat shots
            }

            // Ball-brick collisions (process one per frame to avoid tunnelling)
            bool hitAny = false;
            for (int r = 0; r < ROWS && !hitAny; r++) {
                for (int c = 0; c < COLS && !hitAny; c++) {
                    if (!_bricks[r][c]) continue;
                    if (ballHitBrick(c, r)) {
                        hitAny = true;
                        if (--_bricks[r][c] == 0) {
                            _score += (ROWS - r) * _level;
                            const float bx = (float)(AK_BRK_X0 + c*(AK_BRK_W+AK_BRK_GX)) + AK_BRK_W*0.5f;
                            const float by = (float)(AK_BRK_Y0 + r*(AK_BRK_H+AK_BRK_GY)) + AK_BRK_H*0.5f;
                            spawnPU(bx, by);
                        }
                    }
                }
            }

            // Check for level clear
            bool anyLeft = false;
            for (int r = 0; r < ROWS && !anyLeft; r++)
                for (int c = 0; c < COLS && !anyLeft; c++)
                    if (_bricks[r][c]) anyLeft = true;
            if (!anyLeft) { _state = LEVEL_DONE; _stateMs = millis(); }

            // Power-up movement and collection
            for (int i = 0; i < MAX_PU; i++) {
                if (!_pus[i].active) continue;
                _pus[i].y += AK_PU_SPD * dt;
                if (_pus[i].y > 70.f) { _pus[i].active = false; continue; }
                if (_pus[i].y + 3.f >= AK_PAD_Y &&
                    _pus[i].y - 3.f <= AK_PAD_Y + AK_PAD_H &&
                    _pus[i].x >= _paddleX - 4.f &&
                    _pus[i].x <= _paddleX + _paddleW + 4.f)
                {
                    _pus[i].active = false;
                    switch (_pus[i].type) {
                        case PU_WIDE: _paddleW = (float)AK_PAD_W_WIDE; _wideTimer = 6.f; break;
                        case PU_SLOW: _slowTimer = 5.f; break;
                        case PU_LIFE: if (_lives < 5) _lives++; break;
                    }
                }
            }

            // Ball lost
            if (_ball.y > 72.f) {
                for (int i = 0; i < MAX_PU; i++) _pus[i].active = false;
                if (--_lives <= 0) { _state = GAME_OVER; _stateMs = millis(); }
                else               { _paddleW = AK_PAD_W_NRM; _wideTimer = 0.f; resetBall(); _state = LAUNCH; }
            }
            break;
        }

        case LEVEL_DONE:
            if (millis() - _stateMs > 1500UL) {
                _level++;
                _ballSpd = fminf(_ballSpd * 1.10f, AK_MAX_SPD);
                _paddleW = (float)AK_PAD_W_NRM;
                _wideTimer = 0.f; _slowTimer = 0.f;
                for (int i = 0; i < MAX_PU; i++) _pus[i].active = false;
                resetBricks();
                resetBall();
                _state = LAUNCH;
            }
            break;

        case GAME_OVER:
            if (btnEdge || millis() - _stateMs > 4000UL) return false;
            break;
    }

    // ── Render ────────────────────────────────────────────────────────────────
    d.clearDisplay();
    drawHUD(d);

    // Bricks
    for (int r = 0; r < ROWS; r++) {
        for (int c = 0; c < COLS; c++) {
            if (!_bricks[r][c]) continue;
            const int16_t bx = (int16_t)(AK_BRK_X0 + c*(AK_BRK_W+AK_BRK_GX));
            const int16_t by = (int16_t)(AK_BRK_Y0 + r*(AK_BRK_H+AK_BRK_GY));
            if (_bricks[r][c] >= 2) d.fillRect(bx, by, AK_BRK_W, AK_BRK_H, SSD1306_WHITE);
            else                    d.drawRect(bx, by, AK_BRK_W, AK_BRK_H, SSD1306_WHITE);
        }
    }

    // Paddle
    d.fillRect((int16_t)_paddleX, (int16_t)AK_PAD_Y, (int16_t)_paddleW, AK_PAD_H, SSD1306_WHITE);

    // Ball
    d.fillRect((int16_t)(_ball.x - AK_BALL_R), (int16_t)(_ball.y - AK_BALL_R),
               (int16_t)(AK_BALL_R*2), (int16_t)(AK_BALL_R*2), SSD1306_WHITE);

    // Power-ups
    for (int i = 0; i < MAX_PU; i++) {
        if (!_pus[i].active) continue;
        const int16_t px = (int16_t)_pus[i].x;
        const int16_t py = (int16_t)_pus[i].y;
        d.drawRect(px - 4, py - 3, 9, 7, SSD1306_WHITE);
        switch (_pus[i].type) {
            case PU_WIDE: d.drawFastHLine(px - 3, py, 7, SSD1306_WHITE); break;
            case PU_SLOW: d.drawCircle(px, py, 2, SSD1306_WHITE);        break;
            case PU_LIFE:
                d.drawFastVLine(px, py - 2, 5, SSD1306_WHITE);
                d.drawFastHLine(px - 2, py, 5, SSD1306_WHITE);
                break;
        }
    }

    // Overlay messages
    d.setTextSize(1); d.setTextColor(SSD1306_WHITE);
    if (_state == LAUNCH) {
        d.setCursor(34, 44); d.print("BTN: lancia");
    } else if (_state == LEVEL_DONE) {
        d.setTextSize(2);
        d.setCursor(4,  18); d.print("LIVELLO");
        d.setCursor(4,  36); d.print("SUPERATO!");
    } else if (_state == GAME_OVER) {
        d.setTextSize(2);
        d.setCursor(14, 14); d.print("GAME OVER");
        d.setTextSize(1);
        d.setCursor(14, 36); d.print("SCORE:"); d.print(_score);
        d.setCursor(14, 46); d.print("LV:");    d.print(_level);
    }

    d.display();
    return true;
}
