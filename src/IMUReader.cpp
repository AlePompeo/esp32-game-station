#include "IMUReader.h"
#include "config.h"
#include <math.h>

bool IMUReader::begin() {
    if (!_mpu.begin()) return false;
    _mpu.setAccelerometerRange(MPU6050_RANGE_2_G);
    _mpu.setGyroRange(MPU6050_RANGE_250_DEG);
    _mpu.setFilterBandwidth(MPU6050_BAND_21_HZ);
    return true;
}

void IMUReader::update() {
    sensors_event_t a, g, temp;
    _mpu.getEvent(&a, &g, &temp);

    // GY-521 mounted vertically (pins down): the chip's X axis is now vertical
    // and reads ~9.81 m/s² at rest → useless for tilt.
    // Y and Z are both horizontal and respond to board tilt.
    //   Y → left/right tilt  (display X axis)
    //   Z → forward/backward tilt  (display Y axis)
    // If a direction is inverted flip the corresponding GRAV_SIGN in config.h.
    float ax = a.acceleration.y;
    float ay = a.acceleration.z;

    // Low-pass filter: tracks slow tilt, attenuates high-frequency noise.
    // IMU_ALPHA=0.4 still lets shake frequencies (~2-4 Hz) partially through
    // so the gravity vector naturally reacts to device motion.
    _filtX += IMU_ALPHA * (ax - _filtX);
    _filtY += IMU_ALPHA * (ay - _filtY);

    _gx = _filtX * GRAV_SIGN_X * GRAVITY_SCALE;
    _gy = _filtY * GRAV_SIGN_Y * GRAVITY_SCALE;

    // High-frequency component magnitude → shake detection
    float hx = ax - _filtX;
    float hy = ay - _filtY;
    _shaking = sqrtf(hx * hx + hy * hy) > SHAKE_THRESHOLD;
}
