#include "BlackjackGame.h"
#include <Arduino.h>
#include <string.h>
#include <stdio.h>

static const char RANKS[] = "A23456789TJQK";
static const char SUITS[] = "HCDS";

// ── begin / setPhase ─────────────────────────────────────────────────────────

void BlackjackGame::begin() {
    _purse     = START_PURSE;
    _prevTiltL = false;
    _prevTiltR = false;
    setPhase(Phase::SPLASH);
}

void BlackjackGame::setPhase(Phase p) {
    _phase  = p;
    _timer  = 0;
    _cursor = 0;
}

// ── update ────────────────────────────────────────────────────────────────────

bool BlackjackGame::update(float dt, float gravX, float /*gravY*/,
                           bool /*shake*/, bool btnPressed, Adafruit_SSD1306& d) {
    uint32_t dtMs = (uint32_t)(dt * 1000.0f);
    _timer     += dtMs;
    _flashTick += dtMs;

    bool jL = justLeft(gravX);
    bool jR = justRight(gravX);

    switch (_phase) {

    case Phase::SPLASH:
        if (_timer >= SPLASH_MS) setPhase(Phase::TITLE);
        break;

    case Phase::TITLE:
        if (btnPressed) startHand();
        break;

    // ── BET: tilt destra = +10, tilt sinistra = -10, btn = conferma ──────────
    case Phase::BET: {
        uint16_t maxBet = (uint16_t)min((int16_t)200, _purse);
        if (jR) {
            if (_betInit + 10u <= maxBet) _betInit += 10u;
            else                          _betInit  = maxBet;
        }
        if (jL) {
            if (_betInit >= 10u) _betInit -= 10u;
            else                 _betInit  = 0u;
        }
        if (btnPressed && _betInit > 0) {
            _h1.bet  = _betInit;
            _purse  -= (int16_t)_betInit;
            setPhase(Phase::DEAL);
            _dealStep = 0;
        }
        break;
    }

    // ── DEAL ─────────────────────────────────────────────────────────────────
    case Phase::DEAL:
        if (_timer >= DEAL_MS) {
            _timer = 0;
            switch (_dealStep) {
            case 0: _h1.cards[_h1.count++] = drawDeckCard(); break;
            case 1: _dealer[_dealerCnt++]   = drawDeckCard(); break;
            case 2: _h1.cards[_h1.count++] = drawDeckCard(); break;
            case 3:
                _dealer[_dealerCnt++] = drawDeckCard();
                afterDeal();
                renderAll(d);
                return false;
            }
            _dealStep++;
        }
        break;

    // ── INSURANCE: tilt destra = +step, tilt sinistra = -step, btn = conferma
    case Phase::INSURANCE: {
        uint16_t half = _h1.bet / 2u;
        uint16_t step = (half >= 10u) ? 10u : (half >= 5u ? 5u : 1u);
        if (jR && step > 0) {
            uint16_t maxIns = (uint16_t)min((int16_t)half, _purse);
            if (_insurance + step <= maxIns) _insurance += step;
            else                             _insurance  = maxIns;
        }
        if (jL) {
            if (_insurance >= step) _insurance -= step;
            else                    _insurance  = 0u;
        }
        if (btnPressed) {
            // confirm: deduct and move to peek
            _purse -= (int16_t)_insurance;
            setPhase(Phase::PEEK);
        }
        break;
    }

    // ── PEEK ─────────────────────────────────────────────────────────────────
    case Phase::PEEK:
        if (_timer >= PEEK_MS) afterPeek();
        break;

    // ── PLAY ─────────────────────────────────────────────────────────────────
    case Phase::PLAY: {
        BjHand& ah = _hand2Turn ? _h2 : _h1;
        bool canDouble = (ah.count == 2 && !ah.doubled && _purse >= (int16_t)ah.bet);
        bool canSplit  = (ah.count == 2 && !_hasSplit &&
                          ah.cards[0]%13u == ah.cards[1]%13u &&
                          _purse >= (int16_t)ah.bet);
        uint8_t maxCur = (uint8_t)(1u + (canDouble?1u:0u) + (canSplit?1u:0u));

        if (jL && _cursor > 0)       _cursor--;
        if (jR && _cursor < maxCur)  _cursor++;

        if (btnPressed) {
            switch (_cursor) {
            case 0: // HIT
                ah.cards[ah.count++] = drawDeckCard();
                if (handValue(ah.cards, ah.count, true) > 21u) {
                    ah.bust = true; advancePlay();
                } else { _cursor = 0; }
                break;
            case 1: // STAND
                ah.stand = true; advancePlay();
                break;
            case 2: // DOUBLE
                if (canDouble) {
                    _purse -= (int16_t)ah.bet;
                    ah.bet *= 2u; ah.doubled = true;
                    ah.cards[ah.count++] = drawDeckCard();
                    if (handValue(ah.cards, ah.count, true) > 21u) ah.bust = true;
                    ah.stand = true; advancePlay();
                }
                break;
            case 3: // SPLIT
                if (canSplit) {
                    _purse -= (int16_t)ah.bet;
                    _h2.reset();
                    _h2.bet       = _h1.bet;
                    _h2.cards[0]  = _h1.cards[1]; _h2.count = 1;
                    _h1.count     = 1;
                    _h1.cards[1]  = drawDeckCard(); _h1.count = 2;
                    _h2.cards[1]  = drawDeckCard(); _h2.count = 2;
                    _hasSplit     = true; _cursor = 0;
                }
                break;
            }
        }
        break;
    }

    // ── PLAY_DEALER ──────────────────────────────────────────────────────────
    case Phase::PLAY_DEALER:
        if (_timer >= DEALER_HIT_MS) {
            _timer = 0;
            if (handValue(_dealer, _dealerCnt, true) < 17u) {
                _dealer[_dealerCnt++] = drawDeckCard();
            } else {
                evaluateHands(); setPhase(Phase::RESULT);
            }
        }
        break;

    case Phase::RESULT:
        if (_timer >= RESULT_MS) setPhase(Phase::END_HAND);
        break;

    // ── END_HAND ─────────────────────────────────────────────────────────────
    case Phase::END_HAND:
        if (jR && _cursor < 1u) _cursor++;
        if (jL && _cursor > 0u) _cursor--;
        if (btnPressed) {
            if (_cursor == 0) {
                if      (_purse >= (int16_t)WIN_PURSE) setPhase(Phase::GAME_WIN);
                else if (_purse <= 0)                  setPhase(Phase::GAME_LOSE);
                else                                   startHand();
            } else {
                setPhase(Phase::TITLE);
            }
        }
        break;

    case Phase::GAME_WIN:
    case Phase::GAME_LOSE:
        if (btnPressed) { _purse = START_PURSE; setPhase(Phase::TITLE); }
        break;
    }

    renderAll(d);
    return false;
}

