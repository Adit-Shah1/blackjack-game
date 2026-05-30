#include <catch2/catch_test_macros.hpp>
#include <blackjack/round.h>
#include <blackjack/card.h>
#include <blackjack/rules.h>

using namespace blackjack;

TEST_CASE("Natural blackjack for player", "[integration]") {
    RuleSet rules;
    rules.startingBankroll = 1000;
    RoundState round(rules);
    round.startRound();
    round.placeBet(0, 100);

    round.seats()[0].hands[0].hand.clear();
    round.seats()[0].hands[0].hand.addCard(Card(Suit::Hearts, Rank::Ace));
    round.seats()[0].hands[0].hand.addCard(Card(Suit::Spades, Rank::King));

    round.dealer().hand.clear();
    round.dealer().hand.addCard(Card(Suit::Clubs, Rank::Nine));
    round.dealer().hand.addCard(Card(Suit::Diamonds, Rank::Seven));
    round.dealer().holeCardVisible = true;

    round.evaluatePayouts();

    REQUIRE(round.seats()[0].hands[0].outcome == HandOutcome::Blackjack);
    REQUIRE(round.seats()[0].bankroll == 1150);
}

TEST_CASE("Natural blackjack for dealer", "[integration]") {
    RuleSet rules;
    rules.startingBankroll = 1000;
    RoundState round(rules);
    round.startRound();
    round.placeBet(0, 100);

    round.dealer().hand.clear();
    round.dealer().hand.addCard(Card(Suit::Spades, Rank::Ace));
    round.dealer().hand.addCard(Card(Suit::Hearts, Rank::Queen));
    round.dealer().holeCardVisible = true;

    round.seats()[0].hands[0].hand.clear();
    round.seats()[0].hands[0].hand.addCard(Card(Suit::Clubs, Rank::Ten));
    round.seats()[0].hands[0].hand.addCard(Card(Suit::Diamonds, Rank::Six));

    round.evaluatePayouts();

    REQUIRE(round.seats()[0].hands[0].outcome == HandOutcome::Lose);
    REQUIRE(round.seats()[0].bankroll == 900);
}

TEST_CASE("Both have natural blackjack", "[integration]") {
    RuleSet rules;
    rules.startingBankroll = 1000;
    RoundState round(rules);
    round.startRound();
    round.placeBet(0, 100);

    round.seats()[0].hands[0].hand.clear();
    round.seats()[0].hands[0].hand.addCard(Card(Suit::Hearts, Rank::Ace));
    round.seats()[0].hands[0].hand.addCard(Card(Suit::Spades, Rank::Ten));

    round.dealer().hand.clear();
    round.dealer().hand.addCard(Card(Suit::Clubs, Rank::Ace));
    round.dealer().hand.addCard(Card(Suit::Diamonds, Rank::Ten));
    round.dealer().holeCardVisible = true;

    round.evaluatePayouts();

    REQUIRE(round.seats()[0].hands[0].outcome == HandOutcome::Push);
    REQUIRE(round.seats()[0].bankroll == 1000);
}

TEST_CASE("Player bust", "[integration]") {
    RuleSet rules;
    rules.startingBankroll = 1000;
    RoundState round(rules);
    round.startRound();
    round.placeBet(0, 100);

    round.seats()[0].hands[0].hand.clear();
    round.seats()[0].hands[0].hand.addCard(Card(Suit::Hearts, Rank::Ten));
    round.seats()[0].hands[0].hand.addCard(Card(Suit::Spades, Rank::Six));
    round.seats()[0].hands[0].hand.addCard(Card(Suit::Diamonds, Rank::Ten));
    round.seats()[0].hands[0].finished = true;

    round.dealer().hand.clear();
    round.dealer().hand.addCard(Card(Suit::Clubs, Rank::Nine));
    round.dealer().hand.addCard(Card(Suit::Clubs, Rank::Eight));
    round.dealer().holeCardVisible = true;

    round.evaluatePayouts();

    REQUIRE(round.seats()[0].hands[0].outcome == HandOutcome::Bust);
    REQUIRE(round.seats()[0].bankroll == 900);
    REQUIRE(round.dealer().hand.cardCount() == 2);
}

