#include "Button.h"
#include <Arduino.h>

void Button::begin(int pin) {
    _pin = pin;
    pinMode(_pin, INPUT_PULLUP);   // active LOW (pressed = GND)
}

void Button::update() {
    _pressed = false;
    const bool raw = (digitalRead(_pin) == LOW);
    const uint32_t now = millis();

    if (raw != _state && now - _lastMs > 20) {  // 20 ms debounce
        _state  = raw;
        _lastMs = now;
        if (_state) _pressed = true;             // trigger on falling edge
    }
}

bool Button::pressed() const { return _pressed; }
bool Button::held()    const { return _state;   }