// ── Edge-triggered tilt ───────────────────────────────────────────────────────

bool BlackjackGame::justLeft(float gravX) {
    bool now  = (gravX < -TILT_THRESH);
    bool edge = now && !_prevTiltL;
    _prevTiltL = now; if (now) _prevTiltR = false;
    return edge;
}

bool BlackjackGame::justRight(float gravX) {
    bool now  = (gravX > TILT_THRESH);
    bool edge = now && !_prevTiltR;
    _prevTiltR = now; if (now) _prevTiltL = false;
    return edge;
}

// ── Deck / hand helpers ───────────────────────────────────────────────────────

uint8_t BlackjackGame::drawDeckCard() {
    uint8_t c;
    do { c = (uint8_t)random(52); } while (_deck[c]);
    _deck[c] = 1;
    return c;
}

uint8_t BlackjackGame::handValue(const uint8_t* c, uint8_t n, bool best) const {
    uint8_t total = 0, aces = 0;
    for (uint8_t i = 0; i < n; i++) {
        uint8_t rank = c[i] % 13u;
        if (rank == 0) { total += 1u; aces++; }
        else           { total += (rank >= 9u) ? 10u : (uint8_t)(rank + 1u); }
    }
    if (best && aces && (uint8_t)(total + 10u) <= 21u) total += 10u;
    return total;
}

bool BlackjackGame::dealerBJ() const {
    if (_dealerCnt < 2) return false;
    uint8_t r0 = _dealer[0] % 13u, r1 = _dealer[1] % 13u;
    return (r0 == 0 && r1 >= 9u) || (r1 == 0 && r0 >= 9u);
}

// ── Game flow ─────────────────────────────────────────────────────────────────

void BlackjackGame::startHand() {
    memset(_deck, 0, sizeof(_deck));
    _dealerCnt = 0; _hasSplit = false; _hand2Turn = false; _showHole = false;
    _betInit = 0; _insurance = 0;
    _res1 = _res2 = Result::NONE; _netWin = 0;
    _h1.reset(); _h2.reset();
    setPhase(Phase::BET);
}

