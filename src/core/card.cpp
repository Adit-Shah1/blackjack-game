#include <blackjack/card.h>
#include <ostream>

namespace blackjack {

Card::Card(Suit suit, Rank rank) : m_suit(suit), m_rank(rank) {}

int Card::baseValue() const {
    switch (m_rank) {
        case Rank::Ace:
            return 11;
        case Rank::Two:
            return 2;
        case Rank::Three:
            return 3;
        case Rank::Four:
            return 4;
        case Rank::Five:
            return 5;
        case Rank::Six:
            return 6;
        case Rank::Seven:
            return 7;
        case Rank::Eight:
            return 8;
        case Rank::Nine:
            return 9;
        case Rank::Ten:
        case Rank::Jack:
        case Rank::Queen:
        case Rank::King:
            return 10;
    }
    return 0;
}

bool Card::isFaceCard() const {
    return m_rank == Rank::Jack || m_rank == Rank::Queen || m_rank == Rank::King;
}

bool Card::isAce() const {
    return m_rank == Rank::Ace;
}

bool Card::isTenValue() const {
    return m_rank == Rank::Ten || m_rank == Rank::Jack ||
           m_rank == Rank::Queen || m_rank == Rank::King;
}

char Card::suitChar() const {
    switch (m_suit) {
        case Suit::Hearts:   return 'H';
        case Suit::Diamonds: return 'D';
        case Suit::Clubs:    return 'C';
        case Suit::Spades:   return 'S';
    }
    return '?';
}

char Card::rankChar() const {
    switch (m_rank) {
        case Rank::Two:   return '2';
        case Rank::Three: return '3';
        case Rank::Four:  return '4';
        case Rank::Five:  return '5';
        case Rank::Six:   return '6';
        case Rank::Seven: return '7';
        case Rank::Eight: return '8';
        case Rank::Nine:  return '9';
        case Rank::Ten:   return 'T';
        case Rank::Jack:  return 'J';
        case Rank::Queen: return 'Q';
        case Rank::King:  return 'K';
        case Rank::Ace:   return 'A';
    }
    return '?';
}

std::string Card::toString() const {
    std::string result;
    result += rankChar();
    result += suitChar();
    return result;
}

bool operator==(const Card& lhs, const Card& rhs) {
    return lhs.suit() == rhs.suit() && lhs.rank() == rhs.rank();
}

std::ostream& operator<<(std::ostream& os, const Card& card) {
    return os << card.toString();
}

std::ostream& operator<<(std::ostream& os, Suit suit) {
    switch (suit) {
        case Suit::Hearts:   return os << "Hearts";
        case Suit::Diamonds: return os << "Diamonds";
        case Suit::Clubs:    return os << "Clubs";
        case Suit::Spades:   return os << "Spades";
    }
    return os << "?";
}

std::ostream& operator<<(std::ostream& os, Rank rank) {
    switch (rank) {
        case Rank::Two:   return os << "Two";
        case Rank::Three: return os << "Three";
        case Rank::Four:  return os << "Four";
        case Rank::Five:  return os << "Five";
        case Rank::Six:   return os << "Six";
        case Rank::Seven: return os << "Seven";
        case Rank::Eight: return os << "Eight";
        case Rank::Nine:  return os << "Nine";
        case Rank::Ten:   return os << "Ten";
        case Rank::Jack:  return os << "Jack";
        case Rank::Queen: return os << "Queen";
        case Rank::King:  return os << "King";
        case Rank::Ace:   return os << "Ace";
    }
    return os << "?";
}

}  // namespace blackjack