#pragma once
#include <Adafruit_SSD1306.h>

class MiniGolfGame {
public:
    void begin();
    // gravX: tilt left/right for yaw aim
    // gravY: tilt forward for power
    // pressed: shot / advance on edge
    // held: currently held down
    // shook: shake gesture
    bool update(float dt, float gravX, float gravY,
                bool pressed, bool held, bool shook,
                Adafruit_SSD1306& d);
};