void BlackjackGame::afterDeal() {
    uint8_t up = _dealer[0] % 13u;
    if (up == 0) {
        setPhase(Phase::INSURANCE); _insurance = 0;
    } else if (up >= 9u) {
        setPhase(Phase::PEEK);
    } else {
        if (_h1.isBlackjack()) {
            _showHole = true; evaluateHands(); setPhase(Phase::RESULT);
        } else { setPhase(Phase::PLAY); }
    }
}

void BlackjackGame::afterPeek() {
    if (dealerBJ()) {
        _showHole = true;
        if (_insurance > 0) _purse += (int16_t)(_insurance * 3u);
        evaluateHands(); setPhase(Phase::RESULT);
    } else {
        _showHole = false;
        if (_h1.isBlackjack()) {
            _showHole = true; evaluateHands(); setPhase(Phase::RESULT);
        } else { setPhase(Phase::PLAY); }
    }
}

void BlackjackGame::advancePlay() {
    if (_hasSplit && !_hand2Turn) {
        _hand2Turn = true; _cursor = 0;
    } else {
        _showHole = true;
        bool allBust = _h1.bust && (!_hasSplit || _h2.bust);
        if (allBust) { evaluateHands(); setPhase(Phase::RESULT); }
        else          setPhase(Phase::PLAY_DEALER);
    }
}

void BlackjackGame::evaluateHands() {
    bool dBJ = dealerBJ(); _netWin = 0;

    auto eval = [&](BjHand& h, Result& r) {
        if (h.count == 0) return;
        uint8_t pv  = handValue(h.cards, h.count, true);
        uint8_t dv  = handValue(_dealer, _dealerCnt, true);
        bool    pBJ = h.isBlackjack();
        if (h.bust) {
            r = Result::LOSE; _netWin -= (int16_t)h.bet;
        } else if (dBJ && pBJ) {
            r = Result::PUSH; _purse += (int16_t)h.bet;
        } else if (pBJ) {
            r = Result::BLACKJACK;
            int16_t win = (int16_t)(h.bet + h.bet * 5u / 2u);
            _purse += win; _netWin += win - (int16_t)h.bet;
        } else if (dBJ) {
            r = Result::LOSE; _netWin -= (int16_t)h.bet;
        } else if (dv > 21u) {
            r = Result::WIN; _purse += (int16_t)(h.bet * 2u); _netWin += (int16_t)h.bet;
        } else if (pv > dv) {
            r = Result::WIN; _purse += (int16_t)(h.bet * 2u); _netWin += (int16_t)h.bet;
        } else if (pv < dv) {
            r = Result::LOSE; _netWin -= (int16_t)h.bet;
        } else {
            r = Result::PUSH; _purse += (int16_t)h.bet;
        }
    };

    eval(_h1, _res1);
    if (_hasSplit) eval(_h2, _res2);
}

// ── drawCard (14×18 px, rank in textSize=2) ───────────────────────────────────

void BlackjackGame::drawCard(Adafruit_SSD1306& d, int16_t x, int16_t y,
                              uint8_t card, bool faceUp) const {
    d.drawRect(x, y, CARD_W, CARD_H, SSD1306_WHITE);
    if (!faceUp) {
        // cross-hatch for back of card
        for (int16_t dy = 3; dy < CARD_H - 2; dy += 3)
            d.drawFastHLine(x + 2, y + dy, CARD_W - 4, SSD1306_WHITE);
        return;
    }
    // rank: textSize=2 → 10×14 px rendered (5px font × 2)
    d.setTextSize(2);
    d.setTextColor(SSD1306_WHITE);
    d.setCursor(x + 1, y + 2);
    d.print(RANKS[card % 13u]);
    // suit: textSize=1, angolo in basso a destra (6px wide, 7px tall)
    d.setTextSize(1);
    d.setCursor(x + CARD_W - 7, y + CARD_H - 8);
    d.print(SUITS[card / 13u]);
}

// ── drawHand ──────────────────────────────────────────────────────────────────

