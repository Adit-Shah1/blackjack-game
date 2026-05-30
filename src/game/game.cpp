#include <blackjack/round.h>
#include <algorithm>
#include <stdexcept>

namespace blackjack {

RoundState::RoundState(const RuleSet& rules)
    : m_rules(rules)
    , m_shoe(rules.deckCount)
{
    m_seats.resize(1);
    m_seats[0].bankroll = rules.startingBankroll;
    m_seats[0].name = "Player";
}

ActionSet RoundState::getLegalActions(int seatIndex, int handIndex) const {
    ActionSet actions{};
    
    if (seatIndex < 0 || seatIndex >= static_cast<int>(m_seats.size())) {
        return actions;
    }
    
    if (handIndex < 0 || handIndex >= static_cast<int>(m_seats[seatIndex].hands.size())) {
        return actions;
    }

    const auto& hand = m_seats[seatIndex].hands[handIndex];
    
    if (hand.finished || hand.outcome != HandOutcome::Pending) {
        return actions;
    }

    if (m_phase == RoundPhase::InsuranceOffer) {
        // During insurance offer, only insurance is available
        if (m_rules.insuranceAllowed && handIndex == 0 && hand.hand.cardCount() == 2) {
            actions.canInsurance = m_seats[seatIndex].bankroll >= hand.bet.mainBet / 2;
        }
        return actions;
    }

    if (m_phase != RoundPhase::PlayerTurns) {
        return actions;
    }

    if (seatIndex != m_currentSeatIndex) {
        return actions;
    }

    if (handIndex != m_currentHandIndex) {
        return actions;
    }

    actions.canHit = true;
    actions.canStand = true;
    actions.canDouble = hand.hand.canDouble() && m_seats[seatIndex].bankroll >= hand.bet.mainBet;
    actions.canSplit = hand.hand.canSplit() &&
                       m_seats[seatIndex].hands.size() < static_cast<size_t>(m_rules.maxSplitHands) &&
                       m_seats[seatIndex].bankroll >= hand.bet.mainBet;

    if (handIndex == 0 && hand.hand.cardCount() == 2 && !hand.isSplit &&
        m_phase == RoundPhase::PlayerTurns && m_rules.surrenderAllowed) {
        actions.canSurrender = true;
    }

    return actions;
}

bool RoundState::placeBet(int seatIndex, int amount) {
    if (seatIndex < 0 || seatIndex >= static_cast<int>(m_seats.size())) {
        return false;
    }
    
    if (amount < m_rules.minBet || amount > m_rules.maxBet) {
        return false;
    }
    
    if (amount > m_seats[seatIndex].bankroll) {
        return false;
    }

    PlayerHandState handState;
    handState.bet.mainBet = amount;
    m_seats[seatIndex].hands.push_back(handState);
    m_seats[seatIndex].bankroll -= amount;
    
    return true;
}

bool RoundState::hit(int seatIndex, int handIndex) {
    auto actions = getLegalActions(seatIndex, handIndex);
    if (!actions.canHit) {
        return false;
    }

    Card card = m_shoe.draw();
    m_seats[seatIndex].hands[handIndex].hand.addCard(card);

    if (m_seats[seatIndex].hands[handIndex].hand.isBust()) {
        m_seats[seatIndex].hands[handIndex].outcome = HandOutcome::Bust;
        m_seats[seatIndex].hands[handIndex].finished = true;
    }

    return true;
}

bool RoundState::stand(int seatIndex, int handIndex) {
    auto actions = getLegalActions(seatIndex, handIndex);
    if (!actions.canStand) {
        return false;
    }

    m_seats[seatIndex].hands[handIndex].finished = true;
    return true;
}

bool RoundState::doubleDown(int seatIndex, int handIndex) {
    auto actions = getLegalActions(seatIndex, handIndex);
    if (!actions.canDouble) {
        return false;
    }

    int betAmount = m_seats[seatIndex].hands[handIndex].bet.mainBet;
    if (betAmount > m_seats[seatIndex].bankroll) {
        return false;
    }

    m_seats[seatIndex].bankroll -= betAmount;
    m_seats[seatIndex].hands[handIndex].bet.mainBet *= 2;
    m_seats[seatIndex].hands[handIndex].doubled = true;

    Card card = m_shoe.draw();
    m_seats[seatIndex].hands[handIndex].hand.addCard(card);
    m_seats[seatIndex].hands[handIndex].finished = true;

    if (m_seats[seatIndex].hands[handIndex].hand.isBust()) {
        m_seats[seatIndex].hands[handIndex].outcome = HandOutcome::Bust;
    }

    return true;
}

bool RoundState::split(int seatIndex, int handIndex) {
    auto actions = getLegalActions(seatIndex, handIndex);
    if (!actions.canSplit) {
        return false;
    }

    int betAmount = m_seats[seatIndex].hands[handIndex].bet.mainBet;

    if (betAmount > m_seats[seatIndex].bankroll) {
        return false;
    }

    // Split: second card moves to new hand, both hands get one new card
    Card transferredCard = m_seats[seatIndex].hands[handIndex].hand.cards()[1];
    Card firstCard = m_seats[seatIndex].hands[handIndex].hand.cards()[0];

    // Rebuild original hand with just the first card, plus one new drawn card
    m_seats[seatIndex].hands[handIndex].hand.clear();
    m_seats[seatIndex].hands[handIndex].hand.addCard(firstCard);
    m_seats[seatIndex].hands[handIndex].hand.addCard(m_shoe.draw());
    m_seats[seatIndex].hands[handIndex].isSplit = true;

    // New split hand gets the transferred card plus one new drawn card
    PlayerHandState newHand;
    newHand.hand.addCard(transferredCard);
    newHand.hand.addCard(m_shoe.draw());
    newHand.bet.mainBet = betAmount;
    newHand.isSplit = true;

    m_seats[seatIndex].bankroll -= betAmount;
    m_seats[seatIndex].hands.insert(
        m_seats[seatIndex].hands.begin() + handIndex + 1,
        newHand
    );

    // Handle split aces: only one card per hand, auto-stand both
    if (firstCard.isAce()) {
        m_seats[seatIndex].hands[handIndex].finished = true;
        m_seats[seatIndex].hands[handIndex + 1].finished = true;
    }

    return true;
}

bool RoundState::surrender(int seatIndex, int handIndex) {
    auto actions = getLegalActions(seatIndex, handIndex);
    if (!actions.canSurrender) {
        return false;
    }

    if (handIndex != 0 || m_seats[seatIndex].hands[handIndex].isSplit) {
        return false;
    }

    m_seats[seatIndex].hands[handIndex].surrendered = true;
    m_seats[seatIndex].hands[handIndex].finished = true;
    m_seats[seatIndex].hands[handIndex].outcome = HandOutcome::Surrender;

    return true;
}

bool RoundState::takeInsurance(int seatIndex, int amount) {
    if (m_phase != RoundPhase::InsuranceOffer) {
        return false;
    }

    if (!m_rules.insuranceAllowed) {
        return false;
    }

    if (m_dealer.hand.cardCount() != 1 || !m_dealer.hand.cards().front().isAce()) {
        return false;
    }

    if (seatIndex < 0 || seatIndex >= static_cast<int>(m_seats.size())) {
        return false;
    }

    if (m_seats[seatIndex].hands.empty()) {
        return false;
    }

    int maxInsurance = m_seats[seatIndex].hands[0].bet.mainBet / 2;
    if (amount < 0 || amount > maxInsurance || amount > m_seats[seatIndex].bankroll) {
        return false;
    }

    m_seats[seatIndex].hands[0].bet.insuranceBet = amount;
    m_seats[seatIndex].bankroll -= amount;

    return true;
}

bool RoundState::allPlayerHandsFinished() const {
    for (const auto& seat : m_seats) {
        for (const auto& hand : seat.hands) {
            if (!hand.finished) {
                return false;
            }
        }
    }
    return true;
}

bool RoundState::isPlayerTurn() const {
    return m_phase == RoundPhase::PlayerTurns && 
           m_currentSeatIndex >= 0 &&
           m_currentHandIndex >= 0;
}

void RoundState::startRound() {
    m_phase = RoundPhase::WaitingForBets;
    m_dealer.hand.clear();
    m_dealer.holeCardVisible = false;
    
    for (auto& seat : m_seats) {
        seat.hands.clear();
    }
}

void RoundState::dealInitialCards() {
    // Standard dealing order: player card, dealer face-up, player card, dealer hole
    Card playerCard1 = m_shoe.draw();
    Card dealerUpcard = m_shoe.draw();
    Card playerCard2 = m_shoe.draw();
    Card dealerHoleCard = m_shoe.draw();

    m_dealer.hand.addCard(dealerUpcard);
    m_dealer.holeCard = dealerHoleCard;
    m_dealer.holeCardVisible = false;

    m_seats[0].hands[0].hand.addCard(playerCard1);
    m_seats[0].hands[0].hand.addCard(playerCard2);
    m_seats[0].hands[0].finished = false;

    advancePhase();
}

void RoundState::advancePhase() {
    switch (m_phase) {
        case RoundPhase::WaitingForBets:
            if (!m_seats[0].hands.empty()) {
                m_phase = RoundPhase::InitialDeal;
                dealInitialCards();
            }
            break;

        case RoundPhase::InitialDeal:
            // Check for immediate dealer blackjack (natural) - skip insurance if dealer has it
            if (m_dealer.holeCard.isTenValue() && m_dealer.hand.cards().front().isAce()) {
                // Dealer has blackjack - skip insurance offer, go straight to evaluate
                m_phase = RoundPhase::DealerTurn;
                revealDealerHoleCard();
            } else if (m_dealer.hand.cards().front().isAce() && m_rules.insuranceAllowed) {
                m_phase = RoundPhase::InsuranceOffer;
            } else if (m_dealer.hand.cards().front().isAce()) {
                // Dealer shows Ace but insurance not allowed - go straight to player turns
                m_phase = RoundPhase::PlayerTurns;
                m_currentSeatIndex = 0;
                m_currentHandIndex = 0;
            } else {
                m_phase = RoundPhase::PlayerTurns;
                m_currentSeatIndex = 0;
                m_currentHandIndex = 0;
            }
            break;

        case RoundPhase::InsuranceOffer:
            m_phase = RoundPhase::PlayerTurns;
            m_currentSeatIndex = 0;
            m_currentHandIndex = 0;
            break;

        case RoundPhase::PlayerTurns:
            if (allPlayerHandsFinished()) {
                m_phase = RoundPhase::DealerTurn;
                revealDealerHoleCard();
                dealerPlay();
            }
            break;

        case RoundPhase::DealerTurn:
            m_phase = RoundPhase::EvaluateHands;
            break;

        case RoundPhase::EvaluateHands:
            evaluatePayouts();
            m_phase = RoundPhase::Payout;
            break;

        case RoundPhase::Payout:
            m_phase = RoundPhase::RoundComplete;
            break;

        case RoundPhase::RoundComplete:
            break;
    }
}

void RoundState::revealDealerHoleCard() {
    m_dealer.hand.addCard(m_dealer.holeCard);
    m_dealer.holeCardVisible = true;
}

bool RoundState::shouldDealerHit() const {
    int value = m_dealer.hand.bestValue();
    if (value < 17) {
        return true;
    }
    if (value == 17 && m_rules.dealerHitsSoft17 && m_dealer.hand.isSoft()) {
        return true;
    }
    return false;
}

void RoundState::dealerPlay() {
    while (shouldDealerHit()) {
        m_dealer.hand.addCard(m_shoe.draw());
    }
}

int RoundState::evaluateDealerHand() {
    if (!m_dealer.holeCardVisible) {
        revealDealerHoleCard();
    }
    dealerPlay();
    return m_dealer.hand.bestValue();
}

void RoundState::resolveHands() {
    bool dealerBust = m_dealer.hand.isBust();
    bool dealerBlackjack = m_dealer.hand.isBlackjack();

    for (size_t seatIdx = 0; seatIdx < m_seats.size(); ++seatIdx) {
        for (size_t handIdx = 0; handIdx < m_seats[seatIdx].hands.size(); ++handIdx) {
            auto& hand = m_seats[seatIdx].hands[handIdx];

            if (hand.outcome != HandOutcome::Pending) {
                continue;
            }

            if (hand.surrendered) {
                hand.outcome = HandOutcome::Surrender;
                continue;
            }

            if (hand.hand.isBust()) {
                hand.outcome = HandOutcome::Bust;
                continue;
            }

            // Player natural blackjack (first hand, not split)
            if (!hand.isSplit && hand.hand.isBlackjack()) {
                if (dealerBlackjack) {
                    hand.outcome = HandOutcome::Push;
                } else {
                    hand.outcome = HandOutcome::Blackjack;
                }
                continue;
            }

            // Dealer has blackjack but player doesn't
            if (dealerBlackjack) {
                hand.outcome = HandOutcome::Lose;
                continue;
            }

            if (dealerBust) {
                hand.outcome = HandOutcome::Win;
            } else if (hand.hand.bestValue() > m_dealer.hand.bestValue()) {
                hand.outcome = HandOutcome::Win;
            } else if (hand.hand.bestValue() < m_dealer.hand.bestValue()) {
                hand.outcome = HandOutcome::Lose;
            } else {
                hand.outcome = HandOutcome::Push;
            }
        }
    }
}

void RoundState::evaluatePayouts() {
    resolveHands();

    for (size_t seatIdx = 0; seatIdx < m_seats.size(); ++seatIdx) {
        for (size_t handIdx = 0; handIdx < m_seats[seatIdx].hands.size(); ++handIdx) {
            int payout = calculatePayout(seatIdx, handIdx);
            m_seats[seatIdx].bankroll += payout;
        }
    }
}

int RoundState::calculatePayout(int seatIndex, int handIndex) {
    const auto& hand = m_seats[seatIndex].hands[handIndex];
    int returnAmount = 0;

    switch (hand.outcome) {
        case HandOutcome::Win:
            returnAmount = hand.bet.mainBet * 2;
            break;
        case HandOutcome::Blackjack:
            returnAmount = hand.bet.mainBet + hand.bet.mainBet * m_rules.blackjackPayoutNumerator / m_rules.blackjackPayoutDenominator;
            break;
        case HandOutcome::Push:
            returnAmount = hand.bet.mainBet;
            break;
        case HandOutcome::Surrender:
            returnAmount = hand.bet.mainBet / 2;
            break;
        default:
            break;
    }

    // Insurance payout: if dealer has blackjack, insurance pays 2:1
    if (hand.bet.insuranceBet > 0 && m_dealer.hand.isBlackjack()) {
        returnAmount += hand.bet.insuranceBet * 3;  // 2:1 payout + return of bet
    }

    return returnAmount;
}

void RoundState::nextHand() {
    m_currentHandIndex++;
    if (m_currentHandIndex >= static_cast<int>(m_seats[m_currentSeatIndex].hands.size())) {
        m_currentHandIndex = -1;
        m_currentSeatIndex = -1;
    }
}

void RoundState::nextSeat() {
    m_currentSeatIndex++;
    if (m_currentSeatIndex >= static_cast<int>(m_seats.size())) {
        m_currentSeatIndex = -1;
    } else {
        m_currentHandIndex = 0;
    }
}

std::string toString(RoundPhase phase) {
    switch (phase) {
        case RoundPhase::WaitingForBets: return "WaitingForBets";
        case RoundPhase::InitialDeal: return "InitialDeal";
        case RoundPhase::InsuranceOffer: return "InsuranceOffer";
        case RoundPhase::PlayerTurns: return "PlayerTurns";
        case RoundPhase::DealerTurn: return "DealerTurn";
        case RoundPhase::EvaluateHands: return "EvaluateHands";
        case RoundPhase::Payout: return "Payout";
        case RoundPhase::RoundComplete: return "RoundComplete";
    }
    return "Unknown";
}

std::string toString(HandOutcome outcome) {
    switch (outcome) {
        case HandOutcome::Pending: return "Pending";
        case HandOutcome::Win: return "Win";
        case HandOutcome::Lose: return "Lose";
        case HandOutcome::Push: return "Push";
        case HandOutcome::Blackjack: return "Blackjack";
        case HandOutcome::Bust: return "Bust";
        case HandOutcome::Surrender: return "Surrender";
    }
    return "Unknown";
}

std::string toString(PlayerAction action) {
    switch (action) {
        case PlayerAction::Hit: return "Hit";
        case PlayerAction::Stand: return "Stand";
        case PlayerAction::DoubleDown: return "DoubleDown";
        case PlayerAction::Split: return "Split";
        case PlayerAction::Surrender: return "Surrender";
        case PlayerAction::Insurance: return "Insurance";
    }
    return "Unknown";
}

RoundPhase roundPhaseFromString(const std::string& str) {
    if (str == "WaitingForBets") return RoundPhase::WaitingForBets;
    if (str == "InitialDeal") return RoundPhase::InitialDeal;
    if (str == "InsuranceOffer") return RoundPhase::InsuranceOffer;
    if (str == "PlayerTurns") return RoundPhase::PlayerTurns;
    if (str == "DealerTurn") return RoundPhase::DealerTurn;
    if (str == "EvaluateHands") return RoundPhase::EvaluateHands;
    if (str == "Payout") return RoundPhase::Payout;
    if (str == "RoundComplete") return RoundPhase::RoundComplete;
    return RoundPhase::WaitingForBets;
}

HandOutcome handOutcomeFromString(const std::string& str) {
    if (str == "Pending") return HandOutcome::Pending;
    if (str == "Win") return HandOutcome::Win;
    if (str == "Lose") return HandOutcome::Lose;
    if (str == "Push") return HandOutcome::Push;
    if (str == "Blackjack") return HandOutcome::Blackjack;
    if (str == "Bust") return HandOutcome::Bust;
    if (str == "Surrender") return HandOutcome::Surrender;
    return HandOutcome::Pending;
}

}  // namespace blackjack