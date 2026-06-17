#include "Menu.h"
#include "Renderer.h"
#include <string.h>

static const char* const GAME_NAMES[GAME_COUNT] = {
    "MERCURY", "INVADERS", "RACER", "ASTEROID",
    "TAMAGOTCHI", "PINBALL", "PACMAN", "PONG",
    "ARKANOID", "SNAKE", "FLAPPY", "TETRIS",
    "LABYRINTH",
    "DOOM",
    "DINO",
    "KONG",
    "MINIGOLF",
    "FIGHTER",
    "BLACKJACK",
    "ARDURACER"
};

void Menu::reset() {
    _idx       = 0;
    _selected  = false;
    _lastShake = false;
}

void Menu::update(const Button& btn, const IMUReader& imu, Renderer& renderer) {
    _selected = false;

    if (btn.pressed()) {
        _idx = (_idx + 1) % GAME_COUNT;
    }

    const bool shakeEdge = imu.isShaking() && !_lastShake;
    _lastShake = imu.isShaking();
    if (shakeEdge) _selected = true;

    render(renderer);
}

void Menu::render(Renderer& renderer) {
    Adafruit_SSD1306& d = renderer.rawDisplay();
    d.clearDisplay();
    d.setTextColor(SSD1306_WHITE);

    // ── Intestazione ──────────────────────────────────────────────────────
    d.setTextSize(1);
    const int16_t hw = (int16_t)(15 * 6);   // "SELEZIONA GIOCO" = 15 char × 6px
    d.setCursor((128 - hw) / 2, 0);
    d.print("SELEZIONA GIOCO");
    d.drawFastHLine(0, 9, 128, SSD1306_WHITE);

    // ── Nome gioco (grande, centrato) ─────────────────────────────────────
    const char*  name = GAME_NAMES[_idx];
    int16_t tw = (int16_t)(strlen(name) * 12);   // textSize 2 → 12 px/char
    int16_t nx = (128 - tw) / 2;
    if (nx < 0) nx = 0;
    d.setTextSize(2);
    d.setCursor(nx, 12);
    d.print(name);

    // ── Suggerimenti navigazione ──────────────────────────────────────────
    d.setTextSize(1);
    d.setCursor(4, 31);
    d.print("BTN: scorri");
    d.setCursor(4, 41);
    d.print("SCUOTI: gioca");

    // ── Separatore inferiore ──────────────────────────────────────────────
    d.drawFastHLine(0, 52, 128, SSD1306_WHITE);

    char counter[16];
    snprintf(counter, sizeof(counter), "Gioco %d / %d", _idx + 1, GAME_COUNT);
    int16_t cw = (int16_t)(strlen(counter) * 6);
    d.setCursor((128 - cw) / 2, 56);
    d.print(counter);

    d.display();
}
