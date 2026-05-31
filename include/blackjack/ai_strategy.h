#pragma once

#include "round.h"
#include "rules.h"

namespace blackjack {

// View objects — read-only snapshots of game state for AI decisions
struct GameView {
    int dealerUpcard = 2;   // Face-up dealer card value (2–11)
    int playerHandValue = 0;
    bool playerHandIsSoft = false;
    int playerHandCardCount = 0;
    bool hasPair = false;   // true if player holds a pair (2 cards of same rank)
};

struct BettingView {
    int bankroll = 0;
    int minBet = 10;
    int maxBet = 500;
};

struct InsuranceView {
    int dealerUpcard = 11;  // Always Ace if insurance is offered
    int bankroll = 0;
    int maxInsurance = 0;
};

// AI Strategy interface
class IAIStrategy {
public:
    virtual ~IAIStrategy() = default;

    // Called during PlayerTurns phase
    virtual PlayerAction decideAction(const GameView& view,
                                      const ActionSet& legalActions) = 0;

    // Called before the round starts
    virtual int decideBet(const BettingView& view) = 0;

    // Called during InsuranceOffer phase
    virtual bool decideInsurance(const InsuranceView& /*view*/) { return false; }

    virtual std::string name() const = 0;
};

}  // namespace blackjack
