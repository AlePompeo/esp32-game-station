#include "PinballGame.h"
#include <Arduino.h>
#include <math.h>

// ─── Field: logical 64×128 after setRotation(1) ──────────────────────────────
static constexpr float FTOP   = 9.0f;
static constexpr float FBOT   = 127.0f;
static constexpr float FLEFT  = 0.0f;
static constexpr float FRIGHT = 63.0f;

// ─── Physics ──────────────────────────────────────────────────────────────────
static constexpr float GRAVITY = 200.0f;
static constexpr float MAX_SPD = 620.0f;
static constexpr float DAMPING = 0.9995f;

// ─── Flippers ─────────────────────────────────────────────────────────────────
static constexpr float PI_F      = 3.14159265f;
static constexpr float D2R       = PI_F / 180.0f;
static constexpr float LPX       = 14.0f, LPY = 117.0f;
static constexpr float RPX       = 50.0f, RPY = 117.0f;
static constexpr float FL        = 17.0f;
static constexpr float L_REST    =  27.0f * D2R;
static constexpr float L_ACTIVE  = -27.0f * D2R;
static constexpr float R_REST    = (180.0f - 27.0f) * D2R;
static constexpr float R_ACTIVE  = (180.0f + 27.0f) * D2R;
static constexpr float FLIP_SPD  = 950.0f * D2R;

// ─── Bumpers: diamond formation (top-left, top-right, mid-left, mid-right) ────
static constexpr int  NB       = 4;
static const float    BUMP_X[] = { 18.0f, 46.0f, 24.0f, 40.0f };
static const float    BUMP_Y[] = { 30.0f, 30.0f, 42.0f, 42.0f };
static constexpr float BUMP_R  = 5.0f;
static constexpr int   BUMP_PTS   = 100;
static constexpr int   JACK_PTS   = 5000;  // jackpot bumper score
static constexpr int   JACK_CLEARS = 2;    // target clears to unlock jackpot

// ─── Guide rails (funnel ball from sides into bumper area) ────────────────────
struct Seg { float x1, y1, x2, y2; };
static const Seg GUIDE[2] = {
    {  0.0f, 57.0f,  9.0f, 23.0f },
    { 63.0f, 57.0f, 54.0f, 23.0f },
};

// ─── Top arc: horizontal bar connecting guide tops, forms the loop chamber ────
// Physics: only bounce ball downward when it is ALREADY inside (y < ARC_Y).
static constexpr float ARC_X1 = 9.0f, ARC_X2 = 54.0f, ARC_Y = 23.0f;

// ─── Slingshots ───────────────────────────────────────────────────────────────
static const Seg SLING[2] = {
    {  1.0f, 90.0f, 12.0f, 74.0f },
    { 62.0f, 90.0f, 51.0f, 74.0f },
};
static constexpr float SLING_KICK = 130.0f;
static constexpr int   SLING_PTS  = 75;

// ─── Inlane/outlane separators ────────────────────────────────────────────────
static const Seg INLANE[2] = {
    { 10.0f, 91.0f, 10.0f, 101.0f },
    { 53.0f, 91.0f, 53.0f, 101.0f },
};

// ─── Gutter walls ─────────────────────────────────────────────────────────────
static const Seg GUTTER[2] = {
    { FLEFT,  100.0f, LPX, LPY },
    { FRIGHT, 100.0f, RPX, RPY },
};

// ─── Drop targets ─────────────────────────────────────────────────────────────
static constexpr int  NT        = 3;
static const float    TARG_CX[] = { 13.0f, 31.0f, 49.0f };
static constexpr float TARG_Y   = 65.0f;
static constexpr float TARG_HW  = 4.0f;
static constexpr float TARG_HH  = 2.0f;
static constexpr int   TARG_PTS = 200;
static constexpr int   TARG_BONUS = 750;

// ─── Spinner ──────────────────────────────────────────────────────────────────
static constexpr float SPIN_CX        = 32.0f;
static constexpr float SPIN_Y         = 55.0f;
static constexpr float SPIN_HW        = 7.0f;
static constexpr int   SPIN_TICKS     = 6;
static constexpr int   SPIN_TICK_FRAMES = 3;
static constexpr int   SPIN_PTS       = 50;

// ─── Side wall kickers (mini bumpers) ────────────────────────────────────────
static constexpr int  NK       = 2;
static const float    KICK_X[] = { 4.0f, 59.0f };
static constexpr float KICK_Y  = 68.0f;
static constexpr float KICK_R  = 3.0f;
static constexpr int   KICK_PTS = 75;

// ─── Rollover lanes ───────────────────────────────────────────────────────────
static constexpr int  NL        = 3;
static const float    LANE_X1[] = {  3.0f, 23.0f, 45.0f };
static const float    LANE_X2[] = { 18.0f, 40.0f, 60.0f };
static constexpr float LANE_Y   = 15.0f;
static constexpr float LANE_DY  = 3.5f;
static constexpr int   LANE_PTS = 150;
static constexpr int   LANE_BONUS = 1000;

// ─── Center post ──────────────────────────────────────────────────────────────
static constexpr float POST_X = 32.0f;
static constexpr float POST_Y = 113.0f;
static constexpr float POST_R = 2.0f;
static constexpr int   POST_PTS = 50;

