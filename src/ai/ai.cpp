// AI layer — strategy implementations and AIController
#include <blackjack/ai.h>
#include <algorithm>
#include <cmath>

namespace blackjack {

// ============================================================================
// Basic Strategy Lookup Tables
// ============================================================================

// Hard totals [5..21] vs dealer upcard [2..11]
// H=Hit, S=Stand, D=Double(if allowed else Hit)
static const char HARD_STRATEGY[17][10] = {
    // vs 2   3   4   5   6   7   8   9   10  A
    /*5*/  {'H','H','H','H','H','H','H','H','H','H'},
    /*6*/  {'H','H','H','H','H','H','H','H','H','H'},
    /*7*/  {'H','H','H','H','H','H','H','H','H','H'},
    /*8*/  {'H','H','H','H','H','H','H','H','H','H'},
    /*9*/  {'H','D','D','D','D','H','H','H','H','H'},
    /*10*/ {'D','D','D','D','D','D','D','D','H','H'},
    /*11*/ {'D','D','D','D','D','D','D','D','D','H'},
    /*12*/ {'H','H','S','S','S','H','H','H','H','H'},
    /*13*/ {'S','S','S','S','S','H','H','H','H','H'},
    /*14*/ {'S','S','S','S','S','H','H','H','H','H'},
    /*15*/ {'S','S','S','S','S','H','H','H','H','H'},
    /*16*/ {'S','S','S','S','S','H','H','H','H','H'},
    /*17*/ {'S','S','S','S','S','S','S','S','S','S'},
    /*18*/ {'S','S','S','S','S','S','S','S','S','S'},
    /*19*/ {'S','S','S','S','S','S','S','S','S','S'},
    /*20*/ {'S','S','S','S','S','S','S','S','S','S'},
    /*21*/ {'S','S','S','S','S','S','S','S','S','S'},
};

// Soft totals [A+2=13 .. A+9=20] vs dealer upcard [2..11]
// H=Hit, S=Stand, D=Double(if allowed else Hit), X=Double(if allowed else Stand)
static const char SOFT_STRATEGY[8][10] = {
    // vs 2   3   4   5   6   7   8   9   10  A
    /*A2=13*/{'H','H','H','D','D','H','H','H','H','H'},
    /*A3=14*/{'H','H','H','D','D','H','H','H','H','H'},
    /*A4=15*/{'H','H','D','D','D','H','H','H','H','H'},
    /*A5=16*/{'H','H','D','D','D','H','H','H','H','H'},
    /*A6=17*/{'H','D','D','D','D','H','H','H','H','H'},
    /*A7=18*/{'S','D','D','D','D','S','S','H','H','H'},
    /*A8=19*/{'S','S','S','S','S','S','S','S','S','S'},
    /*A9=20*/{'S','S','S','S','S','S','S','S','S','S'},
};

// Pair strategy [2..A] vs dealer upcard [2..11]
// P=Split, H=Hit, S=Stand, D=Double(if allowed else Hit)
static const char PAIR_STRATEGY[13][10] = {
    // vs 2   3   4   5   6   7   8   9   10  A
    /*2-2*/{'P','P','P','P','P','P','H','H','H','H'},
    /*3-3*/{'P','P','P','P','P','P','H','H','H','H'},
    /*4-4*/{'H','H','H','P','P','H','H','H','H','H'},
    /*5-5*/{'D','D','D','D','D','D','D','D','H','H'},
    /*6-6*/{'P','P','P','P','P','H','H','H','H','H'},
    /*7-7*/{'P','P','P','P','P','P','H','H','H','H'},
    /*8-8*/{'P','P','P','P','P','P','P','P','P','P'},
    /*9-9*/{'P','P','P','P','P','S','P','P','S','S'},
    /*10-10*/{'S','S','S','S','S','S','S','S','S','S'},
    /*J-J*/{'S','S','S','S','S','S','S','S','S','S'},
    /*Q-Q*/{'S','S','S','S','S','S','S','S','S','S'},
    /*K-K*/{'S','S','S','S','S','S','S','S','S','S'},
    /*A-A*/{'P','P','P','P','P','P','P','P','P','P'},
};

// Helper: get dealer column index (2-11 mapped to 0-9)
static int dealerCol(int upcard) {
    if (upcard >= 2 && upcard <= 11) return upcard - 2;
    return 0;
}

// Map a table char to PlayerAction, checking legality
static PlayerAction mapTableAction(char action, const ActionSet& legal) {
    switch (action) {
        case 'S':
            if (legal.canStand) return PlayerAction::Stand;
            break;
        case 'D':
        case 'X':
            if (legal.canDouble) return PlayerAction::DoubleDown;
            if (action == 'X' && legal.canStand) return PlayerAction::Stand;
            if (legal.canHit) return PlayerAction::Hit;
            break;
        case 'P':
            if (legal.canSplit) return PlayerAction::Split;
            break;
    }
    // Default to Hit or Stand
    if (legal.canHit) return PlayerAction::Hit;
    if (legal.canStand) return PlayerAction::Stand;
    return PlayerAction::Stand;
}

// Basic strategy lookup based on hand composition
static PlayerAction basicStrategyLookup(const GameView& view) {
    int dealer = dealerCol(view.dealerUpcard);

    // Pair strategy — only if we actually hold a pair
    if (view.hasPair && view.playerHandCardCount == 2) {
        int total = view.playerHandValue;
        int pairIdx = -1;
        switch (total) {
            case 4:  pairIdx = 0; break;   // 2-2
            case 6:  pairIdx = 1; break;   // 3-3
            case 8:  pairIdx = 2; break;   // 4-4
            case 10: pairIdx = 3; break;   // 5-5
            case 12: pairIdx = 4; break;   // 6-6
            case 14: pairIdx = 5; break;   // 7-7
            case 16: pairIdx = 6; break;   // 8-8
            case 18: pairIdx = 7; break;   // 9-9
            case 20: pairIdx = 8; break;   // 10-10
            case 22: pairIdx = 12; break;  // A-A (soft 12)
        }
        if (pairIdx >= 0 && pairIdx < 13) {
            char action = PAIR_STRATEGY[pairIdx][dealer];
            if (action == 'P') return PlayerAction::Split;
            if (action == 'D') return PlayerAction::DoubleDown;
            if (action == 'S') return PlayerAction::Stand;
            if (action == 'H') return PlayerAction::Hit;
        }
    }

    // Soft totals
    if (view.playerHandIsSoft) {
        int softTotal = view.playerHandValue;
        int softIdx = -1;
        switch (softTotal) {
            case 13: softIdx = 0; break;
            case 14: softIdx = 1; break;
            case 15: softIdx = 2; break;
            case 16: softIdx = 3; break;
            case 17: softIdx = 4; break;
            case 18: softIdx = 5; break;
            case 19: softIdx = 6; break;
            case 20: softIdx = 7; break;
        }
        if (softIdx >= 0) {
            char action = SOFT_STRATEGY[softIdx][dealer];
            if (action == 'S') return PlayerAction::Stand;
            if (action == 'D') return PlayerAction::DoubleDown;
            if (action == 'X') return PlayerAction::DoubleDown; // double else stand
            return PlayerAction::Hit;
        }
        // Soft 21: always stand
        if (softTotal >= 21) return PlayerAction::Stand;
    }

    // Hard totals
    int hardTotal = view.playerHandValue;
    int hardIdx = -1;
    if (hardTotal >= 5 && hardTotal <= 21) {
        hardIdx = hardTotal - 5;
    }
    if (hardIdx >= 0 && hardIdx < 17) {
        char action = HARD_STRATEGY[hardIdx][dealer];
        if (action == 'S') return PlayerAction::Stand;
        if (action == 'D') return PlayerAction::DoubleDown;
        return PlayerAction::Hit;
    }

    // Fallback
    if (hardTotal >= 17) return PlayerAction::Stand;
    return PlayerAction::Hit;
}

// ============================================================================
// Basic Strategy
// ============================================================================

PlayerAction BasicStrategy::decideAction(const GameView& view,
                                         const ActionSet& legalActions) {
    PlayerAction rec = basicStrategyLookup(view);
    return mapTableAction(
        (rec == PlayerAction::Stand) ? 'S' :
        (rec == PlayerAction::DoubleDown) ? 'D' :
        (rec == PlayerAction::Split) ? 'P' : 'H',
        legalActions);
}

int BasicStrategy::decideBet(const BettingView& view) {
    return view.minBet;
}

bool BasicStrategy::decideInsurance(const InsuranceView& /*view*/) {
    return false;  // Basic strategy: never take insurance
}

// ============================================================================
// Conservative Strategy
// ============================================================================

PlayerAction ConservativeStrategy::decideAction(const GameView& view,
                                                 const ActionSet& legalActions) {
    // Start with basic strategy recommendation
    PlayerAction rec = basicStrategyLookup(view);

    // Deviations for more conservative play
    int total = view.playerHandValue;
    bool soft = view.playerHandIsSoft;

    // Stand on 12+ vs dealer 6 or higher
    if (!soft && total >= 12 && total <= 16 && view.dealerUpcard >= 6) {
        if (legalActions.canStand) return PlayerAction::Stand;
    }

    // Never double on 10 or 11 (avoid risk)
    if (rec == PlayerAction::DoubleDown && (total == 10 || total == 11)) {
        if (legalActions.canStand) return PlayerAction::Stand;
        if (legalActions.canHit) return PlayerAction::Hit;
    }

    // Only split Aces and 8s
    if (rec == PlayerAction::Split && view.playerHandCardCount == 2) {
        // Only keep split for totals 16 (8-8) and soft 12 (A-A)
        if (total != 16 && total != 12) {
            if (legalActions.canStand) return PlayerAction::Stand;
            if (legalActions.canHit) return PlayerAction::Hit;
        }
    }

    return mapTableAction(
        (rec == PlayerAction::Stand) ? 'S' :
        (rec == PlayerAction::DoubleDown) ? 'D' :
        (rec == PlayerAction::Split) ? 'P' : 'H',
        legalActions);
}

int ConservativeStrategy::decideBet(const BettingView& view) {
    // Bet minimum always
    return view.minBet;
}

bool ConservativeStrategy::decideInsurance(const InsuranceView& /*view*/) {
    return false;
}

// ============================================================================
// Aggressive Strategy
// ============================================================================

PlayerAction AggressiveStrategy::decideAction(const GameView& view,
                                              const ActionSet& legalActions) {
    PlayerAction rec = basicStrategyLookup(view);
    int total = view.playerHandValue;
    bool soft = view.playerHandIsSoft;

    // Double on 9 in more situations
    if (!soft && total == 9 && view.dealerUpcard >= 2 && view.dealerUpcard <= 6) {
        if (legalActions.canDouble) return PlayerAction::DoubleDown;
    }

    // Hit soft 18 vs dealer 9, 10, Ace (risky)
    if (soft && total == 18 && view.dealerUpcard >= 9) {
        if (legalActions.canHit) return PlayerAction::Hit;
    }

    // Always split when basic says split
    if (rec == PlayerAction::Split && legalActions.canSplit) {
        return PlayerAction::Split;
    }

    return mapTableAction(
        (rec == PlayerAction::Stand) ? 'S' :
        (rec == PlayerAction::DoubleDown) ? 'D' :
        (rec == PlayerAction::Split) ? 'P' : 'H',
        legalActions);
}

int AggressiveStrategy::decideBet(const BettingView& view) {
    // Bet 2x-3x min bet depending on bankroll
    int bet = view.minBet * 2;
    if (view.bankroll >= view.minBet * 20) {
        bet = view.minBet * 3;
    }
    return std::min(bet, view.bankroll);
}

bool AggressiveStrategy::decideInsurance(const InsuranceView& /*view*/) {
    return false;
}

// ============================================================================
// Card Counter Strategy
// ============================================================================

CardCounterStrategy::CardCounterStrategy(const Shoe* shoe)
    : m_shoe(shoe) {}

float CardCounterStrategy::getTrueCount() const {
    if (!m_shoe) return 0.0f;
    int cardsRemaining = static_cast<int>(m_shoe->remaining());
    int decksRemaining = std::max(1, (cardsRemaining + 51) / 52);
    return m_shoe->getTrueCount(decksRemaining);
}

int CardCounterStrategy::getBetSize(int minBet, int bankroll) const {
    float trueCount = getTrueCount();
    if (trueCount <= 0.0f) return minBet;

    int units = 1 + static_cast<int>(trueCount);
    units = std::min(units, 10);
    int bet = minBet * units;
    bet = std::min(bet, bankroll);
    return bet;
}

PlayerAction CardCounterStrategy::decideAction(const GameView& view,
                                               const ActionSet& legalActions) {
    float trueCount = getTrueCount();
    PlayerAction rec = basicStrategyLookup(view);

    // Deviations from basic strategy based on count
    int total = view.playerHandValue;

    // Stand 16 vs 10 at true count >= 0
    if (!view.playerHandIsSoft && total == 16 && view.dealerUpcard == 10 && trueCount >= 0.0f) {
        if (legalActions.canStand) return PlayerAction::Stand;
    }

    // Stand 15 vs 10 at true count >= +4
    if (!view.playerHandIsSoft && total == 15 && view.dealerUpcard == 10 && trueCount >= 4.0f) {
        if (legalActions.canStand) return PlayerAction::Stand;
    }

    return mapTableAction(
        (rec == PlayerAction::Stand) ? 'S' :
        (rec == PlayerAction::DoubleDown) ? 'D' :
        (rec == PlayerAction::Split) ? 'P' : 'H',
        legalActions);
}

int CardCounterStrategy::decideBet(const BettingView& view) {
    return getBetSize(view.minBet, view.bankroll);
}

bool CardCounterStrategy::decideInsurance(const InsuranceView& /*view*/) {
    float trueCount = getTrueCount();
    return trueCount >= 3.0f;
}

// ============================================================================
// Random Strategy
// ============================================================================

RandomStrategy::RandomStrategy()
    : m_rng(std::random_device{}()) {}

RandomStrategy::RandomStrategy(uint32_t seed)
    : m_rng(seed) {}

PlayerAction RandomStrategy::decideAction(const GameView& view,
                                          const ActionSet& legalActions) {
    std::vector<PlayerAction> legal;
    if (legalActions.canHit) legal.push_back(PlayerAction::Hit);
    if (legalActions.canStand) legal.push_back(PlayerAction::Stand);
    if (legalActions.canDouble) legal.push_back(PlayerAction::DoubleDown);
    if (legalActions.canSplit) legal.push_back(PlayerAction::Split);
    if (legalActions.canSurrender) legal.push_back(PlayerAction::Surrender);

    if (legal.empty()) return PlayerAction::Stand;

    // 60% random, 40% basic strategy
    std::uniform_int_distribution<int> dist(0, 99);
    if (dist(m_rng) < 40) {
        PlayerAction rec = basicStrategyLookup(view);
        char actionChar = (rec == PlayerAction::Stand) ? 'S' :
                          (rec == PlayerAction::DoubleDown) ? 'D' :
                          (rec == PlayerAction::Split) ? 'P' : 'H';
        return mapTableAction(actionChar, legalActions);
    }

    std::uniform_int_distribution<size_t> pick(0, legal.size() - 1);
    return legal[pick(m_rng)];
}

int RandomStrategy::decideBet(const BettingView& view) {
    if (view.minBet >= view.bankroll) return view.minBet;
    std::uniform_int_distribution<int> dist(view.minBet,
        std::min(view.maxBet / 2, view.bankroll));
    return dist(m_rng);
}

bool RandomStrategy::decideInsurance(const InsuranceView& /*view*/) {
    std::uniform_int_distribution<int> dist(0, 1);
    return dist(m_rng) == 1;
}

// ============================================================================
// AI Controller
// ============================================================================

AIController::AIController(std::unique_ptr<IAIStrategy> strategy)
    : m_strategy(std::move(strategy)) {}

std::string AIController::strategyName() const {
    if (m_strategy) return m_strategy->name();
    return "Unknown";
}

GameView AIController::buildGameView(const RoundState& round,
                                      int seatIndex, int handIndex) const {
    GameView view;
    const auto& dealerHand = round.dealer().hand;
    if (!dealerHand.cards().empty()) {
        const Card& upcard = dealerHand.cards()[0];
        view.dealerUpcard = upcard.baseValue();
        if (upcard.isAce()) view.dealerUpcard = 11;
    }

    const auto& seat = round.seats()[seatIndex];
    if (handIndex >= 0 && handIndex < static_cast<int>(seat.hands.size())) {
        const auto& hand = seat.hands[handIndex].hand;
        view.playerHandValue = hand.bestValue();
        view.playerHandIsSoft = hand.isSoft();
        view.playerHandCardCount = static_cast<int>(hand.cardCount());
        view.hasPair = hand.canSplit();
    }
    return view;
}

BettingView AIController::buildBettingView(const RoundState& round,
                                            int seatIndex) const {
    BettingView view;
    view.bankroll = round.seats()[seatIndex].bankroll;
    view.minBet = round.rules().minBet;
    view.maxBet = round.rules().maxBet;
    return view;
}

InsuranceView AIController::buildInsuranceView(const RoundState& round,
                                                  int seatIndex) const {
    InsuranceView view;
    view.dealerUpcard = 11;  // Ace
    view.bankroll = round.seats()[seatIndex].bankroll;
    if (!round.seats()[seatIndex].hands.empty()) {
        view.maxInsurance = round.seats()[seatIndex].hands[0].bet.mainBet / 2;
    }
    return view;
}

PlayerAction AIController::validateAction(PlayerAction action,
                                            const ActionSet& legal) const {
    switch (action) {
        case PlayerAction::Hit:
            if (legal.canHit) return action;
            break;
        case PlayerAction::Stand:
            if (legal.canStand) return action;
            break;
        case PlayerAction::DoubleDown:
            if (legal.canDouble) return action;
            break;
        case PlayerAction::Split:
            if (legal.canSplit) return action;
            break;
        case PlayerAction::Surrender:
            if (legal.canSurrender) return action;
            break;
        case PlayerAction::Insurance:
            if (legal.canInsurance) return action;
            break;
    }
    // Fallback
    if (legal.canHit) return PlayerAction::Hit;
    if (legal.canStand) return PlayerAction::Stand;
    return PlayerAction::Stand;
}

PlayerAction AIController::chooseAction(const RoundState& round,
                                        int seatIndex, int handIndex) {
    if (!m_strategy) return PlayerAction::Stand;

    auto legalActions = round.getLegalActions(seatIndex, handIndex);
    GameView view = buildGameView(round, seatIndex, handIndex);
    PlayerAction action = m_strategy->decideAction(view, legalActions);
    return validateAction(action, legalActions);
}

int AIController::chooseBet(const RoundState& round, int seatIndex) {
    if (!m_strategy) return round.rules().minBet;
    BettingView view = buildBettingView(round, seatIndex);
    int bet = m_strategy->decideBet(view);
    // Clamp to valid range
    bet = std::max(view.minBet, std::min(view.maxBet, bet));
    bet = std::min(bet, view.bankroll);
    return bet;
}

bool AIController::chooseInsurance(const RoundState& round, int seatIndex) {
    if (!m_strategy) return false;
    InsuranceView view = buildInsuranceView(round, seatIndex);
    return m_strategy->decideInsurance(view);
}

}  // namespace blackjack
