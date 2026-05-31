#include <catch2/catch_test_macros.hpp>
#include <blackjack/round.h>
#include <blackjack/rules.h>

using namespace blackjack;

TEST_CASE("RoundState construction", "[round]") {
    RuleSet rules;
    rules.deckCount = 6;
    rules.startingBankroll = 1000;

    RoundState round(rules);

    REQUIRE(round.rules().deckCount == 6);
    REQUIRE(round.phase() == RoundPhase::WaitingForBets);
    REQUIRE(round.seats().size() == 1);
    REQUIRE(round.seats()[0].bankroll == 1000);
}

TEST_CASE("Betting", "[round]") {
    RuleSet rules;
    rules.startingBankroll = 500;
    rules.minBet = 10;
    rules.maxBet = 200;

    RoundState round(rules);

    SECTION("Valid bet") {
        REQUIRE(round.placeBet(0, 50) == true);
        REQUIRE(round.seats()[0].bankroll == 450);
    }

    SECTION("Bet below minimum fails") {
        REQUIRE(round.placeBet(0, 5) == false);
        REQUIRE(round.seats()[0].bankroll == 500);
    }

    SECTION("Bet above maximum fails") {
        REQUIRE(round.placeBet(0, 300) == false);
        REQUIRE(round.seats()[0].bankroll == 500);
    }

    SECTION("Bet exceeding bankroll fails") {
        REQUIRE(round.placeBet(0, 600) == false);
        REQUIRE(round.seats()[0].bankroll == 500);
    }
}

TEST_CASE("Player actions", "[round]") {
    RuleSet rules;
    rules.startingBankroll = 500;
    RoundState round(rules);

    round.startRound();
    round.placeBet(0, 100);

    SECTION("Hit adds card to hand") {
        round.advancePhase();
        round.advancePhase();
        round.advancePhase();

        if (round.isPlayerTurn()) {
            int handIdx = round.currentHandIndex();
            size_t cardsBefore = round.seats()[0].hands[handIdx].hand.cardCount();
            round.hit(0, handIdx);
            REQUIRE(round.seats()[0].hands[handIdx].hand.cardCount() == cardsBefore + 1);
        }
    }

    SECTION("Stand marks hand as finished") {
        round.advancePhase();
        round.advancePhase();
        round.advancePhase();

        if (round.isPlayerTurn()) {
            int handIdx = round.currentHandIndex();
            round.stand(0, handIdx);
            REQUIRE(round.seats()[0].hands[handIdx].finished == true);
        }
    }
}

TEST_CASE("Insurance", "[round]") {
    RuleSet rules;
    rules.startingBankroll = 500;
    rules.insuranceAllowed = true;
    RoundState round(rules);

    round.startRound();
    round.placeBet(0, 100);

    SECTION("Insurance only offered with dealer ace") {
        round.advancePhase();
        auto actions = round.getLegalActions(0, 0);
        if (round.dealer().hand.cards().front().isAce()) {
            REQUIRE(actions.canInsurance == true);
        }
    }
}

TEST_CASE("Split", "[round]") {
    RuleSet rules;
    rules.startingBankroll = 500;
    rules.maxSplitHands = 4;
    RoundState round(rules);

    round.startRound();
    round.placeBet(0, 100);

    SECTION("Can split matching ranks") {
        round.advancePhase();
        round.advancePhase();
        round.advancePhase();

        auto actions = round.getLegalActions(0, 0);
        if (actions.canSplit) {
            REQUIRE(round.split(0, 0) == true);
            REQUIRE(round.seats()[0].hands.size() == 2);
        }
    }
}

TEST_CASE("Double down", "[round]") {
    RuleSet rules;
    rules.startingBankroll = 500;
    RoundState round(rules);

    round.startRound();
    round.placeBet(0, 100);

    SECTION("Can double on two cards") {
        round.advancePhase();
        round.advancePhase();
        round.advancePhase();

        auto actions = round.getLegalActions(0, 0);
        if (actions.canDouble) {
            int bankrollBefore = round.seats()[0].bankroll;
            round.doubleDown(0, 0);
            REQUIRE(round.seats()[0].bankroll == bankrollBefore - 100);
            REQUIRE(round.seats()[0].hands[0].doubled == true);
        }
    }
}

