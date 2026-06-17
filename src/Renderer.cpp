#include "Renderer.h"
#include <Wire.h>

Renderer::Renderer() : _display(128, 64, &Wire, -1) {}

bool Renderer::begin() {
    if (!_display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR)) return false;
    _display.clearDisplay();
    _display.display();
    return true;
}

// ─── Main game render ─────────────────────────────────────────────────────────

void Renderer::renderGame(const Ball* balls, int ballCount, const Level& level,
                           int ballsInDest, int ballsToWin) {
    _display.clearDisplay();

    // Internal level walls
    for (int i = 0; i < level.wallCount; i++) {
        const WallRect& w = level.walls[i];
        _display.fillRect(w.x, w.y, w.w, w.h, SSD1306_WHITE);
    }

    // Open edges: dashed border on each open side (balls fall off there)
    if (level.openEdges & EDGE_BOTTOM)
        for (int16_t x = 1; x < 127; x += 5) _display.drawFastHLine(x, 63, 3, SSD1306_WHITE);
    if (level.openEdges & EDGE_TOP)
        for (int16_t x = 1; x < 127; x += 5) _display.drawFastHLine(x,  0, 3, SSD1306_WHITE);
    if (level.openEdges & EDGE_LEFT)
        for (int16_t y = 1; y < 63;  y += 5) _display.drawFastVLine(0,  y, 3, SSD1306_WHITE);
    if (level.openEdges & EDGE_RIGHT)
        for (int16_t y = 1; y < 63;  y += 5) _display.drawFastVLine(127, y, 3, SSD1306_WHITE);

    // Void zones: white border + X cross on each one
    for (int i = 0; i < level.voidCount; i++) {
        const WallRect& vz = level.voids[i];
        _display.drawRect(vz.x, vz.y, vz.w, vz.h, SSD1306_WHITE);
        _display.drawLine(vz.x,       vz.y,       vz.x+vz.w, vz.y+vz.h, SSD1306_WHITE);
        _display.drawLine(vz.x+vz.w,  vz.y,       vz.x,      vz.y+vz.h, SSD1306_WHITE);
    }

    // Destination zone: dotted border
    for (int16_t x = level.dstX; x <= level.dstX + level.dstW; x += 4)
        _display.drawPixel(x, level.dstY,              SSD1306_WHITE);
    for (int16_t x = level.dstX; x <= level.dstX + level.dstW; x += 4)
        _display.drawPixel(x, level.dstY + level.dstH,  SSD1306_WHITE);
    for (int16_t y = level.dstY; y <= level.dstY + level.dstH; y += 4)
        _display.drawPixel(level.dstX,              y, SSD1306_WHITE);
    for (int16_t y = level.dstY; y <= level.dstY + level.dstH; y += 4)
        _display.drawPixel(level.dstX + level.dstW,  y, SSD1306_WHITE);

    // Balls with black outline (only active ones)
    for (int i = 0; i < ballCount; i++) {
        int16_t x = static_cast<int16_t>(balls[i].x);
        int16_t y = static_cast<int16_t>(balls[i].y);
        int16_t r = static_cast<int16_t>(BALL_RADIUS);
        _display.fillCircle(x, y, r, SSD1306_WHITE);
        _display.drawCircle(x, y, r, SSD1306_BLACK);
    }

    // Progress bar: bottom 2 rows (y=62..63)
    if (ballsToWin > 0) {
        int filled = constrain((int)(126.f * ballsInDest / ballsToWin), 0, 126);
        _display.drawFastHLine(1, 62, 126, SSD1306_WHITE);
        if (filled > 0) _display.drawFastHLine(1, 63, filled, SSD1306_WHITE);
    }

    _display.display();
}

// ─── Overlay screens ──────────────────────────────────────────────────────────

void Renderer::drawLevelIntro(int levelNum, const char* name) {
    _display.clearDisplay();
    _display.setTextSize(1);
    _display.setTextColor(SSD1306_WHITE);

    // "LIVELLO X" centred horizontally
    char buf[16];
    snprintf(buf, sizeof(buf), "LIVELLO %d", levelNum + 1);
    int16_t tw = strlen(buf) * 6;
    _display.setCursor((128 - tw) / 2, 18);
    _display.print(buf);

    // Level name centred
    int16_t nw = strlen(name) * 6;
    _display.setCursor((128 - nw) / 2, 32);
    _display.print(name);

    _display.display();
}

void Renderer::drawWinScreen(int levelNum, int ballsIn, int ballsToWin) {
    _display.clearDisplay();
    _display.setTextSize(1);
    _display.setTextColor(SSD1306_WHITE);

    _display.setCursor(32, 8);
    _display.print("LIVELLO ");
    _display.print(levelNum + 1);
    _display.print(" OK!");

    char buf[20];
    snprintf(buf, sizeof(buf), "Palline: %d / %d", ballsIn, ballsToWin);
    int16_t tw = strlen(buf) * 6;
    _display.setCursor((128 - tw) / 2, 24);
    _display.print(buf);

    _display.setCursor(8, 44);
    _display.print("Agita per continuare");

    _display.display();
}

void Renderer::drawLoseScreen(int levelNum) {
    _display.clearDisplay();
    _display.setTextSize(1);
    _display.setTextColor(SSD1306_WHITE);

    _display.setCursor(16, 8);
    _display.print("Troppo fluido perso!");

    char buf[16];
    snprintf(buf, sizeof(buf), "Livello %d", levelNum + 1);
    int16_t tw = strlen(buf) * 6;
    _display.setCursor((128 - tw) / 2, 24);
    _display.print(buf);

    _display.setCursor(16, 44);
    _display.print("Agita per riprovare");

    _display.display();
}

void Renderer::drawVictoryScreen() {
    _display.clearDisplay();
    _display.setTextSize(1);
    _display.setTextColor(SSD1306_WHITE);

    _display.setCursor(22, 12);
    _display.print("HAI VINTO!");

    _display.setCursor(13, 26);
    _display.print("Tutti i livelli");
    _display.setCursor(22, 36);
    _display.print("completati!");

    _display.setCursor(5, 52);
    _display.print("Agita per ripartire");

    _display.display();
}
