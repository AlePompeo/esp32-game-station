// FlappyBall port for ESP32 + SSD1306
// Original game: Chris Martinez (2014), Scott Allen (2016-2017), BSD-3-Clause
// Adapted from Arduboy fixed-point physics to ESP32 float physics.

#include "FlappyGame.h"
#include <Arduino.h>
#include <string.h>

// ─── Init ────────────────────────────────────────────────────────────────────

void FlappyGame::begin() {
    _birdY       = 31.0f;
    _birdVY      = 0.0f;
    _prevBirdY   = 31.0f;
    _wingLen     = (int8_t)FL_BALL_R;
    _pipeGap     = (int8_t)FL_GAP_MAX;
    _gapReduce   = (int8_t)FL_GAP_REDUCE;
    _spawnTimer  = FL_SPAWN_SECS * 0.5f;   // first pipe spawns early so screen isn't empty
    _score       = 0;
    _pipesPassed = 0;
    _floatFrames = 0;
    _floatScore  = 0;
    _state       = READY;
    _deadMs      = 0;
    _lastBtn     = false;
    _lastShake   = false;

    for (int i = 0; i < MAX_PIPES; i++) _pipes[i].active = false;
    for (int i = 0; i < NUM_STARS; i++) {
        _starX[i] = (float)random(0, 128);
        _starY[i] = (float)random(4, FL_FLOOR_Y - 1);
    }
}

// ─── Pipe helpers ─────────────────────────────────────────────────────────────

void FlappyGame::spawnPipe() {
    for (int i = 0; i < MAX_PIPES; i++) {
        if (_pipes[i].active) continue;
        _pipes[i].x      = 128.0f;
        // gapY: top of the opening – same range as original
        // random(PIPE_MIN_H, FLOOR - PIPE_MIN_H - pipeGap + 1) → gapY ∈ [6, 63-6-gap]
        _pipes[i].gapY   = (int8_t)random(FL_PIPE_MIN_H,
                               FL_FLOOR_Y - FL_PIPE_MIN_H - _pipeGap + 1);
        _pipes[i].active = true;
        _pipes[i].scored = false;
        return;
    }
}

// ─── Collision ────────────────────────────────────────────────────────────────
// Direct port of FlappyBall checkPipe(): AABB tests for body + cap of each pipe.

bool FlappyGame::ballHitsPipe() const {
    const int AxA = FL_BALL_X - (FL_BALL_R - 1);
    const int AxB = FL_BALL_X + (FL_BALL_R - 1);
    const int AyA = (int)_birdY - (FL_BALL_R - 1);
    const int AyB = (int)_birdY + (FL_BALL_R - 1);

    for (int i = 0; i < MAX_PIPES; i++) {
        if (!_pipes[i].active) continue;
        const int px  = (int)_pipes[i].x;
        const int gy  = (int)_pipes[i].gapY;
        const int gh  = (int)_pipeGap;
        int BxA, BxB, ByA, ByB;

        // ── Top pipe body ──────────────────────────────────────────────────────
        BxA = px;            BxB = px + FL_PIPE_W;
        ByA = 0;             ByB = gy;
        if (AxA < BxB && AxB > BxA && AyA < ByB && AyB > ByA) return true;

        // ── Top pipe cap (wider lip at the opening) ────────────────────────────
        BxA = px - FL_CAP_W; BxB = px + FL_PIPE_W + FL_CAP_W;
        ByA = gy - FL_CAP_H; // ByB stays = gy
        if (AxA < BxB && AxB > BxA && AyA < ByB && AyB > ByA) return true;

        // ── Bottom pipe body ───────────────────────────────────────────────────
        BxA = px;            BxB = px + FL_PIPE_W;
        ByA = gy + gh;       ByB = FL_FLOOR_Y;
        if (AxA < BxB && AxB > BxA && AyA < ByB && AyB > ByA) return true;

        // ── Bottom pipe cap ────────────────────────────────────────────────────
        BxA = px - FL_CAP_W; BxB = px + FL_PIPE_W + FL_CAP_W;
        // ByA stays = gy + gh
        ByB = ByA + FL_CAP_H;
        if (AxA < BxB && AxB > BxA && AyA < ByB && AyB > ByA) return true;
    }
    return false;
}

// ─── Rendering ────────────────────────────────────────────────────────────────

void FlappyGame::drawStars(Adafruit_SSD1306& d) const {
    for (int i = 0; i < NUM_STARS; i++)
        d.drawPixel((int16_t)_starX[i], (int16_t)_starY[i], SSD1306_WHITE);
}

