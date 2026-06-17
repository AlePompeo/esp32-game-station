#include "Game.h"
#include "Level.h"
#include "config.h"
#include <Arduino.h>

static const Level LEVELS[] = {
    { { {0, 30, 100, 2} }, 1, {}, 0, 4, 4, 60, 22, 102, 34, 22, 26, 0, 24, "Goccio"    },
    { { {0, 22, 90, 2}, {38, 44, 90, 2} }, 2, {}, 0, 4, 3, 60, 15, 4, 48, 30, 12, 0, 22, "Zigzag"    },
    { { {0, 16, 80, 2}, {48, 34, 80, 2}, {0, 50, 80, 2} }, 3, {}, 0, 3, 2, 75, 12, 82, 52, 42, 8, 0, 18, "Tripla"    },
    { { {0, 22, 98, 2}, {0, 42, 98, 2} }, 2,
      { {5, 26, 18, 14}, {36, 26, 20, 14}, {68, 26, 16, 14} }, 3,
      3, 2, 72, 18, 102, 46, 22, 14, 0, 15, "Baratro"   },
    { { {0, 20, 85, 2}, {43, 38, 85, 2}, {2, 54, 2, 8}, {38, 54, 2, 8}, {2, 61, 38, 2} }, 5,
      { {18, 42, 20, 10} }, 1, 3, 2, 55, 16, 4, 54, 34, 7, EDGE_BOTTOM, 14, "Precipizio" }
};

static constexpr int NUM_LEVELS = (int)(sizeof(LEVELS)/sizeof(LEVELS[0]));

void Game::setState(State s) { _state = s; _stateMs = millis(); }
void Game::loadLevel(uint8_t idx, Physics& p) { p.init(LEVELS[idx]); }

void Game::begin(Physics& p) {
    _levelIdx = 0;
    loadLevel(0, p);
    setState(SPLASH);
}

bool Game::update(const Ball* balls, int ballCount, Physics& physics,
                   const IMUReader& imu, const Button& btn, Renderer& renderer) {
    const Level& lv       = LEVELS[_levelIdx];
    const bool   shakeEdge = imu.isShaking() && !_lastShake;
    _lastShake = imu.isShaking();
    const bool   act       = shakeEdge || btn.pressed();

    switch (_state) {

        case SPLASH:
            renderer.drawLevelIntro(_levelIdx, lv.name);
            if (millis() - _stateMs > 1500UL) setState(PLAYING);
            return false;

        case PLAYING: {
            _ballsInDest = 0;
            for (int i = 0; i < ballCount; i++)
                if (balls[i].x >= lv.dstX && balls[i].x <= lv.dstX+lv.dstW &&
                    balls[i].y >= lv.dstY && balls[i].y <= lv.dstY+lv.dstH)
                    _ballsInDest++;

            renderer.renderGame(balls, ballCount, lv, _ballsInDest, lv.ballsToWin);

            if (_ballsInDest >= lv.ballsToWin)
                setState(_levelIdx+1 < NUM_LEVELS ? WIN : COMPLETE);
            else if (ballCount < lv.ballsToWin)
                setState(LOSE);

            return true;
        }

        case WIN:
            renderer.drawWinScreen(_levelIdx, _ballsInDest, lv.ballsToWin);
            if (act || millis()-_stateMs > 3000UL) {
                _levelIdx++;
                loadLevel(_levelIdx, physics);
                setState(SPLASH);
            }
            return false;

        case LOSE:
            renderer.drawLoseScreen(_levelIdx);
            if (act) { loadLevel(_levelIdx, physics); setState(SPLASH); }
            return false;

        case COMPLETE:
            renderer.drawVictoryScreen();
            if (act) { _levelIdx=0; loadLevel(0, physics); setState(SPLASH); }
            return false;
    }
    return false;
}
