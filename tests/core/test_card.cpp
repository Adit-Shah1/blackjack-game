#include <catch2/catch_test_macros.hpp>
#include <blackjack/card.h>

using namespace blackjack;

TEST_CASE("Card construction and accessors", "[card]") {
    Card card(Suit::Hearts, Rank::Ace);

    SECTION("suit returns correct suit") {
        REQUIRE(card.suit() == Suit::Hearts);
    }

    SECTION("rank returns correct rank") {
        REQUIRE(card.rank() == Rank::Ace);
    }
}

TEST_CASE("Card baseValue", "[card]") {
    SECTION("Ace has value 11") {
        REQUIRE(Card(Suit::Spades, Rank::Ace).baseValue() == 11);
    }

    SECTION("Two through Nine have face value") {
        REQUIRE(Card(Suit::Hearts, Rank::Two).baseValue() == 2);
        REQUIRE(Card(Suit::Hearts, Rank::Three).baseValue() == 3);
        REQUIRE(Card(Suit::Hearts, Rank::Four).baseValue() == 4);
        REQUIRE(Card(Suit::Hearts, Rank::Five).baseValue() == 5);
        REQUIRE(Card(Suit::Hearts, Rank::Six).baseValue() == 6);
        REQUIRE(Card(Suit::Hearts, Rank::Seven).baseValue() == 7);
        REQUIRE(Card(Suit::Hearts, Rank::Eight).baseValue() == 8);
        REQUIRE(Card(Suit::Hearts, Rank::Nine).baseValue() == 9);
    }

    SECTION("Ten and face cards have value 10") {
        REQUIRE(Card(Suit::Hearts, Rank::Ten).baseValue() == 10);
        REQUIRE(Card(Suit::Hearts, Rank::Jack).baseValue() == 10);
        REQUIRE(Card(Suit::Hearts, Rank::Queen).baseValue() == 10);
        REQUIRE(Card(Suit::Hearts, Rank::King).baseValue() == 10);
    }
}

TEST_CASE("Card isAce", "[card]") {
    REQUIRE(Card(Suit::Spades, Rank::Ace).isAce() == true);
    REQUIRE(Card(Suit::Spades, Rank::King).isAce() == false);
    REQUIRE(Card(Suit::Spades, Rank::Ten).isAce() == false);
}

TEST_CASE("Card isFaceCard", "[card]") {
    REQUIRE(Card(Suit::Spades, Rank::Jack).isFaceCard() == true);
    REQUIRE(Card(Suit::Spades, Rank::Queen).isFaceCard() == true);
    REQUIRE(Card(Suit::Spades, Rank::King).isFaceCard() == true);
    REQUIRE(Card(Suit::Spades, Rank::Ten).isFaceCard() == false);
    REQUIRE(Card(Suit::Spades, Rank::Ace).isFaceCard() == false);
}

TEST_CASE("Card isTenValue", "[card]") {
    REQUIRE(Card(Suit::Spades, Rank::Ten).isTenValue() == true);
    REQUIRE(Card(Suit::Spades, Rank::Jack).isTenValue() == true);
    REQUIRE(Card(Suit::Spades, Rank::Queen).isTenValue() == true);
    REQUIRE(Card(Suit::Spades, Rank::King).isTenValue() == true);
    REQUIRE(Card(Suit::Spades, Rank::Nine).isTenValue() == false);
    REQUIRE(Card(Suit::Spades, Rank::Ace).isTenValue() == false);
}

TEST_CASE("Card toString", "[card]") {
    REQUIRE(Card(Suit::Hearts, Rank::Ace).toString() == "AH");
    REQUIRE(Card(Suit::Spades, Rank::King).toString() == "KS");
    REQUIRE(Card(Suit::Diamonds, Rank::Ten).toString() == "TD");
    REQUIRE(Card(Suit::Clubs, Rank::Two).toString() == "2C");
}

TEST_CASE("Card equality", "[card]") {
    Card c1(Suit::Hearts, Rank::Ace);
    Card c2(Suit::Hearts, Rank::Ace);
    Card c3(Suit::Spades, Rank::Ace);

    REQUIRE(c1 == c2);
    REQUIRE(c1 != c3);
}

TEST_CASE("Card suitChar", "[card]") {
    REQUIRE(Card(Suit::Hearts, Rank::Ace).suitChar() == 'H');
    REQUIRE(Card(Suit::Diamonds, Rank::Ace).suitChar() == 'D');
    REQUIRE(Card(Suit::Clubs, Rank::Ace).suitChar() == 'C');
    REQUIRE(Card(Suit::Spades, Rank::Ace).suitChar() == 'S');
}

TEST_CASE("Card rankChar", "[card]") {
    REQUIRE(Card(Suit::Spades, Rank::Ace).rankChar() == 'A');
    REQUIRE(Card(Suit::Spades, Rank::Two).rankChar() == '2');
    REQUIRE(Card(Suit::Spades, Rank::Ten).rankChar() == 'T');
    REQUIRE(Card(Suit::Spades, Rank::Jack).rankChar() == 'J');
    REQUIRE(Card(Suit::Spades, Rank::Queen).rankChar() == 'Q');
    REQUIRE(Card(Suit::Spades, Rank::King).rankChar() == 'K');
}