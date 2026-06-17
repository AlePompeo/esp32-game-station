#pragma once
#include <stdint.h>

constexpr int MAX_WALLS = 8;
constexpr int MAX_VOIDS = 4;

struct WallRect { int16_t x, y, w, h; };

// Bitmask for open screen edges (balls that cross them are destroyed)
constexpr uint8_t EDGE_LEFT   = 0x01;
constexpr uint8_t EDGE_RIGHT  = 0x02;
constexpr uint8_t EDGE_TOP    = 0x04;
constexpr uint8_t EDGE_BOTTOM = 0x08;

struct Level {
    WallRect    walls[MAX_WALLS];
    uint8_t     wallCount;
    WallRect    voids[MAX_VOIDS]; // balls entering these zones are destroyed
    uint8_t     voidCount;
    int16_t     srcX, srcY, srcW, srcH;
    int16_t     dstX, dstY, dstW, dstH;
    uint8_t     openEdges;
    uint8_t     ballsToWin;
    const char* name;
};