TEST_CASE("Surrender", "[round]") {
    RuleSet rules;
    rules.startingBankroll = 500;
    rules.surrenderAllowed = true;
    RoundState round(rules);

    round.startRound();
    round.placeBet(0, 100);

    SECTION("Can surrender on first hand") {
        round.advancePhase();
        round.advancePhase();
        round.advancePhase();

        auto actions = round.getLegalActions(0, 0);
        if (actions.canSurrender) {
            REQUIRE(round.surrender(0, 0) == true);
            REQUIRE(round.seats()[0].hands[0].surrendered == true);
        }
    }
}

TEST_CASE("Dealer natural blackjack skips player turn", "[round]") {
    RuleSet rules;
    RoundState round(rules);
    round.startRound();
    round.placeBet(0, 100);

    round.phase() = RoundPhase::InitialDeal;
    round.dealer().hand.clear();
    round.dealer().hand.addCard(Card(Suit::Spades, Rank::Ace));
    round.dealer().holeCard = Card(Suit::Hearts, Rank::King);

    round.advancePhase();

    REQUIRE(round.phase() == RoundPhase::DealerTurn);
    REQUIRE(round.currentSeatIndex() == -1);
}

TEST_CASE("InsuranceOffer phase exposes only insurance", "[round]") {
    RuleSet rules;
    rules.insuranceAllowed = true;
    RoundState round(rules);
    round.startRound();
    round.placeBet(0, 100);

    round.phase() = RoundPhase::InitialDeal;
    round.dealer().hand.clear();
    round.dealer().hand.addCard(Card(Suit::Spades, Rank::Ace));
    round.dealer().holeCard = Card(Suit::Hearts, Rank::Seven);

    round.seats()[0].hands[0].hand.clear();
    round.seats()[0].hands[0].hand.addCard(Card(Suit::Clubs, Rank::Ten));
    round.seats()[0].hands[0].hand.addCard(Card(Suit::Diamonds, Rank::Six));

    round.advancePhase();

    REQUIRE(round.phase() == RoundPhase::InsuranceOffer);
    auto actions = round.getLegalActions(0, 0);
    REQUIRE(actions.canInsurance == true);
    REQUIRE(actions.canHit == false);
    REQUIRE(actions.canStand == false);
    REQUIRE(actions.canDouble == false);
    REQUIRE(actions.canSplit == false);
    REQUIRE(actions.canSurrender == false);
}

TEST_CASE("Surrender available only on first decision", "[round]") {
    RuleSet rules;
    rules.surrenderAllowed = true;
    RoundState round(rules);
    round.startRound();
    round.placeBet(0, 100);

    round.phase() = RoundPhase::PlayerTurns;
    round.currentSeatIndex() = 0;
    round.currentHandIndex() = 0;

    round.seats()[0].hands[0].hand.clear();
    round.seats()[0].hands[0].hand.addCard(Card(Suit::Hearts, Rank::Ten));
    round.seats()[0].hands[0].hand.addCard(Card(Suit::Spades, Rank::Six));

    auto actionsBefore = round.getLegalActions(0, 0);
    REQUIRE(actionsBefore.canSurrender == true);

    round.seats()[0].hands[0].hand.addCard(Card(Suit::Diamonds, Rank::Two));

    auto actionsAfter = round.getLegalActions(0, 0);
    REQUIRE(actionsAfter.canSurrender == false);
}

TEST_CASE("Double down not available after hit", "[round]") {
    RuleSet rules;
    RoundState round(rules);
    round.startRound();
    round.placeBet(0, 100);

    round.phase() = RoundPhase::PlayerTurns;
    round.currentSeatIndex() = 0;
    round.currentHandIndex() = 0;

    round.seats()[0].hands[0].hand.clear();
    round.seats()[0].hands[0].hand.addCard(Card(Suit::Hearts, Rank::Ten));
    round.seats()[0].hands[0].hand.addCard(Card(Suit::Spades, Rank::Six));

    auto actionsBefore = round.getLegalActions(0, 0);
    REQUIRE(actionsBefore.canDouble == true);

    round.seats()[0].hands[0].hand.addCard(Card(Suit::Diamonds, Rank::Two));

    auto actionsAfter = round.getLegalActions(0, 0);
    REQUIRE(actionsAfter.canDouble == false);
}

