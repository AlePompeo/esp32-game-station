#pragma once
#include <Adafruit_SSD1306.h>
#include "Ball.h"
#include "Level.h"
#include "config.h"

class Renderer {
public:
    Renderer();
    bool begin();

    // Main game frame: walls + balls + progress bar
    void renderGame(const Ball* balls, int ballCount, const Level& level,
                    int ballsInDest, int ballsToWin);

    // Direct display access for games that render themselves
    Adafruit_SSD1306& rawDisplay() { return _display; }

    // Overlay screens
    void drawLevelIntro(int levelNum, const char* name);
    void drawWinScreen(int levelNum, int ballsIn, int ballsToWin);
    void drawLoseScreen(int levelNum);
    void drawVictoryScreen();

private:
    Adafruit_SSD1306 _display;
};
