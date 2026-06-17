#pragma once
#include <stdint.h>

class Adafruit_SSD1306;

class TamagotchiGame {
public:
    void begin();
    bool update(float dt, bool btnPressed, bool shake, Adafruit_SSD1306& d);

private:
    enum PetState : uint8_t { NORMAL, HAPPY, HUNGRY, SAD, TIRED, SICK, DEAD };
    enum Action   : uint8_t { ACT_FEED, ACT_PLAY, ACT_SLEEP, ACT_CURE, ACT_COUNT };

    float    _hunger, _happy, _energy, _health;
    PetState _petState;
    Action   _selAction;
    float    _actionCooldown;
    bool     _sleeping;
    float    _sleepTimer;
    bool     _lastShake;
    bool     _lastBtn;
    uint32_t _deadMs;
    uint8_t  _animFrame;   // 0..63 wrapping counter, drives all animations

    void computePetState();
    void doAction(Action a);
    void drawStatBars(Adafruit_SSD1306& d);
    void drawPikachu (Adafruit_SSD1306& d, int16_t cx, int16_t cy);
    void drawMenu    (Adafruit_SSD1306& d);
};
