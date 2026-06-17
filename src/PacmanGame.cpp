#include "PacmanGame.h"
#include <Arduino.h>

// ─── Maze: '#'=wall, '.'=dot, 'o'=power pellet (16 cols × 7 rows) ─────────────
// Row 3 is the tunnel row: outer cells (col 0-3, col 12-15) wrap left↔right.
static const char MAZE[] =
    "################"
    "#o...#....#...o#"
    "#.##...##...##.#"
    ".....#....#....."
    "#.##...##...##.#"
    "#o...#....#...o#"
    "################";

// dir: 0=R 1=D 2=L 3=U
static const int8_t DX[4] = {  1, 0, -1, 0 };
static const int8_t DY[4] = {  0, 1,  0, -1 };

static constexpr uint8_t  PAC_TICK   = 7;
static constexpr uint8_t  GHOST_TICK = 9;
static constexpr uint8_t  LOOP_TICK  = 63;   // LCM(7,9)
static constexpr uint8_t  TUNNEL_ROW = 3;
static constexpr uint32_t FRIGHT_MS  = 7000; // ghost scared duration

static constexpr int8_t P0C = 7, P0R = 3;            // pac-man start (center tunnel)
static constexpr int8_t G0C[2] = {  2, 13 };          // ghost starts (far corners)
static constexpr int8_t G0R[2] = {  1,  5 };

// ─── Helpers ─────────────────────────────────────────────────────────────────

bool PacmanGame::isWall(int c, int r) const {
    if (c < 0 || c >= 16 || r < 0 || r >= 7) return true;
    return MAZE[r * 16 + c] == '#';
}

bool PacmanGame::hasDot(int c, int r) const {
    return (_dots[r] >> (15 - c)) & 1;
}

void PacmanGame::clrDot(int c, int r) {
    if (!hasDot(c, r)) return;
    _dots[r] &= ~(uint16_t)(1u << (15 - c));
    _dotsLeft--;
    if (MAZE[r * 16 + c] == 'o') {
        _score += 50;
        _frightMs = millis();
    } else {
        _score += 10;
    }
}

void PacmanGame::initDots() {
    _dotsLeft = 0;
    for (int r = 0; r < 7; r++) {
        _dots[r] = 0;
        for (int c = 0; c < 16; c++) {
            if (!isWall(c, r)) { _dots[r] |= (uint16_t)(1u << (15 - c)); _dotsLeft++; }
        }
    }
}

// Wrap column for tunnel row
static inline int tunnelWrap(int c, int r) {
    if (r != TUNNEL_ROW) return c;
    if (c < 0) return 15;
    if (c >= 16) return 0;
    return c;
}

void PacmanGame::moveGhost(int i) {
    bool fright = (millis() - _frightMs < FRIGHT_MS);
    int8_t rev = (_gDir[i] + 2) % 4;
    int8_t best = _gDir[i];
    int bestDist = fright ? -1 : 9999;

    for (int d = 0; d < 4; d++) {
        if (d == rev) continue;
        int nc = tunnelWrap(_gCol[i] + DX[d], _gRow[i] + DY[d]);
        int nr = _gRow[i] + DY[d];
        if (isWall(nc, nr)) continue;
        int dist = abs(nc - _pCol) + abs(nr - _pRow);
        if (fright ? (dist > bestDist) : (dist < bestDist)) {
            bestDist = dist;
            best = (int8_t)d;
        }
    }

    int nc = tunnelWrap(_gCol[i] + DX[best], _gRow[i] + DY[best]);
    int nr = _gRow[i] + DY[best];
    if (isWall(nc, nr)) {
        best = rev;
        nc = tunnelWrap(_gCol[i] + DX[rev], _gRow[i] + DY[rev]);
        nr = _gRow[i] + DY[rev];
    }
    if (!isWall(nc, nr)) { _gCol[i] = (int8_t)nc; _gRow[i] = (int8_t)nr; _gDir[i] = best; }
}

// ─── Public ───────────────────────────────────────────────────────────────────

void PacmanGame::begin() {
    _pCol = P0C; _pRow = P0R; _pDir = 0; _nDir = 0;
    for (int i = 0; i < NG; i++) { _gCol[i] = G0C[i]; _gRow[i] = G0R[i]; _gDir[i] = 1; }
    _score = 0; _lives = 3; _moveTick = 0; _anim = 0;
    _frightMs = 0;
    _state = READY; _stateMs = millis();
    initDots();
    clrDot(_pCol, _pRow);
}

