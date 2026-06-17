#include "Physics.h"
#include <Arduino.h>
#include <math.h>

// ─── Init ─────────────────────────────────────────────────────────────────────

void Physics::init(const Level& level) {
    _level  = &level;
    _active = NUM_BALLS;
    randomSeed(esp_random());

    const float D2    = (BALL_RADIUS * 2.f) * (BALL_RADIUS * 2.f);
    const int   xSpan = max(1, (int)(level.srcW - BALL_RADIUS * 2.f));
    const int   ySpan = max(1, (int)(level.srcH - BALL_RADIUS * 2.f));

    for (int i = 0; i < NUM_BALLS; i++) {
        int tries = 0;
        while (true) {
            float x = level.srcX + BALL_RADIUS + (float)random(0, xSpan);
            float y = level.srcY + BALL_RADIUS + (float)random(0, ySpan);
            bool ok = true;
            for (int j = 0; j < i && ok; j++) {
                float dx = x - _balls[j].x, dy = y - _balls[j].y;
                ok = (dx*dx + dy*dy) >= D2;
            }
            if (ok || tries++ > 150) { _balls[i] = {x, y, 0.f, 0.f}; break; }
        }
    }
}

// ─── Shake ────────────────────────────────────────────────────────────────────

void Physics::applyShake(float strength) {
    for (int i = 0; i < _active; i++) {
        _balls[i].vx += (float)(random(-1000, 1001)) * 0.001f * strength;
        _balls[i].vy += (float)(random(-1000, 1001)) * 0.001f * strength;
    }
}

// ─── Forces & damping ─────────────────────────────────────────────────────────

void Physics::applyForcesAndDamp(float gx, float gy, float dt) {
    // dt-independent damping: v *= (1 - k*dt) ≈ e^(-k*dt) for small dt
    const float damp = 1.f - DAMPING_COEFF * dt;

    for (int i = 0; i < _active; i++) {
        Ball& b = _balls[i];
        b.vx = b.vx * damp + gx * dt;
        b.vy = b.vy * damp + gy * dt;

        const float spd2 = b.vx*b.vx + b.vy*b.vy;
        if (spd2 > MAX_SPEED * MAX_SPEED) {
            const float inv = MAX_SPEED / sqrtf(spd2);
            b.vx *= inv; b.vy *= inv;
        }
    }
}

void Physics::integrate(float dt) {
    for (int i = 0; i < _active; i++) {
        _balls[i].x += _balls[i].vx * dt;
        _balls[i].y += _balls[i].vy * dt;
    }
}

// ─── Screen-edge walls ────────────────────────────────────────────────────────

void Physics::resolveWalls() {
    const uint8_t open = _level ? _level->openEdges : 0;

    for (int i = 0; i < _active; i++) {
        Ball& b = _balls[i];

        if (!(open & EDGE_LEFT) && b.x < BALL_RADIUS) {
            b.x  = BALL_RADIUS;
            b.vx = fabsf(b.vx) * WALL_RESTITUTION;
            b.vy *= WALL_FRICTION;
        } else if (!(open & EDGE_RIGHT) && b.x > SCREEN_W - BALL_RADIUS) {
            b.x  = SCREEN_W - BALL_RADIUS;
            b.vx = -fabsf(b.vx) * WALL_RESTITUTION;
            b.vy *= WALL_FRICTION;
        }

        if (!(open & EDGE_TOP) && b.y < BALL_RADIUS) {
            b.y  = BALL_RADIUS;
            b.vy = fabsf(b.vy) * WALL_RESTITUTION;
            b.vx *= WALL_FRICTION;
        } else if (!(open & EDGE_BOTTOM) && b.y > SCREEN_H - BALL_RADIUS) {
            b.y  = SCREEN_H - BALL_RADIUS;
            b.vy = -fabsf(b.vy) * WALL_RESTITUTION;
            b.vx *= WALL_FRICTION;
        }
    }
}

// ─── Ball–ball collision ─────────────────────────────────────────────────────

