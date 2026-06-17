#pragma once
#include <Adafruit_SSD1306.h>
#include <stdint.h>

class ArduRacerGame {
public:
    void begin();
    bool update(float dt, float gravX, float gravY,
                bool shake, bool btnPressed, bool btnHeld, Adafruit_SSD1306& d);

private:
    static constexpr uint8_t  TILE_SZ      = 64;
    static constexpr uint8_t  NUM_LEVELS   = 10;
    static constexpr float    STEER_THRESH = 14.0f;
    static constexpr float    TURN_RATE    = 3.0f;    // rad/sec (from racer.cpp 0.003*tFrameMs)
    static constexpr float    MAX_SPEED    = 132.0f;  // world-px/sec
    static constexpr float    ACCEL_RATE   = 180.0f;  // world-px/sec²
    static constexpr float    DRAG_RATE    = 76.3f;   // world-px/sec²
    static constexpr float    OFFROAD_MAX  = 44.0f;   // max_speed/3
    static constexpr float    OFFROAD_DEC  = 600.0f;  // off-road deceleration

    enum class Mode : uint8_t { TITLE, SELECT, RACING };

    uint8_t  _level     = 1;
    uint8_t  _levelSize = 10;

    float    _carX = 0.0f, _carY = 0.0f;
    float    _carRot   = 0.0f;
    float    _carSpeed = 0.0f;
    bool     _offRoad  = false;

    bool     _lapRunning  = false;
    uint32_t _lapStart    = 0;
    uint32_t _lapCurrent  = 0;
    uint8_t  _curLap      = 0;
    uint8_t  _chkPassed   = 0;
    uint8_t  _chkTotal    = 1;
    uint8_t  _lastTile    = 24;
    uint32_t _bestLaps[NUM_LEVELS];
    bool     _newBest     = false;
    uint32_t _newBestMs   = 0;

    Mode     _mode      = Mode::TITLE;
    uint32_t _modeMs    = 0;
    bool     _prevShake = false;
    bool     _prevBtn   = false;

    uint8_t  getTile(uint8_t tx, uint8_t ty) const;
    bool     isRoad(uint8_t t) const;
    void     resetRace();
    void     doPhysics(float dt, float gravX, bool btnHeld);
    void     checkTileEvents();
    void     fmtTime(char* buf, uint32_t ms) const;

    void     renderTitle(Adafruit_SSD1306& d);
    void     renderSelect(Adafruit_SSD1306& d);
    void     renderRace(Adafruit_SSD1306& d);
    void     drawTile(Adafruit_SSD1306& d, int16_t sx, int16_t sy, uint8_t t) const;
    void     drawCar(Adafruit_SSD1306& d, int16_t cx, int16_t cy) const;
    void     drawMinimap(Adafruit_SSD1306& d, int16_t ox, int16_t oy) const;
};
