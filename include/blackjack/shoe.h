#pragma once

#include "card.h"
#include <cstddef>
#include <random>

namespace blackjack {

class Shoe {
public:
    explicit Shoe(int deckCount = 6, uint32_t seed = std::random_device{}());

    void shuffle();
    Card draw();
    size_t remaining() const { return m_cards.size(); }
    bool needsReshuffle() const;

    float getTrueCount(int decksRemaining) const;
    int getRunningCount() const { return m_runningCount; }
    void setRunningCount(int count) { m_runningCount = count; }

    const std::vector<Card>& cards() const { return m_cards; }
    void setCards(const std::vector<Card>& cards) { m_cards = cards; }

    void setReshuffleThreshold(float threshold) { m_reshuffleThreshold = threshold; }

private:
    std::vector<Card> m_cards;
    int m_deckCount;
    int m_runningCount;
    float m_reshuffleThreshold;
    std::mt19937 m_rng;

    void createDeck();
    void updateCount(const Card& card);
};

}  // namespace blackjack