void Physics::resolvePairs() {
    const float minDist  = BALL_RADIUS * 2.f;
    const float minDist2 = minDist * minDist;

    for (int i = 0; i < _active - 1; i++) {
        for (int j = i + 1; j < _active; j++) {
            const float dx = _balls[j].x - _balls[i].x;
            const float dy = _balls[j].y - _balls[i].y;
            const float d2 = dx*dx + dy*dy;
            if (d2 >= minDist2 || d2 < 1e-6f) continue;

            const float dist = sqrtf(d2);
            const float nx = dx/dist, ny = dy/dist;
            const float half = (minDist - dist) * 0.5f;

            _balls[i].x -= nx*half; _balls[i].y -= ny*half;
            _balls[j].x += nx*half; _balls[j].y += ny*half;

            const float dvn = (_balls[j].vx - _balls[i].vx)*nx
                            + (_balls[j].vy - _balls[i].vy)*ny;
            if (dvn < 0.f) {
                const float J = -(1.f + RESTITUTION) * dvn * 0.5f;
                _balls[i].vx -= J*nx; _balls[i].vy -= J*ny;
                _balls[j].vx += J*nx; _balls[j].vy += J*ny;
            }
        }
    }
}

// ─── Static level-wall collision (AABB) ──────────────────────────────────────

void Physics::resolveOneBallWall(Ball& b, const WallRect& wr) {
    const float cx = constrain(b.x, (float)wr.x, (float)(wr.x + wr.w));
    const float cy = constrain(b.y, (float)wr.y, (float)(wr.y + wr.h));
    const float dx = b.x - cx, dy = b.y - cy;
    const float d2 = dx*dx + dy*dy;
    if (d2 >= BALL_RADIUS * BALL_RADIUS) return;

    float nx, ny, pen;
    if (d2 < 1e-6f) {
        const float dL = b.x - wr.x, dR = wr.x+wr.w - b.x;
        const float dT = b.y - wr.y, dB = wr.y+wr.h - b.y;
        if      (dL<=dR && dL<=dT && dL<=dB) { nx=-1; ny=0; pen=dL+BALL_RADIUS; }
        else if (dR<=dT && dR<=dB)            { nx=+1; ny=0; pen=dR+BALL_RADIUS; }
        else if (dT<=dB)                       { nx=0; ny=-1; pen=dT+BALL_RADIUS; }
        else                                   { nx=0; ny=+1; pen=dB+BALL_RADIUS; }
    } else {
        const float d = sqrtf(d2);
        nx = dx/d; ny = dy/d; pen = BALL_RADIUS - d;
    }

    b.x += nx*pen; b.y += ny*pen;

    const float vn = b.vx*nx + b.vy*ny;
    if (vn < 0.f) {
        const float J = -(1.f + WALL_RESTITUTION) * vn;
        b.vx += J*nx; b.vy += J*ny;
        const float tx = -ny, ty = nx;
        const float vt = b.vx*tx + b.vy*ty;
        b.vx -= (1.f - WALL_FRICTION)*vt*tx;
        b.vy -= (1.f - WALL_FRICTION)*vt*ty;
    }
}

void Physics::resolveStaticWalls() {
    if (!_level) return;
    for (int i = 0; i < _active; i++)
        for (int w = 0; w < _level->wallCount; w++)
            resolveOneBallWall(_balls[i], _level->walls[w]);
}

// ─── Void zones & open edges: swap-remove balls ───────────────────────────────

void Physics::checkVoid() {
    if (!_level || _level->voidCount == 0) return;
    int i = 0;
    while (i < _active) {
        bool lost = false;
        for (int v = 0; v < _level->voidCount && !lost; v++) {
            const WallRect& vz = _level->voids[v];
            lost = (_balls[i].x >= vz.x && _balls[i].x <= vz.x + vz.w &&
                    _balls[i].y >= vz.y && _balls[i].y <= vz.y + vz.h);
        }
        if (lost) { _balls[i] = _balls[--_active]; } else { ++i; }
    }
}

void Physics::checkOpenEdges() {
    if (!_level || _level->openEdges == 0) return;
    const uint8_t open = _level->openEdges;
    int i = 0;
    while (i < _active) {
        Ball& b   = _balls[i];
        bool  out = false;
        if ((open & EDGE_LEFT)   && b.x < -BALL_RADIUS)           out = true;
        if ((open & EDGE_RIGHT)  && b.x >  SCREEN_W + BALL_RADIUS) out = true;
        if ((open & EDGE_TOP)    && b.y < -BALL_RADIUS)           out = true;
        if ((open & EDGE_BOTTOM) && b.y >  SCREEN_H + BALL_RADIUS) out = true;
        if (out) { _balls[i] = _balls[--_active]; } else { ++i; }
    }
}

// ─── Main step ────────────────────────────────────────────────────────────────

void Physics::step(float gx, float gy, float dt) {
    applyForcesAndDamp(gx, gy, dt);
    integrate(dt);
    checkVoid();
    checkOpenEdges();
    for (int k = 0; k < SOLVER_ITERS; k++) {
        resolvePairs();
        resolveStaticWalls();
        resolveWalls();
    }
}