bool PacmanGame::update(float dt, bool btnPressed, float gravX, float gravY, Adafruit_SSD1306& d) {
    float ax = fabsf(gravX), ay = fabsf(gravY);
    if (ax > 50.0f || ay > 50.0f) {
        if (ax >= ay) _nDir = (gravX < 0) ? 2 : 0;
        else          _nDir = (gravY < 0) ? 3 : 1;
    }

    _anim++;

    switch (_state) {
        case READY:
            if (btnPressed) _state = PLAYING;
            break;

        case PLAYING: {
            _moveTick++;
            if (_moveTick >= LOOP_TICK) _moveTick = 0;

            // ── Pac-Man move ──────────────────────────────────────────────────
            if (_moveTick % PAC_TICK == 0) {
                int nc = tunnelWrap(_pCol + DX[_nDir], _pRow + DY[_nDir]);
                int nr = _pRow + DY[_nDir];
                if (!isWall(nc, nr)) { _pCol = (int8_t)nc; _pRow = (int8_t)nr; _pDir = _nDir; }
                else {
                    nc = tunnelWrap(_pCol + DX[_pDir], _pRow + DY[_pDir]);
                    nr = _pRow + DY[_pDir];
                    if (!isWall(nc, nr)) { _pCol = (int8_t)nc; _pRow = (int8_t)nr; }
                }
                clrDot(_pCol, _pRow);
            }

            // ── Ghost move ────────────────────────────────────────────────────
            if (_moveTick % GHOST_TICK == 0)
                for (int i = 0; i < NG; i++) moveGhost(i);

            // ── Collision ─────────────────────────────────────────────────────
            bool fright = (millis() - _frightMs < FRIGHT_MS);
            for (int i = 0; i < NG; i++) {
                if (_gCol[i] == _pCol && _gRow[i] == _pRow) {
                    if (fright) {
                        _score += 200;
                        _gCol[i] = G0C[i]; _gRow[i] = G0R[i]; _gDir[i] = 1;
                    } else {
                        _lives--;
                        _state   = (_lives <= 0) ? GAME_OVER : DEAD;
                        _stateMs = millis();
                        break;
                    }
                }
            }
            if (_state == PLAYING && _dotsLeft == 0) {
                _state = WIN; _stateMs = millis();
            }
            break;
        }

        case DEAD:
            if (millis() - _stateMs > 1500UL) {
                _pCol = P0C; _pRow = P0R; _pDir = 0; _nDir = 0;
                for (int i = 0; i < NG; i++) { _gCol[i] = G0C[i]; _gRow[i] = G0R[i]; _gDir[i] = 1; }
                _frightMs = 0;
                _state = PLAYING;
            }
            break;

        case WIN:
            if (millis() - _stateMs > 2000UL || btnPressed) {
                _pCol = P0C; _pRow = P0R; _pDir = 0; _nDir = 0;
                for (int i = 0; i < NG; i++) { _gCol[i] = G0C[i]; _gRow[i] = G0R[i]; _gDir[i] = 1; }
                _frightMs = 0;
                _score = 0;
                initDots(); clrDot(_pCol, _pRow);
                _state = PLAYING;
            }
            break;

        case GAME_OVER:
            if (millis() - _stateMs > 3000UL || btnPressed) return false;
            break;
    }

    render(d);
    return true;
}

// ─── Sprites 8×8 (bit7 = col 0) ─────────────────────────────────────────────
static const uint8_t PAC_CLOSED[] = {0x3C,0x7E,0xFF,0xFF,0xFF,0x7E,0x3C,0x00};
static const uint8_t PAC_R[]      = {0x3C,0x7E,0xFC,0xF0,0xFC,0x7E,0x3C,0x00};
static const uint8_t PAC_L[]      = {0x3C,0x7E,0x3F,0x0F,0x3F,0x7E,0x3C,0x00};
static const uint8_t PAC_D[]      = {0x3C,0x7E,0xFF,0xFF,0xFF,0x7E,0x42,0x00};
static const uint8_t PAC_U[]      = {0x42,0x7E,0xFF,0xFF,0xFF,0x7E,0x3C,0x00};
static const uint8_t GHOST_N[]    = {0x7E,0xFF,0xFF,0xDD,0xFF,0xFF,0xDB,0x00};
static const uint8_t GHOST_S[]    = {0x7E,0xFF,0xFF,0xFF,0xFF,0xAB,0xDB,0x00};

