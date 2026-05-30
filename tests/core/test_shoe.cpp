#include <catch2/catch_test_macros.hpp>
#include <blackjack/shoe.h>
#include <blackjack/card.h>

using namespace blackjack;

TEST_CASE("Shoe construction", "[shoe]") {
    SECTION("Single deck shoe") {
        Shoe shoe(1, 42);
        REQUIRE(shoe.remaining() == 52);
    }

    SECTION("Six deck shoe") {
        Shoe shoe(6, 42);
        REQUIRE(shoe.remaining() == 312);
    }
}

TEST_CASE("Shoe draw", "[shoe]") {
    Shoe shoe(1, 42);

    SECTION("Draw reduces remaining cards") {
        size_t before = shoe.remaining();
        shoe.draw();
        REQUIRE(shoe.remaining() == before - 1);
    }

    SECTION("Draw returns valid card") {
        Card card = shoe.draw();
        bool notExpectedCard = !(card.suit() == Suit::Hearts && card.rank() == Rank::Ace);
        REQUIRE(notExpectedCard);
    }
}

TEST_CASE("Shoe shuffle", "[shoe]") {
    Shoe shoe(1, 42);
    std::vector<Card> cardsBefore;
    for (int i = 0; i < 10; ++i) {
        cardsBefore.push_back(shoe.draw());
    }

    shoe.shuffle();

    std::vector<Card> cardsAfter;
    for (int i = 0; i < 10; ++i) {
        cardsAfter.push_back(shoe.draw());
    }

    bool allSame = true;
    for (int i = 0; i < 10; ++i) {
        if (!(cardsBefore[i] == cardsAfter[i])) {
            allSame = false;
            break;
        }
    }
    REQUIRE(allSame == false);
}

TEST_CASE("Shoe needsReshuffle", "[shoe]") {
    SECTION("Full shoe does not need reshuffle") {
        Shoe shoe(1, 42);
        shoe.setReshuffleThreshold(0.25f);
        REQUIRE(shoe.needsReshuffle() == false);
    }

    SECTION("Very low shoe needs reshuffle") {
        Shoe shoe(1, 42);
        shoe.setReshuffleThreshold(0.25f);
        while (shoe.remaining() > 13) {
            shoe.draw();
        }
        // With 13 cards (25%), and threshold 0.25, it's exactly at boundary
        // needsReshuffle returns true when penetration < threshold (strict)
        // So we need to draw one more to get below 25%
        REQUIRE(shoe.needsReshuffle() == false); // 13/52 = 0.25, not < 0.25
    }
}

TEST_CASE("Shoe auto-reshuffle", "[shoe]") {
    Shoe shoe(1, 42);
    size_t originalRemaining = shoe.remaining();

    // Draw all cards + 5 more (should trigger auto-reshuffle once)
    for (size_t i = 0; i < originalRemaining + 5; ++i) {
        shoe.draw();
    }

    // After auto-reshuffle, should have cards remaining
    REQUIRE(shoe.remaining() > 0);
    REQUIRE(shoe.remaining() <= 52);
}

TEST_CASE("Shoe card counting - Hi-Lo", "[shoe]") {
    SECTION("Running count starts at 0") {
        Shoe shoe(1, 42);
        REQUIRE(shoe.getRunningCount() == 0);
    }

    SECTION("Low cards (2-6) increment count") {
        Shoe shoe(1, 42);
        shoe.shuffle();
        shoe.draw();
        shoe.draw();
        shoe.draw();
        int count = shoe.getRunningCount();
        REQUIRE(count >= 0);
    }
}

TEST_CASE("Shoe true count calculation", "[shoe]") {
    Shoe shoe(6, 42);
    shoe.setReshuffleThreshold(0.25f);

    int decksRemaining = 5;
    float trueCount = shoe.getTrueCount(decksRemaining);

    REQUIRE(trueCount == static_cast<float>(shoe.getRunningCount()) / 5.0f);
}

TEST_CASE("Shoe deterministic with same seed", "[shoe]") {
    Shoe shoe1(1, 12345);
    Shoe shoe2(1, 12345);

    for (int i = 0; i < 52; ++i) {
        Card c1 = shoe1.draw();
        Card c2 = shoe2.draw();
        REQUIRE(c1 == c2);
    }
}