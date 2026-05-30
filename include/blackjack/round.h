#pragma once

#include "card.h"
#include "hand.h"
#include "rules.h"
#include "shoe.h"
#include <array>
#include <optional>
#include <string>
#include <vector>

namespace blackjack {

enum class HandOutcome { Pending, Win, Lose, Push, Blackjack, Bust, Surrender };
enum class PlayerAction { Hit, Stand, DoubleDown, Split, Surrender, Insurance };
enum class RoundPhase {
    WaitingForBets,
    InitialDeal,
    InsuranceOffer,
    PlayerTurns,
    DealerTurn,
    EvaluateHands,
    Payout,
    RoundComplete
};

struct Bet {
    int mainBet = 0;
    int insuranceBet = 0;
};

struct PlayerHandState {
    Hand hand;
    Bet bet;
    bool doubled = false;
    bool surrendered = false;
    bool isSplit = false;
    bool finished = false;
    HandOutcome outcome = HandOutcome::Pending;
};

struct ParticipantState {
    std::string name;
    int bankroll;
    std::vector<PlayerHandState> hands;
    int avatarId = 0;

    void reset() {
        bankroll = 0;
        hands.clear();
    }
};

struct DealerState {
    Hand hand;
    Card holeCard;
    bool holeCardVisible = false;
};

struct ActionSet {
    bool canHit = false;
    bool canStand = false;
    bool canDouble = false;
    bool canSplit = false;
    bool canSurrender = false;
    bool canInsurance = false;
};

class RoundState {
public:
    explicit RoundState(const RuleSet& rules);

    const RuleSet& rules() const { return m_rules; }
    Shoe& shoe() { return m_shoe; }
    const Shoe& shoe() const { return m_shoe; }
    DealerState& dealer() { return m_dealer; }
    const DealerState& dealer() const { return m_dealer; }
    std::vector<ParticipantState>& seats() { return m_seats; }
    const std::vector<ParticipantState>& seats() const { return m_seats; }
    int& currentSeatIndex() { return m_currentSeatIndex; }
    int currentSeatIndex() const { return m_currentSeatIndex; }
    int& currentHandIndex() { return m_currentHandIndex; }
    int currentHandIndex() const { return m_currentHandIndex; }
    RoundPhase& phase() { return m_phase; }
    RoundPhase phase() const { return m_phase; }

    ActionSet getLegalActions(int seatIndex, int handIndex) const;
    bool placeBet(int seatIndex, int amount);
    bool hit(int seatIndex, int handIndex);
    bool stand(int seatIndex, int handIndex);
    bool doubleDown(int seatIndex, int handIndex);
    bool split(int seatIndex, int handIndex);
    bool surrender(int seatIndex, int handIndex);
    bool takeInsurance(int seatIndex, int amount);

    void startRound();
    void advancePhase();
    void nextHand();
    void nextSeat();

    bool isPlayerTurn() const;
    bool allPlayerHandsFinished() const;
    int evaluateDealerHand();
    void resolveHands();
    void evaluatePayouts();

private:
    RuleSet m_rules;
    Shoe m_shoe;
    DealerState m_dealer;
    std::vector<ParticipantState> m_seats;
    int m_currentSeatIndex = -1;
    int m_currentHandIndex = -1;
    RoundPhase m_phase = RoundPhase::WaitingForBets;

    void dealInitialCards();
    void revealDealerHoleCard();
    bool shouldDealerHit() const;
    void dealerPlay();
    int calculatePayout(int seatIndex, int handIndex);
};

std::string toString(RoundPhase phase);
std::string toString(HandOutcome outcome);
std::string toString(PlayerAction action);

RoundPhase roundPhaseFromString(const std::string& str);
HandOutcome handOutcomeFromString(const std::string& str);

}  // namespace blackjack