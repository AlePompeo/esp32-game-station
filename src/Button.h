#pragma once
#include <stdint.h>

class Button {
public:
    void begin(int pin);
    void update();              // call once per render frame
    bool pressed() const;       // true for ONE frame on button-down edge
    bool held()    const;       // true while held

private:
    int      _pin     = -1;
    bool     _state   = false;
    bool     _pressed = false;
    uint32_t _lastMs  = 0;
};
