#include <catch2/catch_test_macros.hpp>
#include <blackjack/hand.h>
#include <blackjack/card.h>

using namespace blackjack;

TEST_CASE("Empty hand", "[hand]") {
    Hand hand;

    SECTION("isEmpty returns true") {
        REQUIRE(hand.isEmpty() == true);
    }

    SECTION("cardCount returns 0") {
        REQUIRE(hand.cardCount() == 0);
    }

    SECTION("bestValue returns 0") {
        REQUIRE(hand.bestValue() == 0);
    }

    SECTION("isBust returns false") {
        REQUIRE(hand.isBust() == false);
    }

    SECTION("isBlackjack returns false") {
        REQUIRE(hand.isBlackjack() == false);
    }
}

TEST_CASE("Single card hand", "[hand]") {
    Hand hand;
    hand.addCard(Card(Suit::Hearts, Rank::Seven));

    REQUIRE(hand.bestValue() == 7);
    REQUIRE(hand.isBust() == false);
    REQUIRE(hand.isBlackjack() == false);
    REQUIRE(hand.canDouble() == false);  // Single card cannot double (need exactly 2)
    REQUIRE(hand.canSplit() == false);
}

TEST_CASE("Hard hand without ace", "[hand]") {
    Hand hand;
    hand.addCard(Card(Suit::Hearts, Rank::Seven));
    hand.addCard(Card(Suit::Spades, Rank::Nine));

    REQUIRE(hand.bestValue() == 16);
    REQUIRE(hand.hardValue() == 16);
    REQUIRE(hand.softValue() == -1);
    REQUIRE(hand.isSoft() == false);
}

TEST_CASE("Soft hand with ace", "[hand]") {
    Hand hand;
    hand.addCard(Card(Suit::Hearts, Rank::Ace));
    hand.addCard(Card(Suit::Spades, Rank::Six));

    REQUIRE(hand.bestValue() == 17);
    REQUIRE(hand.softValue() == 17);
    REQUIRE(hand.hardValue() == 7);
    REQUIRE(hand.isSoft() == true);
}

TEST_CASE("Ace counted as 1 when soft would bust", "[hand]") {
    Hand hand;
    hand.addCard(Card(Suit::Hearts, Rank::Ace));
    hand.addCard(Card(Suit::Spades, Rank::Six));
    hand.addCard(Card(Suit::Diamonds, Rank::Five));

    REQUIRE(hand.bestValue() == 12);
    REQUIRE(hand.isSoft() == false);
}

TEST_CASE("Multiple aces", "[hand]") {
    Hand hand;
    hand.addCard(Card(Suit::Hearts, Rank::Ace));
    hand.addCard(Card(Suit::Spades, Rank::Ace));
    hand.addCard(Card(Suit::Diamonds, Rank::Seven));

    REQUIRE(hand.bestValue() == 19);
    REQUIRE(hand.isSoft() == true);
}

TEST_CASE("Bust detection", "[hand]") {
    Hand hand;
    hand.addCard(Card(Suit::Hearts, Rank::Ten));
    hand.addCard(Card(Suit::Spades, Rank::Eight));
    hand.addCard(Card(Suit::Diamonds, Rank::Five));

    REQUIRE(hand.isBust() == true);
    REQUIRE(hand.bestValue() == 23);
}

TEST_CASE("Blackjack detection", "[hand]") {
    SECTION("Ace + 10-value is blackjack") {
        Hand hand;
        hand.addCard(Card(Suit::Hearts, Rank::Ace));
        hand.addCard(Card(Suit::Spades, Rank::Ten));
        REQUIRE(hand.isBlackjack() == true);
    }

    SECTION("Ace + face card is blackjack") {
        Hand hand;
        hand.addCard(Card(Suit::Hearts, Rank::Ace));
        hand.addCard(Card(Suit::Spades, Rank::Jack));
        REQUIRE(hand.isBlackjack() == true);
    }

    SECTION("Three cards cannot be blackjack") {
        Hand hand;
        hand.addCard(Card(Suit::Hearts, Rank::Ace));
        hand.addCard(Card(Suit::Spades, Rank::Five));
        hand.addCard(Card(Suit::Diamonds, Rank::Five));
        REQUIRE(hand.isBlackjack() == false);
    }

    SECTION("21 is not blackjack") {
        Hand hand;
        hand.addCard(Card(Suit::Hearts, Rank::Ten));
        hand.addCard(Card(Suit::Spades, Rank::Six));
        hand.addCard(Card(Suit::Diamonds, Rank::Five));
        REQUIRE(hand.isBlackjack() == false);
        REQUIRE(hand.bestValue() == 21);
    }
}

TEST_CASE("Split eligibility", "[hand]") {
    SECTION("Two cards with same rank can split") {
        Hand hand;
        hand.addCard(Card(Suit::Hearts, Rank::Eight));
        hand.addCard(Card(Suit::Spades, Rank::Eight));
        REQUIRE(hand.canSplit() == true);
    }

    SECTION("Two cards with different ranks cannot split") {
        Hand hand;
        hand.addCard(Card(Suit::Hearts, Rank::Eight));
        hand.addCard(Card(Suit::Spades, Rank::Nine));
        REQUIRE(hand.canSplit() == false);
    }

    SECTION("Single card cannot split") {
        Hand hand;
        hand.addCard(Card(Suit::Hearts, Rank::Eight));
        REQUIRE(hand.canSplit() == false);
    }

    SECTION("Three cards cannot split") {
        Hand hand;
        hand.addCard(Card(Suit::Hearts, Rank::Eight));
        hand.addCard(Card(Suit::Spades, Rank::Eight));
        hand.addCard(Card(Suit::Diamonds, Rank::Two));
        REQUIRE(hand.canSplit() == false);
    }
}

TEST_CASE("Double down eligibility", "[hand]") {
    SECTION("Two cards can double") {
        Hand hand;
        hand.addCard(Card(Suit::Hearts, Rank::Eight));
        hand.addCard(Card(Suit::Spades, Rank::Three));
        REQUIRE(hand.canDouble() == true);
    }

    SECTION("Single card cannot double") {
        Hand hand;
        hand.addCard(Card(Suit::Hearts, Rank::Eight));
        REQUIRE(hand.canDouble() == false);
    }

    SECTION("Three cards cannot double") {
        Hand hand;
        hand.addCard(Card(Suit::Hearts, Rank::Eight));
        hand.addCard(Card(Suit::Spades, Rank::Three));
        hand.addCard(Card(Suit::Diamonds, Rank::Two));
        REQUIRE(hand.canDouble() == false);
    }
}

TEST_CASE("Hand clear", "[hand]") {
    Hand hand;
    hand.addCard(Card(Suit::Hearts, Rank::Ace));
    hand.addCard(Card(Suit::Spades, Rank::Six));

    hand.clear();

    REQUIRE(hand.isEmpty() == true);
    REQUIRE(hand.cardCount() == 0);
}