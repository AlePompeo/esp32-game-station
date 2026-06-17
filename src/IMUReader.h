#pragma once
#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>

class IMUReader {
public:
    bool begin();
    void update();

    // Gravity vector in display-pixel space  (px/s²)
    float gravX()     const { return _gx; }
    float gravY()     const { return _gy; }

    // True when high-frequency acceleration exceeds SHAKE_THRESHOLD
    bool  isShaking() const { return _shaking; }

private:
    Adafruit_MPU6050 _mpu;
    float _filtX  = 0.f;
    float _filtY  = 0.f;
    float _gx     = 0.f;
    float _gy     = 0.f;
    bool  _shaking = false;
};