void BlackjackGame::drawHand(Adafruit_SSD1306& d,
                              const uint8_t* cards, uint8_t cnt,
                              uint8_t cx, uint8_t y, bool hideLastCard,
                              uint8_t step) const {
    if (cnt == 0) return;
    // shrink step automatically for many cards
    if (cnt > 5 && step > 7u) step = 7u;
    else if (cnt > 4 && step > 8u) step = 8u;
    int16_t tw = (int16_t)(cnt - 1u) * step + CARD_W;
    int16_t lx = (int16_t)cx - tw / 2;
    for (uint8_t i = 0; i < cnt; i++) {
        bool up = !(hideLastCard && i == cnt - 1u);
        drawCard(d, lx + (int16_t)i * step, (int16_t)y, cards[i], up);
    }
}

// ── drawStatsPanel ────────────────────────────────────────────────────────────

void BlackjackGame::drawStatsPanel(Adafruit_SSD1306& d) {
    d.drawFastVLine(STATS_X - 2, 0, SEP_Y, SSD1306_WHITE);
    d.setTextSize(1); d.setTextColor(SSD1306_WHITE);
    const uint8_t sx = STATS_X;
    char buf[8];

    snprintf(buf, sizeof(buf), "$%d", (int)_purse);
    d.setCursor(sx, 0); d.print(buf);

    snprintf(buf, sizeof(buf), "B:%d", (int)_h1.bet);
    d.setCursor(sx, 9); d.print(buf);

    uint8_t dv = (_showHole && _dealerCnt >= 2)
               ? handValue(_dealer, _dealerCnt, true)
               : (_dealerCnt > 0 ? handValue(_dealer, 1u, true) : 0u);
    snprintf(buf, sizeof(buf), "D:%d", (int)dv);
    d.setCursor(sx, 18); d.print(buf);

    if (_h1.count > 0) {
        snprintf(buf, sizeof(buf), "P:%d", (int)handValue(_h1.cards, _h1.count, true));
        d.setCursor(sx, 27); d.print(buf);
    }
    if (_hasSplit && _h2.count > 0) {
        snprintf(buf, sizeof(buf), "2:%d", (int)handValue(_h2.cards, _h2.count, true));
        d.setCursor(sx, 36); d.print(buf);
    }

    if (_phase == Phase::RESULT) {
        bool flash = (_flashTick % 600u) < 300u;
        if (flash) {
            const char* rs = nullptr;
            switch (_res1) {
            case Result::WIN:       rs = "WIN!"; break;
            case Result::BLACKJACK: rs = "BJ!";  break;
            case Result::LOSE:      rs = "LOSE"; break;
            case Result::PUSH:      rs = "PUSH"; break;
            default: break;
            }
            if (rs) { d.setCursor(sx, 55); d.print(rs); }
        }
    }
}

// ── btnLabel / drawBtnBar ─────────────────────────────────────────────────────

void BlackjackGame::btnLabel(Adafruit_SSD1306& d, int16_t x,
                              const char* s, bool sel) const {
    uint8_t w = (uint8_t)(strlen(s) * 6u + 4u);
    if (sel) {
        d.fillRect(x, SEP_Y + 1, w, 11, SSD1306_WHITE);
        d.setTextColor(SSD1306_BLACK);
    } else {
        d.setTextColor(SSD1306_WHITE);
    }
    d.setCursor(x + 2, BTN_Y);
    d.print(s);
    d.setTextColor(SSD1306_WHITE);
}

void BlackjackGame::drawBtnBar(Adafruit_SSD1306& d) const {
    d.drawFastHLine(0, SEP_Y, 128, SSD1306_WHITE);

    switch (_phase) {

    case Phase::PLAY: {
        const BjHand& ah = _hand2Turn ? _h2 : _h1;
        bool canDouble = (ah.count == 2 && !ah.doubled && _purse >= (int16_t)ah.bet);
        bool canSplit  = (ah.count == 2 && !_hasSplit &&
                          ah.cards[0]%13u == ah.cards[1]%13u &&
                          _purse >= (int16_t)ah.bet);
        const char* lbls[4];
        uint8_t cnt = 0;
        lbls[cnt++] = "HIT";
        lbls[cnt++] = "STA";
        if (canDouble) lbls[cnt++] = "DBL";
        if (canSplit)  lbls[cnt++] = "SPL";
        int16_t step = 128 / (int16_t)cnt;
        for (uint8_t i = 0; i < cnt; i++)
            btnLabel(d, (int16_t)i * step, lbls[i], _cursor == i);
        break;
    }

    case Phase::END_HAND:
        btnLabel(d,  0, "CONT", _cursor == 0);
        btnLabel(d, 88, "QUIT", _cursor == 1);
        break;

    case Phase::GAME_WIN:
    case Phase::GAME_LOSE:
        btnLabel(d, 24, "RICOMINCIA", true);
        break;

    default: break;
    }
}

