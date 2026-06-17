#pragma once
#include <Adafruit_SSD1306.h>
#include <stdint.h>

class BlackjackGame {
public:
    void begin();
    bool update(float dt, float gravX, float gravY,
                bool shake, bool btnPressed, Adafruit_SSD1306& d);

private:
    // ── Constants ────────────────────────────────────────────────────────────
    static constexpr uint16_t START_PURSE    = 500;
    static constexpr uint16_t WIN_PURSE      = 1000;
    static constexpr uint8_t  MAX_CARDS      = 11;
    static constexpr uint8_t  CARD_W         = 17;  // larger cards for readability
    static constexpr uint8_t  CARD_H         = 23;
    static constexpr uint8_t  CARD_STEP      = 17;  // x-step per carte sovrapposte
    static constexpr uint8_t  DEALER_CX      = 36;
    static constexpr uint8_t  DEALER_Y       = 0;
    static constexpr uint8_t  PLAYER_Y       = 28;
    static constexpr uint8_t  MSG_Y          = 55;  // riga messaggio centrale
    static constexpr uint8_t  SEP_Y          = 54;
    static constexpr uint8_t  BTN_Y          = 56;
    static constexpr uint8_t  STATS_X        = 90;
    static constexpr float    TILT_THRESH    = 6.0f;
    static constexpr uint32_t DEAL_MS        = 380;
    static constexpr uint32_t PEEK_MS        = 1500;
    static constexpr uint32_t DEALER_HIT_MS  = 500;
    static constexpr uint32_t RESULT_MS      = 3000;
    static constexpr uint32_t SPLASH_MS      = 2000;

    // ── Fase ─────────────────────────────────────────────────────────────────
    enum class Phase : uint8_t {
        SPLASH, TITLE,
        BET, DEAL, INSURANCE, PEEK,
        PLAY, PLAY_DEALER,
        RESULT, END_HAND,
        GAME_WIN, GAME_LOSE
    };
    enum class Result : uint8_t { NONE, WIN, BLACKJACK, LOSE, PUSH };

    // ── Mano ─────────────────────────────────────────────────────────────────
    struct BjHand {
        uint8_t  cards[MAX_CARDS];
        uint8_t  count   = 0;
        uint16_t bet     = 0;
        bool     stand   = false;
        bool     bust    = false;
        bool     doubled = false;

        void reset() { count=0; bet=0; stand=false; bust=false; doubled=false; }
        bool isBlackjack() const {
            return count == 2 &&
                   ((cards[0]%13 == 0 && cards[1]%13 >= 9) ||
                    (cards[1]%13 == 0 && cards[0]%13 >= 9));
        }
    };

    // ── Stato ────────────────────────────────────────────────────────────────
    Phase    _phase     = Phase::SPLASH;
    uint32_t _timer     = 0;
    uint32_t _flashTick = 0;

    uint8_t  _deck[52];
    uint8_t  _dealer[MAX_CARDS];
    uint8_t  _dealerCnt = 0;
    BjHand   _h1, _h2;

    bool     _hasSplit  = false;
    bool     _hand2Turn = false;
    bool     _showHole  = false;

    int16_t  _purse     = START_PURSE;
    uint16_t _betInit   = 0;   // bet in progress (BET phase)
    uint16_t _insurance = 0;

    Result   _res1      = Result::NONE;
    Result   _res2      = Result::NONE;
    int16_t  _netWin    = 0;

    uint8_t  _cursor    = 0;   // used only in PLAY / END_HAND
    uint8_t  _dealStep  = 0;

    bool     _prevTiltL = false;
    bool     _prevTiltR = false;

    // ── Logica ───────────────────────────────────────────────────────────────
    uint8_t  drawDeckCard();
    uint8_t  handValue(const uint8_t* c, uint8_t n, bool best) const;
    bool     dealerBJ() const;
    void     startHand();
    void     afterDeal();
    void     afterPeek();
    void     advancePlay();
    void     evaluateHands();
    void     setPhase(Phase p);
    bool     justLeft(float gravX);
    bool     justRight(float gravX);

    // ── Rendering ────────────────────────────────────────────────────────────
    void renderAll(Adafruit_SSD1306& d);
    void renderBet(Adafruit_SSD1306& d);
    void renderGame(Adafruit_SSD1306& d);
    void drawCard(Adafruit_SSD1306& d, int16_t x, int16_t y,
                  uint8_t card, bool faceUp) const;
    void drawHand(Adafruit_SSD1306& d,
                  const uint8_t* cards, uint8_t cnt,
                  uint8_t cx, uint8_t y, bool hideLastCard, uint8_t step) const;
    void drawStatsPanel(Adafruit_SSD1306& d);
    void drawBtnBar(Adafruit_SSD1306& d) const;
    void btnLabel(Adafruit_SSD1306& d, int16_t x, const char* s, bool sel) const;
};