// ─── Spiral loop ──────────────────────────────────────────────────────────────
// Sits in the dead zone between the two slingshots (x 8-55, y 74-90).
// A radius-7 circle fits with ~1 px clearance on each side.
static constexpr float LOOP_CX      = 32.0f;
static constexpr float LOOP_CY      = 80.0f;
static constexpr float LOOP_R       = 7.0f;
static constexpr float LOOP_ANG_SPD = 3.0f * 2.0f * PI_F;  // 3 turns/sec
static constexpr int   LOOP_PTS     = 2000;

// ─── Timings ──────────────────────────────────────────────────────────────────
static constexpr uint32_t BALL_SAVE_MS = 2000;
static constexpr uint32_t TILT_MS      = 1500;
static constexpr float    KB_KICK      = 290.0f;

// ─── Geometry helpers ─────────────────────────────────────────────────────────

static float closestOnSeg(float px, float py,
                           float ax, float ay, float bx, float by,
                           float& cx, float& cy) {
    float dx = bx-ax, dy = by-ay, len2 = dx*dx+dy*dy;
    if (len2 < 1e-9f) { cx = ax; cy = ay; }
    else {
        float t = ((px-ax)*dx + (py-ay)*dy) / len2;
        if (t < 0.f) t = 0.f; if (t > 1.f) t = 1.f;
        cx = ax + t*dx; cy = ay + t*dy;
    }
    float ex = px-cx, ey = py-cy;
    return sqrtf(ex*ex + ey*ey);
}

static bool bounceSegment(float& px, float& py, float& vx, float& vy,
                           float ax, float ay, float ex, float ey,
                           float r, float restitution, float kick = 0.f) {
    float cx, cy;
    float dist = closestOnSeg(px, py, ax, ay, ex, ey, cx, cy);
    if (dist >= r) return false;
    float nx = px-cx, ny = py-cy;
    float nl = sqrtf(nx*nx + ny*ny);
    if (nl < 0.001f) { nx = 0.f; ny = -1.f; } else { nx /= nl; ny /= nl; }
    px += nx * (r - dist);
    py += ny * (r - dist);
    float dot = vx*nx + vy*ny;
    if (dot < 0.f) {
        vx -= (1.f + restitution) * dot * nx;
        vy -= (1.f + restitution) * dot * ny;
    }
    if (kick > 0.f) { vx += nx*kick; vy += ny*kick; }
    return true;
}

// ─── PinballGame public ───────────────────────────────────────────────────────

void PinballGame::begin() {
    _prevHiScore = _hiScore;
    if (_score > _hiScore) _hiScore = _score;
    _score          = 0;
    _ballScore      = 0;
    _lives          = LIVES_MAX;
    _mult           = 1;
    _multFlash      = 0;
    _bonusFlash     = 0;
    _state          = READY;
    _stateMs        = millis();
    _lastShake      = false;
    _leftAngle      = L_REST;
    _rightAngle     = R_REST;
    _postFlash      = 0;
    _ballSave       = false;
    _skillShotAvail = false;
    _skillFlash     = 0;
    _kickbackAvail  = true;
    _kickbackFlash  = 0;
    _spinTicks      = 0;
    _spinState      = 0;
    _spinTimer      = 0;
    _tilted         = false;
    _targetClears   = 0;
    _jackpotReady   = false;
    _jackpotBumper  = 0;
    _jackpotFlash   = 0;
    _loopMode       = false;
    _loopEntry      = 0.0f;
    _loopAngle      = 0.0f;
    _loopFlash      = 0;
    for (int i = 0; i < NB; i++) { _bumperFlash[i] = 0; _bumperHit[i] = false; }
    for (int i = 0; i < NK; i++) _kickerFlash[i] = 0;
    for (int i = 0; i < 2;  i++) _slingFlash[i]  = 0;
    for (int i = 0; i < TRAIL_LEN; i++) { _trailX[i] = -999.f; _trailY[i] = -999.f; }
    _trailHead = 0;
    resetBall();
    resetTargets();
    resetLanes();
}

void PinballGame::resetBall() {
    _bx  = 32.0f; _by  = 96.0f;
    _bvx = 0.0f;  _bvy = 0.0f;
    _nudgesLeft     = NUDGES_MAX;
    _kickbackAvail  = true;
    _skillShotAvail = false;
    _ballSave       = false;
    _spinTicks      = 0;
    _tilted         = false;
    _ballScore      = 0;
    _loopMode       = false;
    _loopFlash      = 0;
    for (int i = 0; i < TRAIL_LEN; i++) { _trailX[i] = -999.f; _trailY[i] = -999.f; }
    _trailHead = 0;
}

void PinballGame::resetTargets() {
    for (int i = 0; i < NT; i++) { _targetUp[i] = true; _targetFlash[i] = 0; }
}

void PinballGame::resetLanes() {
    for (int i = 0; i < NL; i++) { _laneHit[i] = false; _laneFlash[i] = 0; }
}

void PinballGame::awardPoints(int base) {
    int pts    = base * _mult;
    _score    += pts;
    _ballScore += pts;
}

void PinballGame::checkMultiplier() {
    for (int i = 0; i < NB; i++) if (!_bumperHit[i]) return;
    for (int i = 0; i < NB; i++) _bumperHit[i] = false;
    if (_mult < MULT_MAX) { _mult++; _multFlash = 50; }
}

// ─── Update ───────────────────────────────────────────────────────────────────

