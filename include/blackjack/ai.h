#pragma once

#include "ai_strategy.h"
#include "round.h"
#include <memory>
#include <random>

namespace blackjack {

// ---------------------------------------------------------------------------
// Basic Strategy — mathematically optimal play
// ---------------------------------------------------------------------------
class BasicStrategy : public IAIStrategy {
public:
    std::string name() const override { return "Basic Strategy"; }

    PlayerAction decideAction(const GameView& view,
                              const ActionSet& legalActions) override;
    int decideBet(const BettingView& view) override;
    bool decideInsurance(const InsuranceView& view) override;
};

// ---------------------------------------------------------------------------
// Conservative Strategy — safer deviations from basic strategy
// ---------------------------------------------------------------------------
class ConservativeStrategy : public IAIStrategy {
public:
    std::string name() const override { return "Conservative"; }

    PlayerAction decideAction(const GameView& view,
                              const ActionSet& legalActions) override;
    int decideBet(const BettingView& view) override;
    bool decideInsurance(const InsuranceView& view) override;
};

// ---------------------------------------------------------------------------
// Aggressive Strategy — riskier, higher-variance deviations
// ---------------------------------------------------------------------------
class AggressiveStrategy : public IAIStrategy {
public:
    std::string name() const override { return "Aggressive"; }

    PlayerAction decideAction(const GameView& view,
                              const ActionSet& legalActions) override;
    int decideBet(const BettingView& view) override;
    bool decideInsurance(const InsuranceView& view) override;
};

// ---------------------------------------------------------------------------
// Card Counter Strategy — Hi-Lo counting with bet sizing
// ---------------------------------------------------------------------------
class CardCounterStrategy : public IAIStrategy {
public:
    explicit CardCounterStrategy(const Shoe* shoe);
    std::string name() const override { return "Card Counter"; }

    PlayerAction decideAction(const GameView& view,
                              const ActionSet& legalActions) override;
    int decideBet(const BettingView& view) override;
    bool decideInsurance(const InsuranceView& view) override;

private:
    const Shoe* m_shoe = nullptr;

    float getTrueCount() const;
    int getBetSize(int minBet, int bankroll) const;
};

// ---------------------------------------------------------------------------
// Random Strategy — weighted random from legal actions
// ---------------------------------------------------------------------------
class RandomStrategy : public IAIStrategy {
public:
    RandomStrategy();
    explicit RandomStrategy(uint32_t seed);
    std::string name() const override { return "Random"; }

    PlayerAction decideAction(const GameView& view,
                              const ActionSet& legalActions) override;
    int decideBet(const BettingView& view) override;
    bool decideInsurance(const InsuranceView& view) override;

private:
    std::mt19937 m_rng;
};

// ---------------------------------------------------------------------------
// AI Controller — adapts RoundState for AI strategies
// ---------------------------------------------------------------------------
class AIController {
public:
    explicit AIController(std::unique_ptr<IAIStrategy> strategy);

    PlayerAction chooseAction(const RoundState& round, int seatIndex, int handIndex);
    int chooseBet(const RoundState& round, int seatIndex);
    bool chooseInsurance(const RoundState& round, int seatIndex);

    const IAIStrategy* strategy() const { return m_strategy.get(); }
    std::string strategyName() const;

private:
    std::unique_ptr<IAIStrategy> m_strategy;

    GameView buildGameView(const RoundState& round, int seatIndex, int handIndex) const;
    BettingView buildBettingView(const RoundState& round, int seatIndex) const;
    InsuranceView buildInsuranceView(const RoundState& round, int seatIndex) const;

    // Ensure the chosen action is legal; falls back if not
    PlayerAction validateAction(PlayerAction action,
                                const ActionSet& legal) const;
};

}  // namespace blackjack