TEST_CASE("Split not available with 3 cards", "[round]") {
    RuleSet rules;
    RoundState round(rules);
    round.startRound();
    round.placeBet(0, 100);

    round.phase() = RoundPhase::PlayerTurns;
    round.currentSeatIndex() = 0;
    round.currentHandIndex() = 0;

    round.seats()[0].hands[0].hand.clear();
    round.seats()[0].hands[0].hand.addCard(Card(Suit::Hearts, Rank::Eight));
    round.seats()[0].hands[0].hand.addCard(Card(Suit::Spades, Rank::Eight));
    round.seats()[0].hands[0].hand.addCard(Card(Suit::Diamonds, Rank::Two));

    auto actions = round.getLegalActions(0, 0);
    REQUIRE(actions.canSplit == false);
}

TEST_CASE("Cannot split beyond maxSplitHands", "[round]") {
    RuleSet rules;
    rules.maxSplitHands = 2;
    RoundState round(rules);
    round.startRound();
    round.placeBet(0, 100);

    round.phase() = RoundPhase::PlayerTurns;
    round.currentSeatIndex() = 0;
    round.currentHandIndex() = 0;

    round.seats()[0].hands.clear();
    PlayerHandState h0, h1;
    h0.hand.addCard(Card(Suit::Hearts, Rank::Eight));
    h0.hand.addCard(Card(Suit::Spades, Rank::Eight));
    h1.hand.addCard(Card(Suit::Diamonds, Rank::Eight));
    h1.hand.addCard(Card(Suit::Clubs, Rank::Eight));
    round.seats()[0].hands.push_back(h0);
    round.seats()[0].hands.push_back(h1);

    auto actions = round.getLegalActions(0, 0);
    REQUIRE(actions.canSplit == false);
}

TEST_CASE("Cannot double if bankroll insufficient", "[round]") {
    RuleSet rules;
    rules.startingBankroll = 100;
    RoundState round(rules);
    round.startRound();
    round.placeBet(0, 100);

    round.phase() = RoundPhase::PlayerTurns;
    round.currentSeatIndex() = 0;
    round.currentHandIndex() = 0;

    round.seats()[0].hands[0].hand.clear();
    round.seats()[0].hands[0].hand.addCard(Card(Suit::Hearts, Rank::Five));
    round.seats()[0].hands[0].hand.addCard(Card(Suit::Spades, Rank::Six));

    auto actions = round.getLegalActions(0, 0);
    REQUIRE(actions.canDouble == false);
}

TEST_CASE("All players bust means dealer does not draw extra cards", "[round]") {
    RuleSet rules;
    RoundState round(rules);
    round.startRound();
    round.placeBet(0, 100);

    round.phase() = RoundPhase::PlayerTurns;
    round.currentSeatIndex() = 0;
    round.currentHandIndex() = 0;

    round.seats()[0].hands[0].hand.clear();
    round.seats()[0].hands[0].hand.addCard(Card(Suit::Hearts, Rank::Ten));
    round.seats()[0].hands[0].hand.addCard(Card(Suit::Spades, Rank::Six));
    round.seats()[0].hands[0].hand.addCard(Card(Suit::Diamonds, Rank::Ten));
    round.seats()[0].hands[0].finished = true;
    round.seats()[0].hands[0].outcome = HandOutcome::Bust;

    round.dealer().hand.clear();
    round.dealer().hand.addCard(Card(Suit::Clubs, Rank::Ten));
    round.dealer().holeCard = Card(Suit::Hearts, Rank::Seven);
    round.dealer().holeCardVisible = false;

    round.advancePhase();

    REQUIRE(round.phase() == RoundPhase::DealerTurn);
    REQUIRE(round.dealer().hand.cardCount() == 2);
}