// ─── Render ───────────────────────────────────────────────────────────────────

void PacmanGame::render(Adafruit_SSD1306& d) {
    d.clearDisplay();

    // ── Score bar ─────────────────────────────────────────────────────────────
    d.setTextSize(1); d.setTextColor(SSD1306_WHITE);
    d.setCursor(0, 0); d.print("SC:"); d.print(_score);
    for (int i = 0; i < _lives; i++)
        d.fillRect(110 + i*6, 1, 5, 5, SSD1306_WHITE);

    if (_state == GAME_OVER) {
        d.setTextSize(2); d.setCursor(10, 18); d.print("GAME OVER");
        d.setTextSize(1); d.setCursor(28, 38); d.print("SC: "); d.print(_score);
        d.setCursor(7, 52); d.print("BTN: ricomincia");
        d.display();
        return;
    }

    // ── Maze: walls and dots ──────────────────────────────────────────────────
    bool frightNow  = (millis() - _frightMs < FRIGHT_MS);
    // Power pellets blink when fright is about to end (last 2 s)
    bool pelletShow = !frightNow || ((millis() - _frightMs) < FRIGHT_MS - 2000) || (_anim & 8);

    for (int r = 0; r < 7; r++) {
        for (int c = 0; c < 16; c++) {
            int x = c * 8, y = r * 8 + 8;
            if (isWall(c, r)) {
                auto op = [&](int dc, int dr) {
                    int nc=c+dc, nr=r+dr;
                    return (nc<0||nc>=16||nr<0||nr>=7) || !isWall(nc,nr);
                };
                if (op( 0,-1)) d.fillRect(x,   y,   8, 2, SSD1306_WHITE);
                if (op( 0, 1)) d.fillRect(x,   y+6, 8, 2, SSD1306_WHITE);
                if (op(-1, 0)) d.fillRect(x,   y,   2, 8, SSD1306_WHITE);
                if (op( 1, 0)) d.fillRect(x+6, y,   2, 8, SSD1306_WHITE);
            } else if (hasDot(c, r)) {
                if (MAZE[r * 16 + c] == 'o') {
                    if (pelletShow) d.fillRect(x + 3, y + 3, 2, 2, SSD1306_WHITE);
                } else {
                    d.drawPixel(x + 4, y + 4, SSD1306_WHITE);
                }
            }
        }
    }

    // ── Ghosts ────────────────────────────────────────────────────────────────
    bool showScared = frightNow && (
        (millis() - _frightMs < FRIGHT_MS - 2000) || !(_anim & 8)
    );
    for (int i = 0; i < NG; i++) {
        d.drawBitmap(_gCol[i]*8, _gRow[i]*8+8,
                     showScared ? GHOST_S : GHOST_N, 8, 8, SSD1306_WHITE);
    }

    // ── Pac-Man ───────────────────────────────────────────────────────────────
    bool pacVisible = (_state != DEAD) || ((_anim >> 2) & 1);
    if (pacVisible) {
        const uint8_t* bmp = PAC_CLOSED;
        if (_anim & 8) {
            switch (_pDir) {
                case 0: bmp = PAC_R; break;
                case 1: bmp = PAC_D; break;
                case 2: bmp = PAC_L; break;
                case 3: bmp = PAC_U; break;
            }
        }
        d.drawBitmap(_pCol*8, _pRow*8+8, bmp, 8, 8, SSD1306_WHITE);
    }

    // ── Overlays ──────────────────────────────────────────────────────────────
    if (_state == READY) {
        d.fillRect(18, 32, 92, 8, SSD1306_BLACK);
        d.setTextSize(1); d.setTextColor(SSD1306_WHITE);
        d.setCursor(22, 33); d.print("BTN: inizia");
    }
    if (_state == WIN) {
        d.fillRect(14, 32, 100, 8, SSD1306_WHITE);
        d.setTextSize(1); d.setTextColor(SSD1306_BLACK);
        d.setCursor(18, 33); d.print("COMPLIMENTI! +lvl");
    }

    d.display();
}