TEST_CASE("Dealer bust", "[integration]") {
    RuleSet rules;
    rules.startingBankroll = 1000;
    RoundState round(rules);
    round.startRound();
    round.placeBet(0, 100);

    round.seats()[0].hands[0].hand.clear();
    round.seats()[0].hands[0].hand.addCard(Card(Suit::Hearts, Rank::Ten));
    round.seats()[0].hands[0].hand.addCard(Card(Suit::Spades, Rank::Eight));
    round.seats()[0].hands[0].finished = true;

    round.dealer().hand.clear();
    round.dealer().hand.addCard(Card(Suit::Clubs, Rank::Ten));
    round.dealer().hand.addCard(Card(Suit::Diamonds, Rank::Six));
    round.dealer().hand.addCard(Card(Suit::Clubs, Rank::Ten));
    round.dealer().holeCardVisible = true;

    round.evaluatePayouts();

    REQUIRE(round.seats()[0].hands[0].outcome == HandOutcome::Win);
    REQUIRE(round.seats()[0].bankroll == 1100);
}

TEST_CASE("Insurance wins when dealer has blackjack", "[integration]") {
    RuleSet rules;
    rules.startingBankroll = 1000;
    rules.insuranceAllowed = true;
    RoundState round(rules);
    round.startRound();
    round.placeBet(0, 100);

    round.dealer().hand.clear();
    round.dealer().hand.addCard(Card(Suit::Spades, Rank::Ace));
    round.dealer().hand.addCard(Card(Suit::Hearts, Rank::King));
    round.dealer().holeCardVisible = true;

    round.seats()[0].hands[0].hand.clear();
    round.seats()[0].hands[0].hand.addCard(Card(Suit::Clubs, Rank::Ten));
    round.seats()[0].hands[0].hand.addCard(Card(Suit::Diamonds, Rank::Six));

    round.seats()[0].hands[0].bet.insuranceBet = 50;
    round.seats()[0].bankroll = 850;

    round.evaluatePayouts();

    REQUIRE(round.seats()[0].bankroll == 1000);
}

TEST_CASE("Insurance loses when dealer has no blackjack", "[integration]") {
    RuleSet rules;
    rules.startingBankroll = 1000;
    rules.insuranceAllowed = true;
    RoundState round(rules);
    round.startRound();
    round.placeBet(0, 100);

    round.dealer().hand.clear();
    round.dealer().hand.addCard(Card(Suit::Spades, Rank::Ace));
    round.dealer().hand.addCard(Card(Suit::Hearts, Rank::Seven));
    round.dealer().holeCardVisible = true;

    round.seats()[0].hands[0].hand.clear();
    round.seats()[0].hands[0].hand.addCard(Card(Suit::Clubs, Rank::Ten));
    round.seats()[0].hands[0].hand.addCard(Card(Suit::Diamonds, Rank::Six));

    round.seats()[0].hands[0].bet.insuranceBet = 50;
    round.seats()[0].bankroll = 850;

    round.evaluatePayouts();

    REQUIRE(round.seats()[0].bankroll == 850);
}

TEST_CASE("Split then win one lose one", "[integration]") {
    RuleSet rules;
    rules.startingBankroll = 1000;
    RoundState round(rules);
    round.startRound();
    round.placeBet(0, 100);

    round.seats()[0].hands.clear();

    PlayerHandState h0;
    h0.hand.addCard(Card(Suit::Hearts, Rank::Eight));
    h0.hand.addCard(Card(Suit::Spades, Rank::Ten));
    h0.bet.mainBet = 100;
    h0.isSplit = true;
    round.seats()[0].hands.push_back(h0);

    PlayerHandState h1;
    h1.hand.addCard(Card(Suit::Diamonds, Rank::Eight));
    h1.hand.addCard(Card(Suit::Clubs, Rank::Six));
    h1.bet.mainBet = 100;
    h1.isSplit = true;
    round.seats()[0].hands.push_back(h1);

    round.seats()[0].bankroll = 800;

    round.dealer().hand.clear();
    round.dealer().hand.addCard(Card(Suit::Clubs, Rank::Ten));
    round.dealer().hand.addCard(Card(Suit::Hearts, Rank::Seven));
    round.dealer().holeCardVisible = true;

    round.evaluatePayouts();

    REQUIRE(round.seats()[0].hands[0].outcome == HandOutcome::Win);
    REQUIRE(round.seats()[0].hands[1].outcome == HandOutcome::Lose);
    REQUIRE(round.seats()[0].bankroll == 1000);
}

