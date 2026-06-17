#pragma once
#include "Ball.h"
#include "Level.h"
#include "config.h"

class Physics {
public:
    void        init(const Level& level);
    void        step(float gx, float gy, float dt);
    void        applyShake(float strength);
    int         activeCount() const { return _active; }
    const Ball* balls()       const { return _balls; }

private:
    Ball         _balls[NUM_BALLS];
    const Level* _level  = nullptr;
    int          _active = NUM_BALLS;

    void applyForcesAndDamp(float gx, float gy, float dt);
    void integrate(float dt);
    void resolveWalls();
    void resolvePairs();
    void resolveStaticWalls();
    void checkVoid();
    void checkOpenEdges();

    static void resolveOneBallWall(Ball& b, const WallRect& wr);
};
