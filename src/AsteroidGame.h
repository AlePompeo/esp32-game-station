#pragma once
#include <Adafruit_SSD1306.h>
#include <freertos/semphr.h>
#include <stdint.h>

class AsteroidGame {
public:
    static constexpr int MAX_ROCKS         = 5;
    static constexpr int MAX_BULLETS       = 3;
    static constexpr int NUM_STARS         = 14;
    static constexpr int MAX_POWERUPS      = 2;
    static constexpr int MAX_ENEMIES       = 2;
    static constexpr int MAX_ENEMY_BULLETS = 4;

    enum PowerUpType : uint8_t { PU_SHIELD=0, PU_RAPIDFIRE=1, PU_TRIPLESHOT=2, PU_EXTRALIFE=3 };

    struct Rock        { float x, y, vx, vy; int8_t r; bool active; };
    struct Bullet      { float x, y; bool active; };
    struct PowerUp     { float x, y; bool active; PowerUpType type; };
    struct Enemy       { float x, y, vy; bool active; float fireTimer; };
    struct EnemyBullet { float x, y; bool active; };

    void begin();
    void logicStep(float shipX, float shipY, bool fire, float dt);
    bool renderFrame(Adafruit_SSD1306& d, bool btnPressed, bool shake);

private:
    enum GState : uint8_t { PLAYING, GAME_OVER };

    float        _shipX, _shipY;
    Rock         _rocks[MAX_ROCKS];
    Bullet       _bullets[MAX_BULLETS];
    float        _starX[NUM_STARS];
    float        _starY[NUM_STARS];
    float        _starVX[NUM_STARS];
    PowerUp      _powerups[MAX_POWERUPS];
    Enemy        _enemies[MAX_ENEMIES];
    EnemyBullet  _eBullets[MAX_ENEMY_BULLETS];
    int          _score, _lives;
    int          _level, _rocksDestroyed;
    float        _spawnTimer, _spawnInterval;
    float        _fireTimer;
    float        _puSpawnTimer, _puInterval;
    float        _enemySpawnTimer, _enemyInterval;
    float        _shieldTimer;
    float        _rapidFireTimer;
    float        _tripleShotTimer;
    GState       _gstate;
    uint32_t     _stateMs;

    struct Snap {
        float        shipX, shipY;
        Rock         rocks[MAX_ROCKS];
        Bullet       bullets[MAX_BULLETS];
        float        starX[NUM_STARS];
        float        starY[NUM_STARS];
        PowerUp      powerups[MAX_POWERUPS];
        Enemy        enemies[MAX_ENEMIES];
        EnemyBullet  eBullets[MAX_ENEMY_BULLETS];
        int          score, lives, level;
        float        shieldTimer, rapidFireTimer, tripleShotTimer;
        GState       gstate;
        uint32_t     stateMs;
    } _snap;

    SemaphoreHandle_t _mutex = nullptr;

    void spawnRock();
    void spawnPowerUp();
    void spawnEnemy();
    void fireBullet();
    void fireEnemyBullet(int ei);
    void checkCollisions();
    void copyToSnap();
    void setLevel(int lv);

    static void drawShip   (Adafruit_SSD1306& d, int16_t cx, int16_t cy);
    static void drawRock   (Adafruit_SSD1306& d, int16_t cx, int16_t cy, int8_t r);
    static void drawEnemy  (Adafruit_SSD1306& d, int16_t cx, int16_t cy);
    static void drawPowerUp(Adafruit_SSD1306& d, int16_t cx, int16_t cy, PowerUpType t);
};