TEST_CASE("Blackjack payout is 3 to 2", "[round]") {
    RuleSet rules;
    rules.blackjackPayoutNumerator = 3;
    rules.blackjackPayoutDenominator = 2;
    RoundState round(rules);
    round.startRound();
    round.placeBet(0, 100);

    round.seats()[0].hands[0].hand.clear();
    round.seats()[0].hands[0].hand.addCard(Card(Suit::Hearts, Rank::Ace));
    round.seats()[0].hands[0].hand.addCard(Card(Suit::Spades, Rank::King));

    round.dealer().hand.clear();
    round.dealer().hand.addCard(Card(Suit::Clubs, Rank::Ten));
    round.dealer().hand.addCard(Card(Suit::Diamonds, Rank::Seven));
    round.dealer().holeCardVisible = true;

    round.evaluatePayouts();

    REQUIRE(round.seats()[0].bankroll == 1150);
}

TEST_CASE("Insurance payout is 2 to 1", "[round]") {
    RuleSet rules;
    rules.insuranceAllowed = true;
    RoundState round(rules);
    round.startRound();
    round.placeBet(0, 100);

    round.seats()[0].hands[0].hand.clear();
    round.seats()[0].hands[0].hand.addCard(Card(Suit::Hearts, Rank::Ten));
    round.seats()[0].hands[0].hand.addCard(Card(Suit::Spades, Rank::Six));

    round.dealer().hand.clear();
    round.dealer().hand.addCard(Card(Suit::Clubs, Rank::Ace));
    round.dealer().hand.addCard(Card(Suit::Diamonds, Rank::King));
    round.dealer().holeCardVisible = true;

    round.seats()[0].hands[0].bet.insuranceBet = 50;
    round.seats()[0].bankroll = 850;

    round.evaluatePayouts();

    REQUIRE(round.seats()[0].bankroll == 1000);
}

TEST_CASE("addSeat creates additional seats", "[round][multiplayer]") {
    RuleSet rules;
    RoundState round(rules);

    REQUIRE(round.seats().size() == 1);
    REQUIRE(round.seats()[0].name == "Player");

    round.addSeat("Alice", 1000);
    round.addSeat("Bob", 1500);

    REQUIRE(round.seats().size() == 3);
    REQUIRE(round.seats()[1].name == "Alice");
    REQUIRE(round.seats()[1].bankroll == 1000);
    REQUIRE(round.seats()[2].name == "Bob");
    REQUIRE(round.seats()[2].bankroll == 1500);
}

TEST_CASE("allSeatsHaveBets requires every seat to bet", "[round][multiplayer]") {
    RuleSet rules;
    RoundState round(rules);
    round.addSeat("Alice", 1000);
    round.addSeat("Bob", 1000);

    REQUIRE(round.allSeatsHaveBets() == false);

    round.placeBet(0, 50);
    REQUIRE(round.allSeatsHaveBets() == false);

    round.placeBet(1, 50);
    REQUIRE(round.allSeatsHaveBets() == false);

    round.placeBet(2, 50);
    REQUIRE(round.allSeatsHaveBets() == true);
}

TEST_CASE("Multi-seat turn order", "[round][multiplayer]") {
    RuleSet rules;
    RoundState round(rules);
    round.addSeat("Alice", 1000);
    round.addSeat("Bob", 1000);

    round.startRound();
    round.placeBet(0, 50);
    round.placeBet(1, 50);
    round.placeBet(2, 50);

    // Force known hands for deterministic testing
    round.phase() = RoundPhase::PlayerTurns;
    round.currentSeatIndex() = 0;
    round.currentHandIndex() = 0;

    round.seats()[0].hands[0].hand.clear();
    round.seats()[0].hands[0].hand.addCard(Card(Suit::Hearts, Rank::Ten));
    round.seats()[0].hands[0].hand.addCard(Card(Suit::Spades, Rank::Six));

    round.seats()[1].hands[0].hand.clear();
    round.seats()[1].hands[0].hand.addCard(Card(Suit::Diamonds, Rank::Ten));
    round.seats()[1].hands[0].hand.addCard(Card(Suit::Clubs, Rank::Seven));

    round.seats()[2].hands[0].hand.clear();
    round.seats()[2].hands[0].hand.addCard(Card(Suit::Hearts, Rank::Nine));
    round.seats()[2].hands[0].hand.addCard(Card(Suit::Spades, Rank::Eight));

    round.dealer().hand.clear();
    round.dealer().hand.addCard(Card(Suit::Clubs, Rank::Ten));
    round.dealer().holeCard = Card(Suit::Diamonds, Rank::Seven);
    round.dealer().holeCardVisible = false;

    // Seat 0 stands
    round.stand(0, 0);
    round.nextHand();
    round.advancePhase();

    REQUIRE(round.currentSeatIndex() == 1);
    REQUIRE(round.currentHandIndex() == 0);

    // Seat 1 stands
    round.stand(1, 0);
    round.nextHand();
    round.advancePhase();

    REQUIRE(round.currentSeatIndex() == 2);
    REQUIRE(round.currentHandIndex() == 0);

    // Seat 2 stands
    round.stand(2, 0);
    round.nextHand();
    round.advancePhase();

    // All done -> dealer turn
    REQUIRE(round.phase() == RoundPhase::DealerTurn);
}

