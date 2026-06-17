#pragma once
#include <Adafruit_SSD1306.h>
#include <freertos/semphr.h>
#include <stdint.h>

class RacerGame {
public:
    static constexpr int MAX_OBS     = 2;   // solo 2 avversari
    static constexpr int MAX_SCENERY = 10;  // più densità palazzi

    void begin();
    void logicStep(float steerInput, float dt);
    bool renderFrame(Adafruit_SSD1306& d, bool btnPressed, bool shake);

private:
    enum GState : uint8_t { PLAYING, RESULT };

    struct Obs       { float z, laneX; bool active; };
    struct SceneryObj{ float z; uint8_t type; int8_t side; bool active; };
    // type: 0=palazzo  1=palma  2=albero

    float      _playerX;
    float      _speed;
    float      _distance;
    float      _curvePhase;
    float      _cameraX;
    float      _roadBend;   // shift laterale accumulato del fondo strada (px)
    bool       _offTrack;
    Obs        _obs[MAX_OBS];
    SceneryObj _scenery[MAX_SCENERY];
    int        _score;
    GState     _gstate;
    uint32_t   _stateMs;

    struct Snap {
        float    playerX, speed, distance, cameraX, roadBend;
        struct   { float z, laneX; bool active; }            obs[MAX_OBS];
        struct   { float z; uint8_t type; int8_t side; bool active; } scenery[MAX_SCENERY];
        bool     offTrack;
        int      score;
        GState   gstate;
        uint32_t stateMs;
    } _snap;

    SemaphoreHandle_t _mutex = nullptr;

    void spawnObs    (int i, float minZ);
    void spawnScenery(int i, float minZ);
    void copyToSnap  ();

    static void drawSky       (Adafruit_SSD1306& d, int vanishX);
    static void drawRoad      (Adafruit_SSD1306& d, const Snap& s);
    static void drawPlayerCar (Adafruit_SSD1306& d, int16_t cx, int16_t bot);
    static void drawObsCar    (Adafruit_SSD1306& d, int16_t cx, int16_t bot, float scale);
    static void drawSceneryObj(Adafruit_SSD1306& d, int ox, int oy, float t, uint8_t type);
};
