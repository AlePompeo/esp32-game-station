#include <Arduino.h>
#include <Wire.h>
#include <WiFi.h>
#include <esp_wifi.h>
#include <esp_bt.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/semphr.h>
#include "config.h"
#include "Button.h"
#include "IMUReader.h"
#include "Physics.h"
#include "Renderer.h"
#include "Menu.h"
#include "Game.h"
#include "SpaceInvaders.h"
#include "RacerGame.h"
#include "AsteroidGame.h"
#include "TamagotchiGame.h"
#include "PinballGame.h"
#include "PacmanGame.h"
#include "PongGame.h"
#include "ArkanoidGame.h"
#include "SnakeGame.h"
#include "FlappyGame.h"
#include "TetrisGame.h"
#include "LabyrinthGame.h"
#include "DoomGame.h"
#include "DinoGame.h"
#include "KongGame.h"
#include "MiniGolfGame.h"
#include "PocketFighterGame.h"
#include "BlackjackGame.h"
#include "ArduRacerGame.h"

// ─── Application state ────────────────────────────────────────────────────────
enum AppState : uint8_t {
    APP_MENU, APP_MERCURY, APP_INVADERS, APP_RACER, APP_ASTEROID,
    APP_TAMA, APP_PINBALL, APP_PACMAN, APP_PONG,
    APP_ARKANOID, APP_SNAKE, APP_FLAPPY, APP_TETRIS,
    APP_LABYRINTH, APP_DOOM, APP_DINO, APP_KONG, APP_MINIGOLF,
    APP_FIGHTER, APP_BLACKJACK, APP_ARDURACER
};
static AppState g_app = APP_MENU;   // set once at boot, never changes afterwards

// ─── Generic compute task (Core 0) ───────────────────────────────────────────
// Both cores are fully devoted to the active game:
//   Mercury  → Core 0: physics.step()          Core 1: IMU + render + state
//   Invaders → Core 0: invaders.logicStep()    Core 1: IMU + render + state
// Adding a new game only requires setting a new function pointer.
typedef void (*ComputeFn)(float dt);
static volatile ComputeFn g_computeFn  = nullptr;
static volatile bool      g_computeRun = false;

// ─── Mercury shared state (Core 0 ↔ Core 1) ──────────────────────────────────
static volatile float g_mercGx = 0.f, g_mercGy = 0.f;
static volatile bool  g_mercShake = false;
static Ball           g_mercSnap[NUM_BALLS];
static volatile int   g_mercSnapCnt = 0;
static SemaphoreHandle_t g_mercMutex;

// ─── Space Invaders shared input (Core 1 writes, Core 0 reads) ───────────────
static volatile float g_siShipX  = 64.f;
static volatile bool  g_siFire   = false;

// ─── Racer shared input ───────────────────────────────────────────────────────
static volatile float g_racerSteer = 0.f;

// ─── Asteroid shared input ────────────────────────────────────────────────────
static volatile float g_asShipX = 18.f;
static volatile float g_asShipY = 36.f;
static volatile bool  g_asFire  = false;

// ─── Subsystems ───────────────────────────────────────────────────────────────
static Button        btn;
static IMUReader     imu;
static Physics       physics;
static Renderer      renderer;
static Menu          menu;
static Game          mercury;
static SpaceInvaders  invaders;
static RacerGame      racer;
static AsteroidGame   asteroid;
static TamagotchiGame tama;
static PinballGame    pinball;
static PacmanGame     pacman;
static PongGame       pong;
static ArkanoidGame   arkanoid;
static SnakeGame      snake;
static FlappyGame     flappy;
static TetrisGame     tetris;
static LabyrinthGame  labyrinth;
static DoomGame       doom;
static DinoGame       dino;
static KongGame       kong;
static MiniGolfGame      minigolf;
static PocketFighterGame fighter;
static BlackjackGame     blackjack;
static ArduRacerGame     arduracer;