// Direct port of FlappyBall drawFloaty(): circle + animated wing + eye.
void FlappyGame::drawBall(Adafruit_SSD1306& d) const {
    const int bx = FL_BALL_X;
    const int by = (int)_birdY;
    d.drawCircle(bx, by, FL_BALL_R, SSD1306_WHITE);
    // Wing: line from center to (bx - BALL_R - 1, by - wingLen)
    d.drawLine(bx, by, bx - (FL_BALL_R + 1), by - _wingLen, SSD1306_WHITE);
    // Wing tip dot
    d.drawPixel(bx - (FL_BALL_R + 1), by - _wingLen + 1, SSD1306_WHITE);
    // Eye
    d.drawPixel(bx + 1, by - 2, SSD1306_WHITE);
}

// Direct port of FlappyBall drawPipes(): body + cap + detail line per pipe.
void FlappyGame::drawPipes(Adafruit_SSD1306& d) const {
    for (int i = 0; i < MAX_PIPES; i++) {
        if (!_pipes[i].active) continue;
        const int px  = (int)_pipes[i].x;
        const int gy  = (int)_pipes[i].gapY;
        const int gap = (int)_pipeGap;

        // ── Top pipe ──────────────────────────────────────────────────────────
        // Body: y=-1 .. gy-1 (height = gy)
        d.drawRect(px, -1, FL_PIPE_W, gy, SSD1306_WHITE);
        // Cap: wider lip pushed into body bottom (does not add length)
        d.drawRect(px - FL_CAP_W, gy - FL_CAP_H,
                   FL_PIPE_W + FL_CAP_W * 2, FL_CAP_H, SSD1306_WHITE);
        // Detail line
        d.drawLine(px + 2, 0, px + 2, gy - FL_CAP_H - 1, SSD1306_WHITE);

        // ── Bottom pipe ───────────────────────────────────────────────────────
        const int by2 = gy + gap;
        // Body: by2 .. FL_FLOOR_Y - 1 (height = FL_FLOOR_Y - by2)
        d.drawRect(px, by2, FL_PIPE_W, FL_FLOOR_Y - by2, SSD1306_WHITE);
        // Cap
        d.drawRect(px - FL_CAP_W, by2, FL_PIPE_W + FL_CAP_W * 2, FL_CAP_H, SSD1306_WHITE);
        // Detail line
        d.drawLine(px + 2, by2 + FL_CAP_H + 1, px + 2, FL_FLOOR_Y - 3, SSD1306_WHITE);
    }
}

// ─── Update ───────────────────────────────────────────────────────────────────