bool PinballGame::update(float dt, bool btnHeld, bool btnPressed,
                          bool shake, float gravX, float gravY,
                          Adafruit_SSD1306& d) {
    const bool shakeEdge = shake && !_lastShake;
    _lastShake = shake;

    const float tilt   = fabsf(gravY) > fabsf(gravX) ? gravY : gravX;
    const bool leftUp  = btnHeld || (tilt < -1.5f);
    const bool rightUp = btnHeld || (tilt >  1.5f);

    // Decay flash timers
    for (int i = 0; i < NB; i++) if (_bumperFlash[i] > 0) _bumperFlash[i]--;
    for (int i = 0; i < NT; i++) if (_targetFlash[i] > 0) _targetFlash[i]--;
    for (int i = 0; i < NL; i++) if (_laneFlash[i]   > 0) _laneFlash[i]--;
    for (int i = 0; i < NK; i++) if (_kickerFlash[i]  > 0) _kickerFlash[i]--;
    for (int i = 0; i < 2;  i++) if (_slingFlash[i]  > 0) _slingFlash[i]--;
    if (_multFlash     > 0) _multFlash--;
    if (_bonusFlash    > 0) _bonusFlash--;
    if (_postFlash     > 0) _postFlash--;
    if (_kickbackFlash > 0) _kickbackFlash--;
    if (_skillFlash    > 0) _skillFlash--;
    if (_jackpotFlash  > 0) _jackpotFlash--;
    if (_loopFlash     > 0) _loopFlash--;

    // Expiry timers
    if (_ballSave && (millis() - _ballSaveMs) > BALL_SAVE_MS)
        _ballSave = false;
    if (_tilted && (millis() - _tiltMs) > TILT_MS)
        _tilted = false;

    // Spinner tick scoring (frame-rate driven)
    if (_spinTicks > 0) {
        if (_spinTimer <= 0) {
            awardPoints(SPIN_PTS);
            _spinTicks--;
            _spinTimer = SPIN_TICK_FRAMES;
            _spinState ^= 1;
        } else {
            _spinTimer--;
        }
    }

    // Flipper control (disabled when tilted)
    const bool flipL = leftUp  && !_tilted;
    const bool flipR = rightUp && !_tilted;
    auto moveAngle = [](float& angle, float target, float step) {
        float delta = target - angle;
        if (fabsf(delta) <= step) angle = target;
        else angle += (delta > 0.f ? step : -step);
    };
    moveAngle(_leftAngle,  flipL ? L_ACTIVE : L_REST, FLIP_SPD * dt);
    moveAngle(_rightAngle, flipR ? R_ACTIVE : R_REST, FLIP_SPD * dt);

    switch (_state) {

        case READY:
            if (btnPressed || shakeEdge) {
                _bvx = (float)(random(-60, 61));
                _bvy = -340.0f;
                _state          = PLAYING;
                _ballSave       = true;
                _ballSaveMs     = millis();
                _skillShotAvail = true;
                _ballScore      = 0;
            }
            break;

        case PLAYING:
            if (shakeEdge && !_loopMode) {
                if (_nudgesLeft > 0) {
                    _bvx += (float)(random(-60, 61));
                    _bvy -= 50.0f;
                    _nudgesLeft--;
                } else if (!_tilted) {
                    _tilted = true;
                    _tiltMs = millis();
                }
            }

            if (_loopMode) {
                // Scripted orbit: advance angle clockwise, position ball on ring.
                _loopAngle += LOOP_ANG_SPD * dt;
                if (_loopAngle - _loopEntry >= 3.0f * PI_F) {
                    // 1.5 turns complete: eject upward from the top of the ring
                    _bx       = LOOP_CX;
                    _by       = LOOP_CY - LOOP_R;
                    _bvx      = (float)(random(-50, 51));
                    _bvy      = -360.0f;
                    _loopMode = false;
                    _loopFlash = 55;
                    awardPoints(LOOP_PTS);
                } else {
                    _bx = LOOP_CX + LOOP_R * cosf(_loopAngle);
                    _by = LOOP_CY + LOOP_R * sinf(_loopAngle);
                }
            } else {
                // Check loop entry: ball crosses into the loop ring area
                float ldx = _bx - LOOP_CX, ldy = _by - LOOP_CY;
                if (ldx*ldx + ldy*ldy < (LOOP_R + BALL_R) * (LOOP_R + BALL_R)
                        && _loopFlash == 0 && !_tilted) {
                    _loopMode  = true;
                    _loopEntry = atan2f(ldy, ldx);
                    _loopAngle = _loopEntry;
                } else {
                    stepPhysics(dt, flipL, flipR);
                }
            }

            // Update trail
            _trailHead = (_trailHead + 1) % TRAIL_LEN;
            _trailX[_trailHead] = _bx;
            _trailY[_trailHead] = _by;
            if (!_loopMode && _by > FBOT + BALL_R) {
                if (_ballSave) {
                    _by  = FBOT - BALL_R;
                    _bvy = -fabsf(_bvy) * 0.7f - 80.0f;
                } else {
                    _lives--;
                    _state   = (_lives <= 0) ? GAME_OVER : BALL_LOST;
                    _stateMs = millis();
                }
            }
            break;

        case BALL_LOST:
            if (millis() - _stateMs > 2500UL) {
                resetBall();
                _state = READY;
            }
            break;

        case GAME_OVER:
            if (millis() - _stateMs > 3000UL || shakeEdge || btnPressed)
                return false;
            break;
    }

    render(d);
    return true;
}

// ─── Physics step ─────────────────────────────────────────────────────────────