// ── renderBet (full-screen, nessun pannello statistiche) ──────────────────────

void BlackjackGame::renderBet(Adafruit_SSD1306& d) {
    d.setTextSize(1); d.setTextColor(SSD1306_WHITE);

    // Titolo centrato (13 chars × 6 = 78 px → x=25)
    d.setCursor(25, 2); d.print("= BLACKJACK =");

    // Cassa centrata
    char buf[22];
    snprintf(buf, sizeof(buf), "Cassa: $%d", (int)_purse);
    int16_t tw = (int16_t)(strlen(buf) * 6);
    d.setCursor((128 - tw) / 2, 14); d.print(buf);

    // Puntata con frecce (<<  $XX  >>)
    snprintf(buf, sizeof(buf), "<< $%d >>", (int)_betInit);
    tw = (int16_t)(strlen(buf) * 6);
    d.setTextSize(1);
    d.setCursor((128 - tw) / 2, 27); d.print(buf);

    // Limiti puntata
    uint16_t maxBet = (uint16_t)min((int16_t)200, _purse);
    snprintf(buf, sizeof(buf), "max:$%d", (int)maxBet);
    tw = (int16_t)(strlen(buf) * 6);
    d.setCursor((128 - tw) / 2, 37); d.print(buf);

    // Istruzioni
    d.setCursor(1, 47);
    d.print("< > puntata  btn:ok");  // 19 chars × 6 = 114 px, x=1..114 < 128

    // Separatore + barra info
    d.drawFastHLine(0, SEP_Y, 128, SSD1306_WHITE);
    if (_betInit == 0) {
        d.setCursor(20, BTN_Y); d.print("inclina dx per puntare");
    } else {
        snprintf(buf, sizeof(buf), "Puntata: $%d  [btn]:ok", (int)_betInit);
        tw = (int16_t)(strlen(buf) * 6);
        d.setCursor((128 - tw) / 2, BTN_Y); d.print(buf);
    }
}

// ── renderGame (tutte le fasi con carte) ─────────────────────────────────────