TEST_CASE("Multi-seat payouts are correct", "[round][multiplayer]") {
    RuleSet rules;
    RoundState round(rules);
    round.addSeat("Alice", 1000);
    round.addSeat("Bob", 1000);

    round.startRound();
    round.placeBet(0, 100);
    round.placeBet(1, 100);
    round.placeBet(2, 100);

    // Set up known hands
    round.seats()[0].hands[0].hand.clear();
    round.seats()[0].hands[0].hand.addCard(Card(Suit::Hearts, Rank::Ace));
    round.seats()[0].hands[0].hand.addCard(Card(Suit::Spades, Rank::King));

    round.seats()[1].hands[0].hand.clear();
    round.seats()[1].hands[0].hand.addCard(Card(Suit::Diamonds, Rank::Ten));
    round.seats()[1].hands[0].hand.addCard(Card(Suit::Clubs, Rank::Six));

    round.seats()[2].hands[0].hand.clear();
    round.seats()[2].hands[0].hand.addCard(Card(Suit::Hearts, Rank::Ten));
    round.seats()[2].hands[0].hand.addCard(Card(Suit::Spades, Rank::Five));

    round.dealer().hand.clear();
    round.dealer().hand.addCard(Card(Suit::Clubs, Rank::Ten));
    round.dealer().hand.addCard(Card(Suit::Diamonds, Rank::Eight));
    round.dealer().holeCardVisible = true;

    round.evaluatePayouts();

    // Seat 0: Blackjack (3:2 payout) = 100 + 150 = 250 returned
    REQUIRE(round.seats()[0].bankroll == 1150);
    // Seat 1: 16 vs dealer 18 -> lose
    REQUIRE(round.seats()[1].bankroll == 900);
    // Seat 2: 15 vs dealer 18 -> lose
    REQUIRE(round.seats()[2].bankroll == 900);
}

TEST_CASE("Multi-seat split aces auto-stand", "[round][multiplayer]") {
    RuleSet rules;
    RoundState round(rules);
    round.addSeat("Alice", 1000);

    round.startRound();
    REQUIRE(round.placeBet(0, 50) == true);
    REQUIRE(round.placeBet(1, 50) == true);

    round.phase() = RoundPhase::PlayerTurns;
    round.currentSeatIndex() = 0;
    round.currentHandIndex() = 0;

    round.seats()[0].hands[0].hand.clear();
    round.seats()[0].hands[0].hand.addCard(Card(Suit::Hearts, Rank::Ace));
    round.seats()[0].hands[0].hand.addCard(Card(Suit::Spades, Rank::Ace));

    round.seats()[1].hands[0].hand.clear();
    round.seats()[1].hands[0].hand.addCard(Card(Suit::Hearts, Rank::Ten));
    round.seats()[1].hands[0].hand.addCard(Card(Suit::Spades, Rank::Seven));

    REQUIRE(round.split(0, 0) == true);
    REQUIRE(round.seats()[0].hands.size() == 2);
    REQUIRE(round.seats()[0].hands[0].finished == true);
    REQUIRE(round.seats()[0].hands[1].finished == true);
    REQUIRE(round.seats()[1].hands.size() == 1);
    REQUIRE(round.seats()[1].hands[0].finished == false);
    REQUIRE(round.allPlayerHandsFinished() == false);

    // Next hand should advance to seat 1
    round.nextHand();
    REQUIRE(round.currentHandIndex() == 1);
    round.advancePhase();

    REQUIRE(round.currentSeatIndex() == 1);
    REQUIRE(round.currentHandIndex() == 0);
}