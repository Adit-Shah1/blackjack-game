#pragma once

#include "card.h"
#include <vector>

namespace blackjack {

class Hand {
public:
    void addCard(const Card& card);
    void clear();

    const std::vector<Card>& cards() const { return m_cards; }

    int bestValue() const;
    int hardValue() const;
    int softValue() const;
    bool isSoft() const;

    bool isBlackjack() const;
    bool isBust() const;
    bool canSplit() const;
    bool canDouble() const;

    size_t cardCount() const { return m_cards.size(); }
    bool isEmpty() const { return m_cards.empty(); }

private:
    std::vector<Card> m_cards;

    int calculateValue(bool countAceAsEleven) const;
    bool hasSoftAce() const;
    bool hasAce() const;
};

}  // namespace blackjack