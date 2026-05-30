#include <blackjack/hand.h>
#include <algorithm>
#include <limits>

namespace blackjack {

void Hand::addCard(const Card& card) {
    m_cards.push_back(card);
}

void Hand::clear() {
    m_cards.clear();
}

int Hand::calculateValue(bool countAceAsEleven) const {
    int total = 0;
    int aceCount = 0;

    for (const auto& card : m_cards) {
        if (card.isAce()) {
            aceCount++;
            total += countAceAsEleven ? 11 : 1;
        } else {
            total += card.baseValue();
        }
    }

    while (total > 21 && aceCount > 0) {
        total -= 10;
        aceCount--;
    }

    return total;
}

int Hand::hardValue() const {
    return calculateValue(false);
}

int Hand::softValue() const {
    // If hand has no aces, it's not a soft hand (no ace can be counted as 11)
    if (!hasAce()) {
        return -1;
    }
    int val = calculateValue(true);
    if (val > 21) {
        return -1;
    }
    return val;
}

bool Hand::hasSoftAce() const {
    int hard = hardValue();
    int soft = softValue();
    return soft != -1 && soft != hard;
}

bool Hand::hasAce() const {
    for (const auto& card : m_cards) {
        if (card.isAce()) {
            return true;
        }
    }
    return false;
}

int Hand::bestValue() const {
    int val = calculateValue(true);
    if (val > 21) {
        return calculateValue(false);
    }
    return val;
}

bool Hand::isSoft() const {
    return hasSoftAce();
}

bool Hand::isBlackjack() const {
    return m_cards.size() == 2 && bestValue() == 21;
}

bool Hand::isBust() const {
    return bestValue() > 21;
}

bool Hand::canSplit() const {
    return m_cards.size() == 2 && m_cards[0].rank() == m_cards[1].rank();
}

bool Hand::canDouble() const {
    return m_cards.size() == 2;
}

}  // namespace blackjack