void PinballGame::stepPhysics(float dt, bool leftUp, bool rightUp) {
    // Sub-step loop: 4 iterations with dt/4 each.
    // At MAX_SPD=620 and 30fps, 1 step = 20.7 px. With 4 substeps = 5.2 px each,
    // smaller than BALL_R*2=6 → tunneling through any element is physically impossible.
    static constexpr int SUBSTEPS = 4;
    const float sdt = dt / SUBSTEPS;
    for (int _sub = 0; _sub < SUBSTEPS; _sub++) {

    _bvy += GRAVITY * sdt;
    _bvx *= DAMPING;
    _bvy *= DAMPING;
    _bx  += _bvx * sdt;
    _by  += _bvy * sdt;

    float spd = sqrtf(_bvx*_bvx + _bvy*_bvy);
    if (spd > MAX_SPD) { float s = MAX_SPD/spd; _bvx *= s; _bvy *= s; }

    // ── Walls ─────────────────────────────────────────────────────────────────
    if (_bx < FLEFT  + BALL_R) { _bx = FLEFT  + BALL_R; _bvx =  fabsf(_bvx) * 0.70f; }
    if (_bx > FRIGHT - BALL_R) { _bx = FRIGHT - BALL_R; _bvx = -fabsf(_bvx) * 0.70f; }
    if (_by < FTOP   + BALL_R) {
        _by  = FTOP + BALL_R;
        _bvy = fabsf(_bvy) * 0.65f;
        _bvx += (float)(random(-25, 26));  // lateral spread to escape loop naturally
    }

    // ── Top arc ───────────────────────────────────────────────────────────────
    // BUG-FIX: guard _by < ARC_Y so balls from below pass through freely;
    // only balls already inside the chamber (y < ARC_Y) are bounced downward.
    if (_by < ARC_Y && _bx >= ARC_X1 && _bx <= ARC_X2)
        bounceSegment(_bx, _by, _bvx, _bvy, ARC_X1, ARC_Y, ARC_X2, ARC_Y, BALL_R, 0.55f);

    // ── Rollover lanes ────────────────────────────────────────────────────────
    if (_by >= LANE_Y - LANE_DY && _by <= LANE_Y + LANE_DY) {
        for (int i = 0; i < NL; i++) {
            if (!_laneHit[i] && _bx >= LANE_X1[i] && _bx <= LANE_X2[i]) {
                _laneHit[i]   = true;
                _laneFlash[i] = 18;
                awardPoints(LANE_PTS);
                bool allLit = true;
                for (int j = 0; j < NL; j++) if (!_laneHit[j]) { allLit = false; break; }
                if (allLit) {
                    awardPoints(LANE_BONUS);
                    _bonusFlash = 50;
                    resetLanes();
                    if (_mult < MULT_MAX) { _mult++; _multFlash = 40; }
                }
            }
        }
    }

    // ── Bumpers ───────────────────────────────────────────────────────────────
    for (int i = 0; i < NB; i++) {
        float dx = _bx - BUMP_X[i], dy = _by - BUMP_Y[i];
        float dist = sqrtf(dx*dx + dy*dy);
        float minD = BUMP_R + BALL_R;
        if (dist < minD) {
            if (dist < 0.1f) dist = 0.1f;
            float nx = dx/dist, ny = dy/dist;
            _bx += nx * (minD - dist);
            _by += ny * (minD - dist);
            float dot = _bvx*nx + _bvy*ny;
            _bvx = (_bvx - 2.f*dot*nx) * 1.15f;
            _bvy = (_bvy - 2.f*dot*ny) * 1.15f;
            float outSpd = _bvx*nx + _bvy*ny;
            if (outSpd < 90.f) { _bvx += nx*(90.f-outSpd); _bvy += ny*(90.f-outSpd); }

            // Jackpot bumper hit
            if (_jackpotReady && i == _jackpotBumper) {
                awardPoints(JACK_PTS);
                _jackpotReady = false;
                _jackpotFlash = 80;
                _bonusFlash   = 80;
                _targetClears = 0;
            }
            // Skill shot
            else if (_skillShotAvail) {
                awardPoints(500);
                _skillShotAvail = false;
                _skillFlash     = 60;
            }
            else {
                awardPoints(BUMP_PTS);
            }
            if (!_bumperHit[i]) _bumperHit[i] = true;
            _bumperFlash[i] = 10;
            checkMultiplier();
        }
    }

    // ── Spinner ───────────────────────────────────────────────────────────────
    // Always check collision (was gated on _spinTicks==0, causing pass-through).
    // Only start the spin animation when the spinner was stationary.
    if (bounceSegment(_bx, _by, _bvx, _bvy,
                      SPIN_CX - SPIN_HW, SPIN_Y,
                      SPIN_CX + SPIN_HW, SPIN_Y,
                      BALL_R, 0.20f) && _spinTicks == 0) {
        _spinTicks = SPIN_TICKS;
        _spinTimer = 0;
        _spinState = 0;
    }

    // ── Side wall kickers ─────────────────────────────────────────────────────
    for (int i = 0; i < NK; i++) {
        float dx = _bx - KICK_X[i], dy = _by - KICK_Y;
        float dist = sqrtf(dx*dx + dy*dy);
        float minD = KICK_R + BALL_R;
        if (dist < minD) {
            if (dist < 0.1f) dist = 0.1f;
            float nx = dx/dist, ny = dy/dist;
            _bx += nx * (minD - dist);
            _by += ny * (minD - dist);
            float dot = _bvx*nx + _bvy*ny;
            _bvx = (_bvx - 2.f*dot*nx) * 1.10f;
            _bvy = (_bvy - 2.f*dot*ny) * 1.10f;
            float outSpd = _bvx*nx + _bvy*ny;
            if (outSpd < 60.f) { _bvx += nx*(60.f-outSpd); _bvy += ny*(60.f-outSpd); }
            awardPoints(KICK_PTS);
            _kickerFlash[i] = 8;
        }
    }

    // ── Center post ───────────────────────────────────────────────────────────
    {
        float dx = _bx - POST_X, dy = _by - POST_Y;
        float dist = sqrtf(dx*dx + dy*dy);
        float minD = POST_R + BALL_R;
        if (dist < minD) {
            if (dist < 0.1f) dist = 0.1f;
            float nx = dx/dist, ny = dy/dist;
            _bx += nx * (minD - dist);
            _by += ny * (minD - dist);
            float dot = _bvx*nx + _bvy*ny;
            if (dot < 0.f) {
                _bvx -= 2.f * dot * nx * 0.80f;
                _bvy -= 2.f * dot * ny * 0.80f;
            }
            _postFlash = 8;
            awardPoints(POST_PTS);
        }
    }

    // ── Drop targets ─────────────────────────────────────────────────────────
    for (int i = 0; i < NT; i++) {
        if (!_targetUp[i]) continue;
        float tx1 = TARG_CX[i] - TARG_HW, ty1 = TARG_Y - TARG_HH;
        float tx2 = TARG_CX[i] + TARG_HW, ty2 = TARG_Y + TARG_HH;
        float cx = _bx < tx1 ? tx1 : (_bx > tx2 ? tx2 : _bx);
        float cy = _by < ty1 ? ty1 : (_by > ty2 ? ty2 : _by);
        float ex = _bx - cx, ey = _by - cy;
        if (sqrtf(ex*ex + ey*ey) < BALL_R) {
            _targetUp[i]    = false;
            _targetFlash[i] = 12;
            awardPoints(TARG_PTS);
            if (_bvy > 0.f) _bvy = -_bvy * 0.8f;
            bool allGone = true;
            for (int j = 0; j < NT; j++) if (_targetUp[j]) { allGone = false; break; }
            if (allGone) {
                awardPoints(TARG_BONUS);
                _bonusFlash = 40;
                resetTargets();
                _targetClears++;
                // Unlock jackpot after JACK_CLEARS full clears
                if (_targetClears >= JACK_CLEARS && !_jackpotReady) {
                    _jackpotReady  = true;
                    _jackpotBumper = (int)(random(0, NB));
                    _jackpotFlash  = 0;
                }
            }
        }
    }

    // ── Slingshots ────────────────────────────────────────────────────────────
    for (int i = 0; i < 2; i++) {
        if (bounceSegment(_bx, _by, _bvx, _bvy,
                          SLING[i].x1, SLING[i].y1, SLING[i].x2, SLING[i].y2,
                          BALL_R, 0.85f, SLING_KICK)) {
            awardPoints(SLING_PTS);
            _slingFlash[i] = 10;
        }
    }

    // ── Upper guide rails ─────────────────────────────────────────────────────
    for (int i = 0; i < 2; i++)
        bounceSegment(_bx, _by, _bvx, _bvy,
                      GUIDE[i].x1, GUIDE[i].y1, GUIDE[i].x2, GUIDE[i].y2,
                      BALL_R, 0.60f);

    // ── Inlane/outlane separators ─────────────────────────────────────────────
    for (int i = 0; i < 2; i++)
        bounceSegment(_bx, _by, _bvx, _bvy,
                      INLANE[i].x1, INLANE[i].y1, INLANE[i].x2, INLANE[i].y2,
                      BALL_R, 0.50f);

    // ── Kickback (one-shot left outlane save) ─────────────────────────────────
    if (_kickbackAvail && _bx < 4.0f && _by > 91.0f && _by < 102.0f && _bvx <= 10.0f) {
        _bvx           = KB_KICK;
        _bvy          -= 100.0f;
        _kickbackAvail = false;
        _kickbackFlash = 25;
    }

    // ── Gutter walls ──────────────────────────────────────────────────────────
    bounceSegment(_bx, _by, _bvx, _bvy,
                  GUTTER[0].x1, GUTTER[0].y1, GUTTER[0].x2, GUTTER[0].y2, BALL_R, 0.45f);
    bounceSegment(_bx, _by, _bvx, _bvy,
                  GUTTER[1].x1, GUTTER[1].y1, GUTTER[1].x2, GUTTER[1].y2, BALL_R, 0.45f);

    // ── Flippers ──────────────────────────────────────────────────────────────
    float lTX = LPX + FL * cosf(_leftAngle),  lTY = LPY + FL * sinf(_leftAngle);
    float rTX = RPX + FL * cosf(_rightAngle), rTY = RPY + FL * sinf(_rightAngle);
    bounceSegment(_bx, _by, _bvx, _bvy, LPX, LPY, lTX, lTY,
                  BALL_R + 1.f, 0.65f, leftUp ? 200.f : 0.f);
    bounceSegment(_bx, _by, _bvx, _bvy, RPX, RPY, rTX, rTY,
                  BALL_R + 1.f, 0.65f, rightUp ? 200.f : 0.f);

    } // end substep loop
}

