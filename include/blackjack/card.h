#pragma once

#include <ostream>
#include <string>

namespace blackjack {

enum class Suit { Hearts, Diamonds, Clubs, Spades };

enum class Rank {
    Two, Three, Four, Five, Six, Seven, Eight, Nine, Ten,
    Jack, Queen, King, Ace
};

class Card {
public:
    Card() = default;
    Card(Suit suit, Rank rank);

    Suit suit() const { return m_suit; }
    Rank rank() const { return m_rank; }

    int baseValue() const;
    bool isFaceCard() const;
    bool isAce() const;
    bool isTenValue() const;

    std::string toString() const;
    char suitChar() const;
    char rankChar() const;

private:
    Suit m_suit = Suit::Hearts;
    Rank m_rank = Rank::Ace;
};

bool operator==(const Card& lhs, const Card& rhs);
std::ostream& operator<<(std::ostream& os, const Card& card);
std::ostream& operator<<(std::ostream& os, Suit suit);
std::ostream& operator<<(std::ostream& os, Rank rank);

}  // namespace blackjack