TEST_CASE("Double down win", "[integration]") {
    RuleSet rules;
    rules.startingBankroll = 1000;
    RoundState round(rules);
    round.startRound();
    round.placeBet(0, 100);

    round.seats()[0].hands[0].hand.clear();
    round.seats()[0].hands[0].hand.addCard(Card(Suit::Hearts, Rank::Eight));
    round.seats()[0].hands[0].hand.addCard(Card(Suit::Spades, Rank::Three));
    round.seats()[0].hands[0].hand.addCard(Card(Suit::Diamonds, Rank::Seven));
    round.seats()[0].hands[0].doubled = true;
    round.seats()[0].hands[0].bet.mainBet = 200;
    round.seats()[0].bankroll = 800;

    round.dealer().hand.clear();
    round.dealer().hand.addCard(Card(Suit::Clubs, Rank::Ten));
    round.dealer().hand.addCard(Card(Suit::Hearts, Rank::Seven));
    round.dealer().holeCardVisible = true;

    round.evaluatePayouts();

    REQUIRE(round.seats()[0].hands[0].outcome == HandOutcome::Win);
    REQUIRE(round.seats()[0].bankroll == 1200);
}

TEST_CASE("Double down loss", "[integration]") {
    RuleSet rules;
    rules.startingBankroll = 1000;
    RoundState round(rules);
    round.startRound();
    round.placeBet(0, 100);

    round.seats()[0].hands[0].hand.clear();
    round.seats()[0].hands[0].hand.addCard(Card(Suit::Hearts, Rank::Six));
    round.seats()[0].hands[0].hand.addCard(Card(Suit::Spades, Rank::Five));
    round.seats()[0].hands[0].hand.addCard(Card(Suit::Diamonds, Rank::Seven));
    round.seats()[0].hands[0].doubled = true;
    round.seats()[0].hands[0].bet.mainBet = 200;
    round.seats()[0].bankroll = 800;

    round.dealer().hand.clear();
    round.dealer().hand.addCard(Card(Suit::Clubs, Rank::Ten));
    round.dealer().hand.addCard(Card(Suit::Hearts, Rank::Nine));
    round.dealer().holeCardVisible = true;

    round.evaluatePayouts();

    REQUIRE(round.seats()[0].hands[0].outcome == HandOutcome::Lose);
    REQUIRE(round.seats()[0].bankroll == 800);
}

TEST_CASE("Surrender returns half bet", "[integration]") {
    RuleSet rules;
    rules.startingBankroll = 1000;
    rules.surrenderAllowed = true;
    RoundState round(rules);
    round.startRound();
    round.placeBet(0, 100);

    round.seats()[0].hands[0].surrendered = true;
    round.seats()[0].hands[0].finished = true;

    round.dealer().hand.clear();
    round.dealer().hand.addCard(Card(Suit::Clubs, Rank::Ten));
    round.dealer().hand.addCard(Card(Suit::Hearts, Rank::Seven));
    round.dealer().holeCardVisible = true;

    round.evaluatePayouts();

    REQUIRE(round.seats()[0].hands[0].outcome == HandOutcome::Surrender);
    REQUIRE(round.seats()[0].bankroll == 950);
}