// ─── Render ───────────────────────────────────────────────────────────────────

// Draw a 4-pixel diamond decoration
static void drawDiamond(Adafruit_SSD1306& d, int x, int y) {
    d.drawPixel(x,   y-2, SSD1306_WHITE);
    d.drawPixel(x-1, y,   SSD1306_WHITE);
    d.drawPixel(x+1, y,   SSD1306_WHITE);
    d.drawPixel(x,   y+2, SSD1306_WHITE);
}

void PinballGame::render(Adafruit_SSD1306& d) {
    d.setRotation(1);
    d.clearDisplay();
    d.setTextColor(SSD1306_WHITE);

    // ── Score bar ─────────────────────────────────────────────────────────────
    d.setTextSize(1);
    d.setCursor(0, 0);
    if (_score > 99999) d.print(_score / 1000); else d.print(_score);
    d.setCursor(28, 0); d.print('x'); d.print(_mult);
    // Ball save blinks "SAV", otherwise shows B:N
    int ballNum = LIVES_MAX - _lives + 1;
    if (ballNum < 1) ballNum = 1;
    if (ballNum > LIVES_MAX) ballNum = LIVES_MAX;
    if (_ballSave && (millis() / 200) % 2 == 0) {
        d.setCursor(43, 0); d.print("SAV");
    } else {
        d.setCursor(43, 0); d.print('B'); d.print(':'); d.print(ballNum);
    }
    d.drawFastHLine(0, 8, 64, SSD1306_WHITE);

    // ── Game Over ─────────────────────────────────────────────────────────────
    if (_state == GAME_OVER) {
        d.setTextSize(1);
        d.setCursor(4, 20);  d.print("GAME  OVER");
        d.drawFastHLine(0, 30, 64, SSD1306_WHITE);
        d.setCursor(0, 36);  d.print("SC:"); d.print(_score);
        d.setCursor(0, 48);  d.print("HI:"); d.print(_hiScore);
        if (_score > _prevHiScore) {
            d.setCursor(4, 60); d.print("NUOVO RECORD!");
        }
        d.drawFastHLine(0, 70, 64, SSD1306_WHITE);
        d.setCursor(4, 76);  d.print("BTN o scuoti");
        d.setCursor(4, 86);  d.print("per ricominc.");
        d.display();
        d.setRotation(0);
        return;
    }

    // ── Ball trail (draw oldest→newest so ball renders on top) ────────────────
    if (_state == PLAYING) {
        for (int age = TRAIL_LEN - 1; age >= 1; age--) {
            int idx = (_trailHead - age + TRAIL_LEN) % TRAIL_LEN;
            float tx = _trailX[idx], ty = _trailY[idx];
            if (tx < -900.f) continue;
            if (ty > 105.f) continue;   // hide trail near flippers — avoids ghost impression
            float ddx = tx - _bx, ddy = ty - _by;
            if (ddx*ddx + ddy*ddy < 16.f) continue;  // skip if within 4px of current ball
            if (age >= TRAIL_LEN - 2)
                d.drawPixel((int)tx, (int)ty, SSD1306_WHITE);
            else
                d.drawCircle((int)tx, (int)ty, 1, SSD1306_WHITE);
        }
    }

    // ── Top arc ───────────────────────────────────────────────────────────────
    d.drawFastHLine((int)ARC_X1, (int)ARC_Y, (int)(ARC_X2 - ARC_X1 + 1), SSD1306_WHITE);

    // ── Rollover lanes ────────────────────────────────────────────────────────
    for (int i = 0; i < NL; i++) {
        int x1 = (int)LANE_X1[i], x2 = (int)LANE_X2[i];
        int y  = (int)LANE_Y, w = x2 - x1 + 1;
        if (_laneFlash[i] > 0 && (_laneFlash[i] % 5) < 3)
            d.fillRect(x1, y-1, w, 3, SSD1306_WHITE);
        else if (_laneHit[i])
            d.fillRect(x1, y-1, w, 3, SSD1306_WHITE);
        else
            d.drawFastHLine(x1, y, w, SSD1306_WHITE);
    }

    // ── Guide rails ───────────────────────────────────────────────────────────
    for (int i = 0; i < 2; i++)
        d.drawLine((int)GUIDE[i].x1, (int)GUIDE[i].y1,
                   (int)GUIDE[i].x2, (int)GUIDE[i].y2, SSD1306_WHITE);

    // ── Bumpers ───────────────────────────────────────────────────────────────
    for (int i = 0; i < NB; i++) {
        int bx = (int)BUMP_X[i], by = (int)BUMP_Y[i], br = (int)BUMP_R;
        bool isJack = _jackpotReady && (i == _jackpotBumper);
        if (_bumperFlash[i] > 0) {
            d.fillCircle(bx, by, br, SSD1306_WHITE);
        } else if (isJack) {
            // Jackpot bumper: double ring + blinking X
            d.drawCircle(bx, by, br,   SSD1306_WHITE);
            d.drawCircle(bx, by, br+2, SSD1306_WHITE);
            if ((millis() / 200) % 2) {
                d.drawLine(bx-2, by-2, bx+2, by+2, SSD1306_WHITE);
                d.drawLine(bx+2, by-2, bx-2, by+2, SSD1306_WHITE);
            }
        } else {
            d.drawCircle(bx, by, br, SSD1306_WHITE);
            if (_bumperHit[i])
                d.fillCircle(bx, by, 1, SSD1306_WHITE);  // hit-progress dot
        }
    }

    // ── Jackpot flash overlay ─────────────────────────────────────────────────
    if (_jackpotFlash > 0 && (_jackpotFlash % 6) < 3) {
        d.setTextSize(1);
        d.setCursor(2, 35);
        d.print("JACKPOT!");
    }

    // ── Spinner ───────────────────────────────────────────────────────────────
    {
        int cx = (int)SPIN_CX, sy = (int)SPIN_Y, hw = (int)SPIN_HW;
        if (_spinTicks > 0 && _spinState == 1)
            d.drawLine(cx - hw, sy - 3, cx + hw, sy + 3, SSD1306_WHITE);
        else {
            d.drawLine(cx - hw, sy, cx + hw, sy, SSD1306_WHITE);
            d.drawPixel(cx - hw, sy - 1, SSD1306_WHITE);
            d.drawPixel(cx + hw, sy - 1, SSD1306_WHITE);
        }
    }
    // "SPIN" overlay shown at a position that does NOT overlap the spinner bar
    if (_spinTicks > 0) {
        d.setTextSize(1);
        d.setCursor(17, 47);
        d.print("SPIN!");
    }

    // ── Drop targets (with thin separators between them) ──────────────────────
    {
        int ty1 = (int)(TARG_Y - TARG_HH), th = (int)(TARG_HH * 2);
        // Separators between targets
        d.drawFastVLine(21, ty1, th + 1, SSD1306_WHITE);
        d.drawFastVLine(40, ty1, th + 1, SSD1306_WHITE);
        for (int i = 0; i < NT; i++) {
            if (_targetUp[i]) {
                int tx = (int)(TARG_CX[i] - TARG_HW), tw = (int)(TARG_HW * 2);
                d.fillRect(tx, ty1, tw, th, SSD1306_WHITE);
                // Small black marker in the center of each target
                d.fillRect((int)TARG_CX[i] - 1, (int)TARG_Y - 1, 3, 2, SSD1306_BLACK);
            } else if (_targetFlash[i] > 0) {
                d.drawRect((int)(TARG_CX[i] - TARG_HW), (int)(TARG_Y - TARG_HH),
                           (int)(TARG_HW * 2), (int)(TARG_HH * 2), SSD1306_WHITE);
            }
        }
    }

    // ── Side wall kickers ─────────────────────────────────────────────────────
    for (int i = 0; i < NK; i++) {
        int kx = (int)KICK_X[i], ky = (int)KICK_Y, kr = (int)KICK_R;
        if (_kickerFlash[i] > 0) d.fillCircle(kx, ky, kr, SSD1306_WHITE);
        else                     d.drawCircle(kx, ky, kr, SSD1306_WHITE);
    }

    // ── Slingshots (thicker on flash) ────────────────────────────────────────
    for (int i = 0; i < 2; i++) {
        d.drawLine((int)SLING[i].x1, (int)SLING[i].y1,
                   (int)SLING[i].x2, (int)SLING[i].y2, SSD1306_WHITE);
        int ox = (i == 0) ? 1 : -1;
        if (_slingFlash[i] > 0)
            d.drawLine((int)SLING[i].x1 + ox, (int)SLING[i].y1,
                       (int)SLING[i].x2 + ox, (int)SLING[i].y2, SSD1306_WHITE);
    }

    // ── Decorative side rails (small horizontal ticks on outer walls) ─────────
    d.drawFastHLine(0, 70, 3, SSD1306_WHITE);   // left wall tick
    d.drawFastHLine(61, 70, 3, SSD1306_WHITE);  // right wall tick

    // ── Inlane separators ────────────────────────────────────────────────────
    for (int i = 0; i < 2; i++)
        d.drawLine((int)INLANE[i].x1, (int)INLANE[i].y1,
                   (int)INLANE[i].x2, (int)INLANE[i].y2, SSD1306_WHITE);

    // ── Gutter walls ─────────────────────────────────────────────────────────
    d.drawLine((int)GUTTER[0].x1, (int)GUTTER[0].y1,
               (int)GUTTER[0].x2, (int)GUTTER[0].y2, SSD1306_WHITE);
    d.drawLine((int)GUTTER[1].x1, (int)GUTTER[1].y1,
               (int)GUTTER[1].x2, (int)GUTTER[1].y2, SSD1306_WHITE);

    // ── Spiral loop ───────────────────────────────────────────────────────────
    // Draw an Archimedean spiral of 1.5 turns, radius 7→1, centered at (32,80).
    // The loop flash inverts the spiral to white-on-black for the reward blink.
    {
        const int   lcx = (int)LOOP_CX, lcy = (int)LOOP_CY;
        const bool  blink = (_loopFlash > 0) && ((_loopFlash % 6) < 3);
        if (blink) d.fillCircle(lcx, lcy, (int)LOOP_R + 1, SSD1306_WHITE);
        // Plot the spiral curve: angle 0 → 3π, radius 7 → 1
        const int STEPS = 48;
        for (int s = 0; s <= STEPS; s++) {
            float a = (3.0f * PI_F * s) / STEPS;
            float r = LOOP_R - (LOOP_R - 1.0f) * (a / (3.0f * PI_F));
            int sx = lcx + (int)(r * cosf(a));
            int sy = lcy + (int)(r * sinf(a));
            d.drawPixel(sx, sy, blink ? SSD1306_BLACK : SSD1306_WHITE);
        }
        // Entry arrow: small tick on the outer ring showing where to aim
        d.drawLine(lcx + (int)LOOP_R, lcy - 2, lcx + (int)LOOP_R, lcy + 2, blink ? SSD1306_BLACK : SSD1306_WHITE);
    }

    // ── Center post ───────────────────────────────────────────────────────────
    if (_postFlash > 0) d.fillCircle((int)POST_X, (int)POST_Y, (int)POST_R, SSD1306_WHITE);
    else                d.drawCircle((int)POST_X, (int)POST_Y, (int)POST_R, SSD1306_WHITE);

    // ── Flippers (2 px thick) + hinge dots ───────────────────────────────────
    int lTX = (int)(LPX + FL * cosf(_leftAngle));
    int lTY = (int)(LPY + FL * sinf(_leftAngle));
    int rTX = (int)(RPX + FL * cosf(_rightAngle));
    int rTY = (int)(RPY + FL * sinf(_rightAngle));
    d.drawLine((int)LPX, (int)LPY,     lTX, lTY,     SSD1306_WHITE);
    d.drawLine((int)LPX, (int)LPY + 1, lTX, lTY + 1, SSD1306_WHITE);
    d.drawLine((int)RPX, (int)RPY,     rTX, rTY,     SSD1306_WHITE);
    d.drawLine((int)RPX, (int)RPY + 1, rTX, rTY + 1, SSD1306_WHITE);
    // Hinge dots at pivot points
    d.fillCircle((int)LPX, (int)LPY, 2, SSD1306_WHITE);
    d.fillCircle((int)RPX, (int)RPY, 2, SSD1306_WHITE);

    // ── Field decorations (diamonds at key empty points) ──────────────────────
    drawDiamond(d, 32, 107);   // between post and flippers
    drawDiamond(d, 32, 36);    // center of bumper diamond
    drawDiamond(d, 6, 83);     // left side between sling and gutter
    drawDiamond(d, 57, 83);    // right side

    // ── Kickback indicator (chevron, left outlane) ────────────────────────────
    if (_kickbackAvail) {
        d.drawLine(1, 99, 4, 102, SSD1306_WHITE);
        d.drawLine(1, 105, 4, 102, SSD1306_WHITE);
    }
    if (_kickbackFlash > 0) {
        d.setTextSize(1); d.setCursor(0, 93); d.print("KB!");
    }

    // ── Ball ─────────────────────────────────────────────────────────────────
    if (_state == PLAYING || _state == READY)
        d.fillCircle((int)_bx, (int)_by, (int)BALL_R, SSD1306_WHITE);

    // ── Launch arrow (READY) ──────────────────────────────────────────────────
    if (_state == READY) {
        int bx = (int)_bx, by = (int)_by;
        d.drawLine(bx, by + 6, bx, by + 13, SSD1306_WHITE);
        d.drawLine(bx - 2, by + 8, bx, by + 6, SSD1306_WHITE);
        d.drawLine(bx + 2, by + 8, bx, by + 6, SSD1306_WHITE);
        // Hi-score display in READY state
        if (_hiScore > 0) {
            d.setTextSize(1);
            d.setCursor(5, 80);
            d.print("HI:"); d.print(_hiScore);
        }
    }

    // ── Nudge indicators (small squares) ─────────────────────────────────────
    if (_state == PLAYING) {
        for (int i = 0; i < _nudgesLeft; i++)
            d.fillRect(1 + i * 4, 10, 3, 3, SSD1306_WHITE);
    }

    // ── Target clear counter (shown as small ticks at top-right of field) ─────
    if (_state == PLAYING && _targetClears > 0) {
        for (int i = 0; i < _targetClears && i < JACK_CLEARS; i++)
            d.fillRect(58 + i * 3, 10, 2, 3, SSD1306_WHITE);
    }

    // ── TILT overlay (only during PLAYING) ───────────────────────────────────
    if (_tilted && _state == PLAYING) {
        d.setTextSize(1);
        d.setCursor(18, 80);
        d.print("TILT!");
    }

    // ── Priority overlays: loop > jackpot won > skill > bonus > mult ──────────
    // Text shows on the OPPOSITE phase from the spiral fill-blink so it's always
    // readable (white text over the normal sparse spiral, not over the white fill).
    if (_loopFlash > 0 && (_loopFlash % 6) >= 3) {
        d.setTextSize(1);
        d.setCursor(4, 72); d.print("SPIRALE!");
    } else if (_skillFlash > 0) {
        d.setTextSize(1);
        d.setCursor(2, 72); d.print("SKILL +500!");
    } else if (_bonusFlash > 0 && _jackpotFlash == 0) {
        d.setTextSize(1);
        d.setCursor(10, 72); d.print("BONUS!");
    } else if (_multFlash > 0) {
        d.setTextSize(1);
        d.setCursor(14, 72); d.print("x"); d.print(_mult); d.print(" UP!");
    }

    // ── State overlays ────────────────────────────────────────────────────────
    if (_state == READY) {
        d.setTextSize(1);
        d.setCursor(4, 108); d.print("BTN: lancia");
        d.setCursor(4, 118); d.print("TILT:flipper");
    }
    if (_state == BALL_LOST) {
        d.setTextSize(1);
        d.setCursor(4,  38); d.print("PALLA PERSA");
        d.setCursor(4,  50); d.print("Punti:"); d.print(_ballScore);
        d.setCursor(4,  62); d.print("Vite:"); d.print(_lives);
        if (_jackpotReady) {
            d.setCursor(4, 74); d.print("JACKPOT ATT!");
        }
    }

    d.display();
    d.setRotation(0);
}
