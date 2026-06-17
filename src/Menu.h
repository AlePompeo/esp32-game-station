#pragma once
#include "Button.h"
#include "IMUReader.h"

enum GameId : uint8_t {
    GAME_MERCURY = 0, GAME_INVADERS, GAME_RACER, GAME_ASTEROID,
    GAME_TAMA, GAME_PINBALL, GAME_PACMAN, GAME_PONG,
    GAME_ARKANOID, GAME_SNAKE, GAME_FLAPPY, GAME_TETRIS,
    GAME_LABYRINTH,
    GAME_DOOM,
    GAME_DINO,
    GAME_KONG,
    GAME_MINIGOLF,
    GAME_FIGHTER,
    GAME_BLACKJACK,
    GAME_ARDURACER,
    GAME_COUNT
};

// Thin forward-declare to avoid circular Renderer↔Menu includes
class Adafruit_SSD1306;
class Renderer;

class Menu {
public:
    void   update(const Button& btn, const IMUReader& imu, Renderer& renderer);
    bool   gameSelected() const { return _selected; }
    GameId selectedGame() const { return (GameId)_idx; }
    void   reset();

private:
    int  _idx      = 0;
    bool _selected = false;
    bool _lastShake = false;

    void render(Renderer& renderer);
};
