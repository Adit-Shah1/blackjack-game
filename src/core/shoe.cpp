#include <blackjack/shoe.h>
#include <algorithm>
#include <cmath>

namespace blackjack {

Shoe::Shoe(int deckCount, uint32_t seed)
    : m_deckCount(deckCount)
    , m_runningCount(0)
    , m_reshuffleThreshold(0.25f)
    , m_rng(seed)
{
    createDeck();
    shuffle();
}

void Shoe::createDeck() {
    m_cards.clear();
    m_cards.reserve(m_deckCount * 52);

    for (int deck = 0; deck < m_deckCount; ++deck) {
        for (int suit = 0; suit < 4; ++suit) {
            for (int rank = 0; rank < 13; ++rank) {
                m_cards.emplace_back(static_cast<Suit>(suit), static_cast<Rank>(rank));
            }
        }
    }
}

void Shoe::shuffle() {
    std::shuffle(m_cards.begin(), m_cards.end(), m_rng);
    m_runningCount = 0;
}

Card Shoe::draw() {
    if (m_cards.empty()) {
        createDeck();
        shuffle();
    }
    Card card = m_cards.back();
    m_cards.pop_back();
    updateCount(card);
    return card;
}

bool Shoe::needsReshuffle() const {
    size_t totalCards = m_deckCount * 52;
    size_t cardsRemaining = m_cards.size();
    float penetration = static_cast<float>(cardsRemaining) / static_cast<float>(totalCards);
    return penetration < m_reshuffleThreshold;
}

void Shoe::updateCount(const Card& card) {
    if (card.isAce() || card.isTenValue()) {
        m_runningCount--;
    } else if (card.baseValue() >= 2 && card.baseValue() <= 6) {
        m_runningCount++;
    }
}

float Shoe::getTrueCount(int decksRemaining) const {
    if (decksRemaining <= 0) {
        return 0.0f;
    }
    return static_cast<float>(m_runningCount) / static_cast<float>(decksRemaining);
}

}  // namespace blackjack