TEST_CASE("Dealer hits soft 17 when configured", "[integration]") {
    RuleSet rules;
    rules.dealerHitsSoft17 = true;
    RoundState round(rules);
    round.startRound();
    round.placeBet(0, 100);

    round.dealer().hand.clear();
    round.dealer().hand.addCard(Card(Suit::Hearts, Rank::Ace));
    round.dealer().holeCard = Card(Suit::Clubs, Rank::Six);
    round.dealer().holeCardVisible = false;

    round.evaluateDealerHand();

    REQUIRE(round.dealer().hand.cardCount() >= 3);
}

TEST_CASE("Dealer stands on soft 17 when configured", "[integration]") {
    RuleSet rules;
    rules.dealerHitsSoft17 = false;
    RoundState round(rules);
    round.startRound();
    round.placeBet(0, 100);

    round.dealer().hand.clear();
    round.dealer().hand.addCard(Card(Suit::Hearts, Rank::Ace));
    round.dealer().holeCard = Card(Suit::Clubs, Rank::Six);
    round.dealer().holeCardVisible = false;

    round.evaluateDealerHand();

    REQUIRE(round.dealer().hand.cardCount() == 2);
}

TEST_CASE("Split aces get exactly one card and auto-stand", "[integration]") {
    RuleSet rules;
    rules.startingBankroll = 1000;
    RoundState round(rules);
    round.startRound();
    round.placeBet(0, 100);
    round.advancePhase();

    // Ensure deterministic phase for split testing
    round.phase() = RoundPhase::PlayerTurns;
    round.currentSeatIndex() = 0;
    round.currentHandIndex() = 0;

    round.seats()[0].hands[0].hand.clear();
    round.seats()[0].hands[0].hand.addCard(Card(Suit::Hearts, Rank::Ace));
    round.seats()[0].hands[0].hand.addCard(Card(Suit::Spades, Rank::Ace));

    REQUIRE(round.split(0, 0) == true);
    REQUIRE(round.seats()[0].hands.size() == 2);
    REQUIRE(round.seats()[0].hands[0].finished == true);
    REQUIRE(round.seats()[0].hands[1].finished == true);
    REQUIRE(round.seats()[0].hands[0].hand.cardCount() == 2);
    REQUIRE(round.seats()[0].hands[1].hand.cardCount() == 2);
}

TEST_CASE("Multiple splits up to maxSplitHands", "[integration]") {
    RuleSet rules;
    rules.startingBankroll = 1000;
    rules.maxSplitHands = 4;
    RoundState round(rules);
    round.startRound();
    round.placeBet(0, 100);
    round.advancePhase();

    // Ensure deterministic phase for split testing
    round.phase() = RoundPhase::PlayerTurns;
    round.currentSeatIndex() = 0;
    round.currentHandIndex() = 0;

    round.seats()[0].hands[0].hand.clear();
    round.seats()[0].hands[0].hand.addCard(Card(Suit::Hearts, Rank::Eight));
    round.seats()[0].hands[0].hand.addCard(Card(Suit::Spades, Rank::Eight));

    REQUIRE(round.split(0, 0) == true);
    REQUIRE(round.seats()[0].hands.size() == 2);

    round.seats()[0].hands[0].hand.clear();
    round.seats()[0].hands[0].hand.addCard(Card(Suit::Hearts, Rank::Eight));
    round.seats()[0].hands[0].hand.addCard(Card(Suit::Diamonds, Rank::Eight));

    REQUIRE(round.split(0, 0) == true);
    REQUIRE(round.seats()[0].hands.size() == 3);

    round.seats()[0].hands[0].hand.clear();
    round.seats()[0].hands[0].hand.addCard(Card(Suit::Clubs, Rank::Eight));
    round.seats()[0].hands[0].hand.addCard(Card(Suit::Spades, Rank::Eight));

    REQUIRE(round.split(0, 0) == true);
    REQUIRE(round.seats()[0].hands.size() == 4);

    round.seats()[0].hands[0].hand.clear();
    round.seats()[0].hands[0].hand.addCard(Card(Suit::Hearts, Rank::Eight));
    round.seats()[0].hands[0].hand.addCard(Card(Suit::Spades, Rank::Eight));

    REQUIRE(round.split(0, 0) == false);
    REQUIRE(round.seats()[0].hands.size() == 4);
}