// ─── Compute functions ────────────────────────────────────────────────────────

static void mercuryComputeFn(float dt) {
    if (g_mercShake) { physics.applyShake(SHAKE_IMPULSE); g_mercShake = false; }
    physics.step(g_mercGx, g_mercGy, dt);
    if (xSemaphoreTake(g_mercMutex, 0) == pdTRUE) {
        g_mercSnapCnt = physics.activeCount();
        memcpy(g_mercSnap, physics.balls(), g_mercSnapCnt * sizeof(Ball));
        xSemaphoreGive(g_mercMutex);
    }
}

static void siComputeFn(float dt) {
    const bool fire = g_siFire;
    if (fire) g_siFire = false;
    invaders.logicStep(g_siShipX, fire, dt);
}

static void racerComputeFn(float dt) {
    racer.logicStep(g_racerSteer, dt);
}

static void asteroidComputeFn(float dt) {
    const bool fire = g_asFire;
    if (fire) g_asFire = false;
    asteroid.logicStep(g_asShipX, g_asShipY, fire, dt);
}

// ─── Core 0: generic compute task ────────────────────────────────────────────
// One task, one function pointer. To support a new game, add a new ComputeFn
// and point g_computeFn to it — zero architectural changes needed.
static void computeTask(void*) {
    uint32_t lastUs = esp_timer_get_time();
    for (;;) {
        vTaskDelay(pdMS_TO_TICKS(1));   // 1 ms cadence, feeds watchdog

        if (!g_computeRun || !g_computeFn) { lastUs = esp_timer_get_time(); continue; }

        const uint32_t now = esp_timer_get_time();
        float dt = (now - lastUs) * 1e-6f;
        lastUs   = now;
        if (dt < 5e-5f) dt = 5e-5f;
        if (dt > 0.02f)  dt = 0.02f;

        g_computeFn(dt);
    }
}

// ─── Core 1: I/O loop ────────────────────────────────────────────────────────

void setup() {
    Serial.begin(115200);

    // Free Core 0 from radio tasks and reclaim ~80 KB RAM
    WiFi.mode(WIFI_OFF);
    esp_wifi_stop();
    btStop();
    esp_bt_controller_disable();

    btn.begin(BUTTON_PIN);
    Wire.begin(SDA_PIN, SCL_PIN);
    Wire.setClock(I2C_FREQ);

    if (!imu.begin())      { Serial.println("[ERR] MPU6050"); while (true) delay(1000); }
    if (!renderer.begin()) { Serial.println("[ERR] SSD1306");  while (true) delay(1000); }

    g_mercMutex = xSemaphoreCreateMutex();

    // Launch Core 0 compute task (priority 2 > Arduino loop priority 1)
    xTaskCreatePinnedToCore(computeTask, "compute", 4096, nullptr, 2, nullptr, 0);

    Serial.println("[OK] ready");
}

