#pragma once
#include <stdint.h>

// ─── Button ──────────────────────────────────────────────────────────────────
// GPIO0 = built-in BOOT button on most ESP32 dev boards (active LOW, INPUT_PULLUP)
// Change to any free GPIO if using an external button.
constexpr int BUTTON_PIN = 0;

// ─── I2C ────────────────────────────────────────────────────────────────────
constexpr int      SDA_PIN  = 21;
constexpr int      SCL_PIN  = 22;
constexpr uint32_t I2C_FREQ = 400000UL;

// ─── Display  SSD1306 128×64 ─────────────────────────────────────────────────
constexpr int     SCREEN_W   = 128;
constexpr int     SCREEN_H   = 64;
constexpr uint8_t OLED_ADDR  = 0x3C;
constexpr int8_t  OLED_RESET = -1;

// ─── Simulation ──────────────────────────────────────────────────────────────
constexpr int   NUM_BALLS        = 40;
constexpr float BALL_RADIUS      = 2.0f;
constexpr float GRAVITY_SCALE    = 35.0f;   // m/s² → px/s²

// Velocity decay expressed as a per-second rate so physics is dt-independent.
// Applied as: v *= (1 - DAMPING_COEFF * dt).
// Value 0.76 means v halves in ~0.9s  (≡ DAMPING=0.975 at 30 fps).
constexpr float DAMPING_COEFF    = 0.76f;

constexpr float RESTITUTION      = 0.20f;
constexpr float WALL_RESTITUTION = 0.35f;
constexpr float WALL_FRICTION    = 0.97f;
constexpr float MAX_SPEED        = 400.0f;  // px/s
constexpr int   SOLVER_ITERS     = 4;

// Render loop target (physics runs independently on Core 0)
constexpr int   TARGET_FPS       = 30;

// ─── IMU ─────────────────────────────────────────────────────────────────────
constexpr float IMU_ALPHA        = 0.40f;
constexpr float SHAKE_THRESHOLD  = 5.0f;
constexpr float SHAKE_IMPULSE    = 120.0f;

// ─── Axis mapping ─────────────────────────────────────────────────────────────
constexpr float GRAV_SIGN_X = +1.0f;
constexpr float GRAV_SIGN_Y = -1.0f;