bool FlappyGame::update(float dt, bool btnPressed, bool shake, Adafruit_SSD1306& d) {
    const bool btnEdge   = btnPressed && !_lastBtn;
    const bool shakeEdge = shake     && !_lastShake;
    _lastBtn   = btnPressed;
    _lastShake = shake;
    const bool flap = btnEdge || shakeEdge;

    // Wing animation: wingLen decrements 4→3→2→1→0, then wraps back to 4
    _wingLen--;
    if (_wingLen < 0) _wingLen = (int8_t)FL_BALL_R;

    // Stars scroll at 15 % of pipe speed
    for (int i = 0; i < NUM_STARS; i++) {
        _starX[i] -= FL_PIPE_SPEED * 0.15f * dt;
        if (_starX[i] < 0.f) {
            _starX[i] = 128.f + (float)random(0, 8);
            _starY[i] = (float)random(4, FL_FLOOR_Y - 1);
        }
    }

    // ── READY ────────────────────────────────────────────────────────────────
    if (_state == READY) {
        if (flap) {
            _birdVY = FL_JUMP_VY;
            _state  = PLAYING;
        }
    }

    // ── PLAYING ──────────────────────────────────────────────────────────────
    else if (_state == PLAYING) {
        // Jump only when NOT actively rising (faithfully ported from FlappyBall:
        // "if ballYprev <= ballY" ≡ ball is falling or at peak).
        if (flap && _prevBirdY <= _birdY) {
            _birdVY = FL_JUMP_VY;
        }

        _prevBirdY = _birdY;
        _birdVY   += FL_GRAVITY * dt;
        _birdY    += _birdVY  * dt;

        // Ceiling: hit and start falling
        if (_birdY < (float)FL_BALL_R) {
            _birdY  = (float)FL_BALL_R;
            _birdVY = 0.f;
        }

        // Spawn pipes on timer
        _spawnTimer -= dt;
        if (_spawnTimer <= 0.f) {
            spawnPipe();
            _spawnTimer = FL_SPAWN_SECS;
        }

        // Move pipes, check scoring
        for (int i = 0; i < MAX_PIPES; i++) {
            if (!_pipes[i].active) continue;
            _pipes[i].x -= FL_PIPE_SPEED * dt;
            if (_pipes[i].x + FL_PIPE_W < 0.f) {
                _pipes[i].active = false;
                continue;
            }
            // Score when pipe right edge passes ball left edge
            if (!_pipes[i].scored &&
                    _pipes[i].x + FL_PIPE_W < (float)(FL_BALL_X - FL_BALL_R)) {
                _pipes[i].scored = true;
                _score++;
                _pipesPassed++;
                // Floating score popup: starts above the ball, drifts up+left
                _floatX      = (int16_t)FL_BALL_X;
                _floatY      = (int16_t)(_birdY) - FL_BALL_R - 8;
                _floatFrames = 15;
                _floatScore  = _score;
                // Progressive difficulty: gap shrinks by 1 every FL_GAP_REDUCE pipes
                if (--_gapReduce <= 0) {
                    if (_pipeGap > (int8_t)FL_GAP_MIN) _pipeGap--;
                    _gapReduce = (int8_t)FL_GAP_REDUCE;
                }
            }
        }

        // Ground hit
        if (_birdY + FL_BALL_R >= (float)FL_FLOOR_Y) {
            _birdY = (float)(FL_FLOOR_Y - FL_BALL_R);
            if (_score > _hiScore) _hiScore = _score;
            _state  = DEAD;
            _deadMs = millis();
        }
        // Pipe collision
        if (_state == PLAYING && ballHitsPipe()) {
            if (_score > _hiScore) _hiScore = _score;
            _state  = DEAD;
            _deadMs = millis();
        }
    }

    // ── DEAD ─────────────────────────────────────────────────────────────────
    else {
        // Ball falls to floor with gravity (like original's post-death animation)
        _prevBirdY = _birdY;
        _birdVY   += FL_GRAVITY * dt;
        _birdY    += _birdVY  * dt;
        if (_birdY + FL_BALL_R >= (float)FL_FLOOR_Y) {
            _birdY  = (float)(FL_FLOOR_Y - FL_BALL_R);
            _birdVY = 0.f;
        }
        if (flap || millis() - _deadMs > FL_DEAD_MS) {
            begin();
            return true;
        }
    }

    // ── Render ───────────────────────────────────────────────────────────────
    d.clearDisplay();
    d.setTextColor(SSD1306_WHITE);

    drawStars(d);
    drawPipes(d);
    d.drawFastHLine(0, FL_FLOOR_Y, 128, SSD1306_WHITE);
    drawBall(d);

    // Floating score popup: drifts up (-y) and left (-x) for 15 frames
    if (_floatFrames > 0) {
        if (_floatY >= 0) {
            d.setTextSize(1);
            d.setCursor(_floatX, _floatY);
            d.print(_floatScore);
        }
        _floatY--;
        _floatX -= 2;
        _floatFrames--;
    }

    // HUD: score left, hi-score right
    d.setTextSize(1);
    d.setCursor(0, 0);
    d.print("SC:"); d.print(_score);
    char hi[12];
    snprintf(hi, sizeof(hi), "HI:%d", _hiScore);
    d.setCursor(128 - (int16_t)(strlen(hi) * 6), 0);
    d.print(hi);

    if (_state == READY) {
        d.setCursor(32, 22); d.print("Premi BTN");
        d.setCursor(26, 32); d.print("o scuoti!");
    }

    if (_state == DEAD) {
        // Game-over box (ported from FlappyBall)
        d.drawRect(16, 8, 96, 48, SSD1306_WHITE);
        d.fillRect(17, 9, 94, 46, SSD1306_BLACK);
        d.setTextSize(2);
        // "GAME OVER" = 9 chars × 12 px = 108; center at (128-108)/2 = 10
        d.setCursor(10, 13); d.print("GAME OVER");
        d.setTextSize(1);
        d.setCursor(22, 34); d.print("Punti:"); d.print(_score);
        d.setCursor(22, 44); d.print("Best: "); d.print(_hiScore);
    }

    d.display();
    return true;
}