void loop() {
    static uint32_t lastMs = millis();
    const uint32_t  now    = millis();
    const float     dt     = constrain((now - lastMs) * 0.001f, 0.001f, 0.05f);
    lastMs = now;

    btn.update();
    imu.update();

    switch (g_app) {

        // ── BOOT MENU (runs once until a game is chosen) ──────────────────
        case APP_MENU: {
            g_computeRun = false;
            menu.update(btn, imu, renderer);
            if (!menu.gameSelected()) break;

            const GameId chosen = menu.selectedGame();
            menu.reset();

            if (chosen == GAME_MERCURY) {
                mercury.begin(physics);
                g_mercSnapCnt = physics.activeCount();
                memcpy(g_mercSnap, physics.balls(), g_mercSnapCnt * sizeof(Ball));
                g_computeFn = mercuryComputeFn;
                g_app       = APP_MERCURY;
                // g_computeRun is managed frame-by-frame below
            } else if (chosen == GAME_INVADERS) {
                invaders.begin();
                g_computeFn  = siComputeFn;
                g_computeRun = true;
                g_app        = APP_INVADERS;
            } else if (chosen == GAME_RACER) {
                racer.begin();
                g_computeFn  = racerComputeFn;
                g_computeRun = true;
                g_app        = APP_RACER;
            } else if (chosen == GAME_ASTEROID) {
                g_asShipX = 18.f;
                g_asShipY = 36.f;
                asteroid.begin();
                g_computeFn  = asteroidComputeFn;
                g_computeRun = true;
                g_app        = APP_ASTEROID;
            } else if (chosen == GAME_TAMA) {
                tama.begin();
                g_computeFn  = nullptr;
                g_computeRun = false;
                g_app        = APP_TAMA;
            } else if (chosen == GAME_PINBALL) {
                pinball.begin();
                g_computeFn  = nullptr;
                g_computeRun = false;
                g_app        = APP_PINBALL;
            } else if (chosen == GAME_PACMAN) {
                pacman.begin();
                g_computeFn  = nullptr;
                g_computeRun = false;
                g_app        = APP_PACMAN;
            } else if (chosen == GAME_PONG) {
                pong.begin();
                g_computeFn  = nullptr;
                g_computeRun = false;
                g_app        = APP_PONG;
            } else if (chosen == GAME_ARKANOID) {
                arkanoid.begin();
                g_computeFn  = nullptr;
                g_computeRun = false;
                g_app        = APP_ARKANOID;
            } else if (chosen == GAME_SNAKE) {
                snake.begin();
                g_computeFn  = nullptr;
                g_computeRun = false;
                g_app        = APP_SNAKE;
            } else if (chosen == GAME_FLAPPY) {
                flappy.begin();
                g_computeFn  = nullptr;
                g_computeRun = false;
                g_app        = APP_FLAPPY;
            } else if (chosen == GAME_TETRIS) {
                tetris.begin();
                g_computeFn  = nullptr;
                g_computeRun = false;
                g_app        = APP_TETRIS;
            } else if (chosen == GAME_LABYRINTH) {
                labyrinth.begin();
                g_computeFn  = nullptr;
                g_computeRun = false;
                g_app        = APP_LABYRINTH;
            } else if (chosen == GAME_DOOM) {
                doom.begin();
                g_computeFn  = nullptr;
                g_computeRun = false;
                g_app        = APP_DOOM;
            } else if (chosen == GAME_DINO) {
                dino.begin();
                g_computeFn  = nullptr;
                g_computeRun = false;
                g_app        = APP_DINO;
            } else if (chosen == GAME_KONG) {
                kong.begin();
                g_computeFn  = nullptr;
                g_computeRun = false;
                g_app        = APP_KONG;
            } else if (chosen == GAME_MINIGOLF) {
                minigolf.begin();
                g_computeFn  = nullptr;
                g_computeRun = false;
                g_app        = APP_MINIGOLF;
            } else if (chosen == GAME_FIGHTER) {
                fighter.begin();
                g_computeFn  = nullptr;
                g_computeRun = false;
                g_app        = APP_FIGHTER;
            } else if (chosen == GAME_BLACKJACK) {
                blackjack.begin();
                g_computeFn  = nullptr;
                g_computeRun = false;
                g_app        = APP_BLACKJACK;
            } else if (chosen == GAME_ARDURACER) {
                arduracer.begin();
                g_computeFn  = nullptr;
                g_computeRun = false;
                g_app        = APP_ARDURACER;
            }
            break;
        }

        // ── MERCURY: Core 0 = physics, Core 1 = IMU + state + render ──────
        case APP_MERCURY: {
            g_mercGx = imu.gravX();
            g_mercGy = imu.gravY();
            if (imu.isShaking()) g_mercShake = true;

            Ball localBalls[NUM_BALLS];
            int  localCount;
            xSemaphoreTake(g_mercMutex, portMAX_DELAY);
            localCount = g_mercSnapCnt;
            memcpy(localBalls, g_mercSnap, localCount * sizeof(Ball));
            xSemaphoreGive(g_mercMutex);

            const bool physicsNeeded =
                mercury.update(localBalls, localCount, physics, imu, btn, renderer);
            g_computeRun = physicsNeeded;
            break;
        }

        // ── RACER: Core 0 = logic, Core 1 = input + render ───────────────
        case APP_RACER: {
            g_racerSteer = imu.gravX();

            const bool racerRunning =
                racer.renderFrame(renderer.rawDisplay(), btn.pressed(), imu.isShaking());

            if (!racerRunning) {
                g_computeRun = false;
                delay(2);
                racer.begin();
                g_computeRun = true;
            }
            break;
        }

        // ── ASTEROID SHOOTER: Core 0 = logic, Core 1 = input + render ────
        case APP_ASTEROID: {
            // Fighter-jet style: tilt board to steer, button to fire
            g_asShipX = constrain(
                g_asShipX + imu.gravX() * 0.40f * dt,
                8.f, 42.f);
            g_asShipY = constrain(
                g_asShipY + imu.gravY() * 0.40f * dt,
                14.f, 58.f);
            if (btn.pressed()) g_asFire = true;

            const bool astRunning =
                asteroid.renderFrame(renderer.rawDisplay(), btn.pressed(), imu.isShaking());

            if (!astRunning) {
                g_computeRun = false;
                delay(2);
                g_asShipX = 18.f;
                g_asShipY = 36.f;
                asteroid.begin();
                g_computeRun = true;
            }
            break;
        }

        // ── TAMAGOTCHI: single-core, all logic on Core 1 ─────────────────
        case APP_TAMA: {
            g_computeRun = false;
            const bool tamaRunning =
                tama.update(dt, btn.pressed(), imu.isShaking(), renderer.rawDisplay());
            if (!tamaRunning) tama.begin();
            break;
        }

        // ── PINBALL: single-core, all on Core 1 ──────────────────────────
        case APP_PINBALL: {
            g_computeRun = false;
            const bool running =
                pinball.update(dt, btn.held(), btn.pressed(),
                               imu.isShaking(), imu.gravX(), imu.gravY(),
                               renderer.rawDisplay());
            if (!running) pinball.begin();
            break;
        }

        // ── PAC-MAN: single-core, all on Core 1 ─────────────────────────
        case APP_PACMAN: {
            g_computeRun = false;
            const bool running =
                pacman.update(dt, btn.pressed(), imu.gravX(), imu.gravY(),
                              renderer.rawDisplay());
            if (!running) pacman.begin();
            break;
        }

        // ── PONG: single-core, all on Core 1 ─────────────────────────────
        case APP_PONG: {
            g_computeRun = false;
            const bool running =
                pong.update(dt, imu.gravY(), btn.pressed(), imu.isShaking(), renderer.rawDisplay());
            if (!running) pong.begin();
            break;
        }

        // ── ARKANOID: single-core ─────────────────────────────────────────
        case APP_ARKANOID: {
            g_computeRun = false;
            const bool running =
                arkanoid.update(dt, imu.gravX(), btn.pressed(), renderer.rawDisplay());
            if (!running) arkanoid.begin();
            break;
        }

        // ── SNAKE: single-core ────────────────────────────────────────────
        case APP_SNAKE: {
            g_computeRun = false;
            const bool running =
                snake.update(dt, imu.gravX(), imu.gravY(), btn.pressed(), renderer.rawDisplay());
            if (!running) snake.begin();
            break;
        }

        // ── FLAPPY BIRD: single-core ──────────────────────────────────────
        case APP_FLAPPY: {
            g_computeRun = false;
            const bool running =
                flappy.update(dt, btn.pressed(), imu.isShaking(), renderer.rawDisplay());
            if (!running) flappy.begin();
            break;
        }

        // ── TETRIS: single-core ───────────────────────────────────────────
        case APP_TETRIS: {
            g_computeRun = false;
            const bool running =
                tetris.update(dt, imu.gravX(), imu.gravY(), btn.pressed(), imu.isShaking(), renderer.rawDisplay());
            if (!running) tetris.begin();
            break;
        }

        // ── LABYRINTH: single-core ────────────────────────────────────────
        case APP_LABYRINTH: {
            g_computeRun = false;
            const bool running =
                labyrinth.update(dt, imu.gravX(), imu.gravY(), btn.pressed(), renderer.rawDisplay());
            if (!running) labyrinth.begin();
            break;
        }

        // ── DINO: single-core ─────────────────────────────────────────────
        case APP_DINO: {
            g_computeRun = false;
            const bool dinoRunning =
                dino.update(dt, btn.pressed(), imu.isShaking(), renderer.rawDisplay());
            if (!dinoRunning) dino.begin();
            break;
        }

        // ── DOOM: single-core, full raycaster on Core 1 ──────────────────
        case APP_DOOM: {
            g_computeRun = false;
            doom.update(dt, imu.gravX(), imu.gravY(),
                        btn.held(), btn.pressed(), renderer.rawDisplay());
            break;
        }
        
        // ── KONG: single-core ──────────────────
        case APP_KONG: {
            g_computeRun = false;
            const bool running = kong.update(dt, btn.pressed(), imu.isShaking(), imu.gravX(), imu.gravY(), renderer.rawDisplay());
            if (!running) kong.begin();
            break;
        }

        // ── MINI GOLF: single-core 3D, all on Core 1 ─────────────────────
        case APP_MINIGOLF: {
            g_computeRun = false;
            const bool running =
                minigolf.update(dt, imu.gravX(), imu.gravY(),
                                btn.pressed(), btn.held(), imu.isShaking(),
                                renderer.rawDisplay());
            if (!running) minigolf.begin();
            break;
        }

        // ── POCKET FIGHTER: single-core, all on Core 1 ───────────────────
        case APP_FIGHTER: {
            g_computeRun = false;
            fighter.update(dt, imu.gravX(), imu.gravY(),
                           imu.isShaking(), btn.pressed(),
                           renderer.rawDisplay());
            break;
        }

        // ── BLACKJACK: single-core, all on Core 1 ────────────────────────
        case APP_BLACKJACK: {
            g_computeRun = false;
            blackjack.update(dt, imu.gravX(), imu.gravY(),
                             imu.isShaking(), btn.pressed(),
                             renderer.rawDisplay());
            break;
        }

        // ── ARDURACER: single-core, all on Core 1 ────────────────────────
        case APP_ARDURACER: {
            g_computeRun = false;
            arduracer.update(dt, imu.gravX(), imu.gravY(),
                             imu.isShaking(), btn.pressed(), btn.held(),
                             renderer.rawDisplay());
            break;
        }

        // ── SPACE INVADERS: Core 0 = logic, Core 1 = input + render ──────
        case APP_INVADERS: {
            // Ship input: Core 1 writes, Core 0 reads on next step
            g_siShipX = constrain(
                g_siShipX + imu.gravX() * 0.28f * dt,
                5.f, 119.f);
            if (btn.pressed()) g_siFire = true;

            const bool running =
                invaders.renderFrame(renderer.rawDisplay(), btn.pressed(), imu.isShaking());

            if (!running) {
                // Restart: pause Core 0 briefly, reinit, resume
                g_computeRun = false;
                delay(2);
                g_siShipX = 64.f;
                invaders.begin();
                g_computeRun = true;
            }
            break;
        }
    }

    // Pace render loop to TARGET_FPS
    const int32_t wait = (1000 / TARGET_FPS) - (int32_t)(millis() - now);
    if (wait > 0) delay((uint32_t)wait);
}
