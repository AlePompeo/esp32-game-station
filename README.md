# ESP32 Game Station

A handheld retro game console built around an **ESP32**, a **128×64 OLED display** (SSD1306) and a **6-axis IMU** (MPU6050). The device has a single push-button and uses physical tilt as the primary input — no joystick needed.

---

## Hardware

| Component | Details |
|---|---|
| Microcontroller | ESP32 (dual-core, 240 MHz) |
| Display | SSD1306 128×64 OLED via I2C |
| IMU | MPU6050 (accelerometer + gyro) via I2C |
| Button | Single push-button on GPIO0 (BOOT pin) |

### Wiring

```
ESP32          SSD1306 / MPU6050
------         -----------------
GPIO21  ──────  SDA
GPIO22  ──────  SCL
3.3V    ──────  VCC
GND     ──────  GND
GPIO0   ──────  Button (other leg to GND)
```

---

## Games (20 titles)

| # | Game | Controls |
|---|---|---|
| 1 | **Mercury** | Tilt to roll 40 fluid balls around the screen |
| 2 | **Space Invaders** | Tilt to move ship, button to fire |
| 3 | **Racer** | Tilt left/right to steer |
| 4 | **Asteroid Shooter** | Tilt to aim, button to fire |
| 5 | **Tamagotchi** | Tilt + button to interact with your pet |
| 6 | **Pinball** | Tilt for flippers, button to launch |
| 7 | **Pac-Man** | Tilt in 4 directions to move |
| 8 | **Pong** | Tilt to move paddle |
| 9 | **Arkanoid** | Tilt to move paddle, button to launch ball |
| 10 | **Snake** | Tilt to change direction |
| 11 | **Flappy Bird** | Button / shake to flap |
| 12 | **Tetris** | Tilt to move, tilt up to rotate, shake to drop |
| 13 | **Labyrinth** | Tilt to roll the ball through the maze |
| 14 | **Doom** | Full raycaster — tilt to move and turn, button to shoot |
| 15 | **Dino** | Button / shake to jump |
| 16 | **Kong** | Tilt to run, tilt up/down near a ladder to climb, button to jump |
| 17 | **Mini Golf** | Tilt to aim + adjust power, button to hit |
| 18 | **Pocket Fighter** | Tilt to move, shake / button to attack |
| 19 | **Blackjack** | Tilt + button to hit or stand |
| 20 | **ArduRacer** | Tilt to steer, button to accelerate |

---

## Architecture

The firmware uses **FreeRTOS** on both cores of the ESP32:

- **Core 0** — runs compute-heavy game logic (physics, AI, raycasting) via a generic `computeTask` with a swappable function pointer.
- **Core 1** — handles I2C reads (IMU + display), button debouncing, game state, and rendering.

WiFi and Bluetooth are disabled at boot to free ~80 KB of RAM and reduce power consumption.

```
Core 0  ┌─────────────────────────────────────┐
        │  computeTask()                       │
        │  ├─ physics.step()   (Mercury)       │
        │  ├─ logicStep()      (Invaders)      │
        │  ├─ logicStep()      (Racer)         │
        │  └─ logicStep()      (Asteroid)      │
        └─────────────────────────────────────┘

Core 1  ┌─────────────────────────────────────┐
        │  loop()                              │
        │  ├─ btn.update()                     │
        │  ├─ imu.update()                     │
        │  ├─ game.update(dt, ...)             │
        │  └─ display()                        │
        └─────────────────────────────────────┘
```

Single-core games (Tetris, Pong, Kong, …) run entirely on Core 1; `g_computeRun` is set to `false` to keep Core 0 idle.

---

## Build & Flash

This project uses [PlatformIO](https://platformio.org/).

```bash
# Install PlatformIO CLI (if not already installed)
pip install platformio

# Clone the repository
git clone https://github.com/AlePompeo/esp32-game-station.git
cd esp32-game-station

# Build
pio run

# Upload to connected ESP32
pio run --target upload

# Monitor serial output
pio device monitor
```

Build flags in `platformio.ini`:

```ini
build_flags =
    -O3
    -ffast-math
    -DCORE_DEBUG_LEVEL=0
```

---

## Project Structure

```
esp32-game-station/
├── platformio.ini          # PlatformIO project configuration
└── src/
    ├── main.cpp            # Entry point, app state machine, Core 0/1 routing
    ├── config.h            # Pin map, display size, physics constants
    ├── Button.h/cpp        # Debounced button driver
    ├── IMUReader.h/cpp     # MPU6050 wrapper with shake detection
    ├── Renderer.h/cpp      # SSD1306 display helper
    ├── Physics.h/cpp       # 2D rigid-body physics engine (Mercury)
    ├── Menu.h/cpp          # Scrollable game selector
    ├── Game.h/cpp          # Mercury fluid-ball game
    ├── SpaceInvaders.h/cpp
    ├── RacerGame.h/cpp
    ├── AsteroidGame.h/cpp
    ├── TamagotchiGame.h/cpp
    ├── PinballGame.h/cpp
    ├── PacmanGame.h/cpp
    ├── PongGame.h/cpp
    ├── ArkanoidGame.h/cpp
    ├── SnakeGame.h/cpp
    ├── FlappyGame.h/cpp
    ├── TetrisGame.h/cpp
    ├── LabyrinthGame.h/cpp
    ├── DoomGame.h/cpp      # Raycaster engine
    ├── DoomLevel.h         # Level map data
    ├── DoomSprites.h       # Enemy sprite data
    ├── DinoGame.h/cpp
    ├── KongGame.h/cpp      # Donkey Kong-style platformer
    ├── MiniGolfGame.h/cpp
    ├── MiniGolfLevels.h
    ├── PocketFighterGame.h/cpp
    ├── BlackjackGame.h/cpp
    └── ArduRacerGame.h/cpp
```

---

## Dependencies

Managed automatically by PlatformIO:

| Library | Version |
|---|---|
| Adafruit GFX Library | ^1.11.9 |
| Adafruit SSD1306 | ^2.5.9 |
| Adafruit MPU6050 | ^2.2.4 |
| Adafruit Unified Sensor | ^1.1.14 |

---

## License

MIT License — see [LICENSE](LICENSE) for details.