void BlackjackGame::renderGame(Adafruit_SSD1306& d) {
    // ── Carte dealer ──
    const uint8_t sx = STATS_X;
    bool hideHole = (!_showHole && _dealerCnt >= 2);
    drawHand(d, _dealer, _dealerCnt, DEALER_CX, DEALER_Y, hideHole, CARD_STEP);

    // ── Carte giocatore ──
    if (_hasSplit) {
        uint8_t sStep = 8u;  // step ridotto in split per stare nello schermo
        drawHand(d, _h1.cards, _h1.count, 20u, PLAYER_Y, false, sStep);
        drawHand(d, _h2.cards, _h2.count, 60u, PLAYER_Y, false, sStep);
        // sottolinea mano attiva
        uint8_t acx = _hand2Turn ? 60u : 20u;
        const BjHand& ah = _hand2Turn ? _h2 : _h1;
        int16_t tw = (int16_t)(ah.count - 1u) * sStep + CARD_W;
        int16_t lx = (int16_t)acx - tw / 2;
        d.drawFastHLine(lx, PLAYER_Y + CARD_H + 1, tw, SSD1306_WHITE);
    } else {
        drawHand(d, _h1.cards, _h1.count, DEALER_CX, PLAYER_Y, false, CARD_STEP);
    }

    // ── Valore mano giocatore (y=47, sotto le carte) ──
    d.setTextSize(1); d.setTextColor(SSD1306_WHITE);
    char buf[8];
    if (_hasSplit) {
        uint8_t v1 = handValue(_h1.cards, _h1.count, true);
        uint8_t v2 = handValue(_h2.cards, _h2.count, true);
        snprintf(buf, sizeof(buf), _h1.bust ? "BST" : "%d", (int)v1);
        d.setCursor(1, 47); d.print(buf);
        snprintf(buf, sizeof(buf), _h2.bust ? "BST" : "%d", (int)v2);
        d.setCursor(52, 47); d.print(buf);
    } else if (_h1.count > 0) {
        uint8_t v1 = handValue(_h1.cards, _h1.count, true);
        if (_h1.bust) { d.setCursor(1, 47); d.print("BUST"); }
        else { snprintf(buf, sizeof(buf), "V:%d", (int)v1); d.setCursor(sx, 45); d.print(buf); }
    }

    // ── Riga messaggio (y=MSG_Y=19, max ~14 chars, entro x=88) ──
    d.setTextSize(1); d.setTextColor(SSD1306_WHITE);
    char msg[16];
    switch (_phase) {
    case Phase::INSURANCE: {
        // mostra importo assicurazione e istruzioni
        snprintf(msg, sizeof(msg), "Ass: $%d", (int)_insurance);
        d.setCursor(1, MSG_Y); d.print(msg);
        break;
    }
    case Phase::PEEK:
        d.setCursor(1, MSG_Y); d.print("Dealer guarda");
        break;
    case Phase::PLAY:
        if (_hasSplit) { d.setCursor(1, MSG_Y); d.print(_hand2Turn ? "Mano 2" : "Mano 1"); }
        break;
    case Phase::PLAY_DEALER:
        d.setCursor(1, MSG_Y); d.print("Dealer gioca..");
        break;
    case Phase::RESULT: {
        bool flash = (_flashTick % 600u) < 300u;
        if (flash) {
            if      (_netWin > 0) snprintf(msg, sizeof(msg), "Vinci $%d!", (int)_netWin);
            else if (_netWin < 0) snprintf(msg, sizeof(msg), "Perdi $%d",  (int)-_netWin);
            else                  snprintf(msg, sizeof(msg), "Pareggio!");
            d.setCursor(1, MSG_Y); d.print(msg);
        }
        break;
    }
    case Phase::END_HAND:
        if      (_netWin > 0) snprintf(msg, sizeof(msg), "+$%d", (int)_netWin);
        else if (_netWin < 0) snprintf(msg, sizeof(msg), "-$%d", (int)-_netWin);
        else                  snprintf(msg, sizeof(msg), "Pareggio");
        d.setCursor(40, MSG_Y); d.print(msg);
        break;
    default: break;
    }

    // ── Pannello statistiche (destra) ──
    drawStatsPanel(d);

    // ── Barra bottoni / separatore ──
    switch (_phase) {
    case Phase::INSURANCE:
        // assicurazione: frecce + istruzione btn
        d.drawFastHLine(0, SEP_Y, 128, SSD1306_WHITE);
        d.setTextColor(SSD1306_WHITE);
        d.setCursor(1,   BTN_Y); d.print("< -");
        d.setCursor(96,  BTN_Y); d.print("+ >");
        d.setCursor(46,  BTN_Y); d.print("[btn]:ok");
        break;
    case Phase::PLAY:
    case Phase::END_HAND:
        drawBtnBar(d);
        break;
    default:
        d.drawFastHLine(0, SEP_Y, 128, SSD1306_WHITE);
        break;
    }
}

// ── renderAll ─────────────────────────────────────────────────────────────────

void BlackjackGame::renderAll(Adafruit_SSD1306& d) {
    d.clearDisplay();
    d.setTextSize(1); d.setTextColor(SSD1306_WHITE);

    switch (_phase) {

    case Phase::SPLASH:
        d.setTextSize(2);
        d.setCursor(10, 10); d.print("BLACKJACK");
        d.setTextSize(1);
        d.setCursor(34, 36); d.print("Press Play!");
        break;

    case Phase::TITLE:
        d.setTextSize(2);
        d.setCursor(10, 4); d.print("BLACKJACK");
        d.setTextSize(1);
        {
            char buf[18];
            snprintf(buf, sizeof(buf), "Cassa: $%d", (int)_purse);
            d.setCursor(20, 28); d.print(buf);
            snprintf(buf, sizeof(buf), "Goal:  $%d", (int)WIN_PURSE);
            d.setCursor(20, 38); d.print(buf);
        }
        d.setCursor(4, 53); d.print("Premi btn: gioca!");
        break;

    case Phase::BET:
        renderBet(d);
        break;

    case Phase::GAME_WIN:
        d.setTextSize(2);
        d.setCursor(10, 4); d.print("HAI VINTO");
        d.setTextSize(1);
        d.setCursor(16, 28); d.print("Sei ricco! $1000");
        drawBtnBar(d);
        break;

    case Phase::GAME_LOSE:
        d.setTextSize(2);
        d.setCursor(4, 4); d.print("GAME OVER");
        d.setTextSize(1);
        d.setCursor(13, 28); d.print("Sei in bancarotta");
        drawBtnBar(d);
        break;

    default:
        renderGame(d);
        break;
    }

    d.display();
}
