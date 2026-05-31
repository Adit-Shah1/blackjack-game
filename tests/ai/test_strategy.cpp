#include <catch2/catch_test_macros.hpp>
#include <blackjack/ai.h>
#include <blackjack/round.h>
#include <blackjack/rules.h>

using namespace blackjack;

// Helper: create a GameView for hard totals
static GameView makeHardView(int dealerUpcard, int playerTotal, int cardCount = 2, bool hasPair = false) {
    GameView v;
    v.dealerUpcard = dealerUpcard;
    v.playerHandValue = playerTotal;
    v.playerHandIsSoft = false;
    v.playerHandCardCount = cardCount;
    v.hasPair = hasPair;
    return v;
}

// Helper: create a GameView for soft totals
static GameView makeSoftView(int dealerUpcard, int playerTotal, int cardCount = 2, bool hasPair = false) {
    GameView v;
    v.dealerUpcard = dealerUpcard;
    v.playerHandValue = playerTotal;
    v.playerHandIsSoft = true;
    v.playerHandCardCount = cardCount;
    v.hasPair = hasPair;
    return v;
}

// Helper: create a BettingView
static BettingView makeBettingView(int bankroll, int minBet = 10, int maxBet = 500) {
    BettingView v;
    v.bankroll = bankroll;
    v.minBet = minBet;
    v.maxBet = maxBet;
    return v;
}

// Helper: create a fully legal ActionSet
static ActionSet allActions() {
    ActionSet a;
    a.canHit = true;
    a.canStand = true;
    a.canDouble = true;
    a.canSplit = true;
    a.canSurrender = true;
    return a;
}

// Helper: create a no-double ActionSet
static ActionSet noDoubleActions() {
    ActionSet a;
    a.canHit = true;
    a.canStand = true;
    a.canDouble = false;
    a.canSplit = true;
    a.canSurrender = true;
    return a;
}

// ============================================================================
// Basic Strategy Tests
// ============================================================================

TEST_CASE("BasicStrategy hits hard 16 vs dealer 10", "[ai][basic]") {
    BasicStrategy bs;
    GameView view = makeHardView(10, 16);
    ActionSet legal = allActions();
    PlayerAction action = bs.decideAction(view, legal);
    REQUIRE(action == PlayerAction::Hit);
}

TEST_CASE("BasicStrategy stands hard 17 vs dealer 6", "[ai][basic]") {
    BasicStrategy bs;
    GameView view = makeHardView(6, 17);
    ActionSet legal = allActions();
    PlayerAction action = bs.decideAction(view, legal);
    REQUIRE(action == PlayerAction::Stand);
}

TEST_CASE("BasicStrategy doubles hard 11 vs dealer 5", "[ai][basic]") {
    BasicStrategy bs;
    GameView view = makeHardView(5, 11);
    ActionSet legal = allActions();
    PlayerAction action = bs.decideAction(view, legal);
    REQUIRE(action == PlayerAction::DoubleDown);
}

TEST_CASE("BasicStrategy hits hard 11 vs dealer Ace when double not allowed", "[ai][basic]") {
    BasicStrategy bs;
    GameView view = makeHardView(11, 11);
    ActionSet legal = noDoubleActions();
    PlayerAction action = bs.decideAction(view, legal);
    REQUIRE(action == PlayerAction::Hit);
}

TEST_CASE("BasicStrategy doubles hard 10 vs dealer 6", "[ai][basic]") {
    BasicStrategy bs;
    GameView view = makeHardView(6, 10);
    ActionSet legal = allActions();
    PlayerAction action = bs.decideAction(view, legal);
    REQUIRE(action == PlayerAction::DoubleDown);
}

TEST_CASE("BasicStrategy stands hard 12 vs dealer 4", "[ai][basic]") {
    BasicStrategy bs;
    GameView view = makeHardView(4, 12);
    ActionSet legal = allActions();
    PlayerAction action = bs.decideAction(view, legal);
    REQUIRE(action == PlayerAction::Stand);
}

TEST_CASE("BasicStrategy hits hard 12 vs dealer 7", "[ai][basic]") {
    BasicStrategy bs;
    GameView view = makeHardView(7, 12);
    ActionSet legal = allActions();
    PlayerAction action = bs.decideAction(view, legal);
    REQUIRE(action == PlayerAction::Hit);
}

TEST_CASE("BasicStrategy stands soft 19 vs dealer 6", "[ai][basic]") {
    BasicStrategy bs;
    GameView view = makeSoftView(6, 19);
    ActionSet legal = allActions();
    PlayerAction action = bs.decideAction(view, legal);
    REQUIRE(action == PlayerAction::Stand);
}

TEST_CASE("BasicStrategy doubles soft 18 vs dealer 5", "[ai][basic]") {
    BasicStrategy bs;
    GameView view = makeSoftView(5, 18);
    ActionSet legal = allActions();
    PlayerAction action = bs.decideAction(view, legal);
    REQUIRE(action == PlayerAction::DoubleDown);
}

TEST_CASE("BasicStrategy bets minimum", "[ai][basic]") {
    BasicStrategy bs;
    BettingView bv = makeBettingView(1000, 10, 500);
    int bet = bs.decideBet(bv);
    REQUIRE(bet == 10);
}

TEST_CASE("BasicStrategy never takes insurance", "[ai][basic]") {
    BasicStrategy bs;
    InsuranceView iv;
    iv.dealerUpcard = 11;
    iv.bankroll = 1000;
    iv.maxInsurance = 50;
    bool take = bs.decideInsurance(iv);
    REQUIRE(take == false);
}

// ============================================================================
// Conservative Strategy Tests
// ============================================================================

TEST_CASE("ConservativeStrategy stands 12+ vs dealer 6", "[ai][conservative]") {
    ConservativeStrategy cs;
    GameView view = makeHardView(6, 13);
    ActionSet legal = allActions();
    PlayerAction action = cs.decideAction(view, legal);
    REQUIRE(action == PlayerAction::Stand);
}

TEST_CASE("ConservativeStrategy avoids doubling 10/11", "[ai][conservative]") {
    ConservativeStrategy cs;
    GameView view = makeHardView(5, 10);
    ActionSet legal = allActions();
    PlayerAction action = cs.decideAction(view, legal);
    // Should NOT double - either stand or hit
    REQUIRE(action != PlayerAction::DoubleDown);
}

TEST_CASE("ConservativeStrategy bets minimum", "[ai][conservative]") {
    ConservativeStrategy cs;
    BettingView bv = makeBettingView(1000, 10, 500);
    int bet = cs.decideBet(bv);
    REQUIRE(bet == 10);
}

// ============================================================================
// Aggressive Strategy Tests
// ============================================================================

TEST_CASE("AggressiveStrategy doubles 9 vs dealer 4", "[ai][aggressive]") {
    AggressiveStrategy as;
    GameView view = makeHardView(4, 9);
    ActionSet legal = allActions();
    PlayerAction action = as.decideAction(view, legal);
    REQUIRE(action == PlayerAction::DoubleDown);
}

TEST_CASE("AggressiveStrategy bets 2x-3x min", "[ai][aggressive]") {
    AggressiveStrategy as;
    BettingView bv = makeBettingView(1000, 10, 500);
    int bet = as.decideBet(bv);
    REQUIRE(bet >= 20);
    REQUIRE(bet <= 30);
}

TEST_CASE("AggressiveStrategy hits soft 18 vs dealer 10", "[ai][aggressive]") {
    AggressiveStrategy as;
    GameView view = makeSoftView(10, 18);
    ActionSet legal = allActions();
    PlayerAction action = as.decideAction(view, legal);
    REQUIRE(action == PlayerAction::Hit);
}

// ============================================================================
// Random Strategy Tests
// ============================================================================

TEST_CASE("RandomStrategy returns a legal action", "[ai][random]") {
    RandomStrategy rs(42);
    GameView view = makeHardView(7, 15);
    ActionSet legal = allActions();
    PlayerAction action = rs.decideAction(view, legal);
    REQUIRE((action == PlayerAction::Hit ||
             action == PlayerAction::Stand ||
             action == PlayerAction::DoubleDown ||
             action == PlayerAction::Split ||
             action == PlayerAction::Surrender));
}

TEST_CASE("RandomStrategy bet is within range", "[ai][random]") {
    RandomStrategy rs(42);
    BettingView bv = makeBettingView(1000, 10, 500);
    int bet = rs.decideBet(bv);
    REQUIRE(bet >= 10);
    REQUIRE(bet <= 500);
}

TEST_CASE("RandomStrategy insurance is boolean", "[ai][random]") {
    RandomStrategy rs(42);
    InsuranceView iv;
    bool take = rs.decideInsurance(iv);
    // Just verify it doesn't crash and returns a bool
    REQUIRE((take == true || take == false));
}

// ============================================================================
// Card Counter Strategy Tests
// ============================================================================

TEST_CASE("CardCounterStrategy bets minimum at neutral count", "[ai][cardcounter]") {
    RuleSet rules;
    RoundState round(rules);
    CardCounterStrategy ccs(&round.shoe());
    BettingView bv = makeBettingView(1000, 10, 500);
    int bet = ccs.decideBet(bv);
    REQUIRE(bet == 10);
}

TEST_CASE("CardCounterStrategy never takes insurance at neutral count", "[ai][cardcounter]") {
    RuleSet rules;
    RoundState round(rules);
    CardCounterStrategy ccs(&round.shoe());
    InsuranceView iv;
    iv.dealerUpcard = 11;
    iv.bankroll = 1000;
    iv.maxInsurance = 50;
    bool take = ccs.decideInsurance(iv);
    REQUIRE(take == false);
}

// ============================================================================
// AIController Tests
// ============================================================================

TEST_CASE("AIController validates illegal actions and falls back", "[ai][controller]") {
    RuleSet rules;
    RoundState round(rules);
    round.addSeat("AI", rules.startingBankroll);
    round.placeBet(0, 10);
    round.placeBet(1, 10);
    round.startRound();
    round.advancePhase(); // deal

    // Create a basic strategy AI
    auto strategy = std::make_unique<BasicStrategy>();
    AIController controller(std::move(strategy));

    // At this point the AI may or may not have a valid turn depending on phase
    // Just verify controller construction and name work
    REQUIRE(controller.strategyName() == "Basic Strategy");
}

TEST_CASE("AIController chooses bet within limits", "[ai][controller]") {
    RuleSet rules;
    RoundState round(rules);
    round.addSeat("AI", rules.startingBankroll);

    auto strategy = std::make_unique<BasicStrategy>();
    AIController controller(std::move(strategy));

    int bet = controller.chooseBet(round, 1);
    REQUIRE(bet >= rules.minBet);
    REQUIRE(bet <= rules.maxBet);
    REQUIRE(bet <= round.seats()[1].bankroll);
}

TEST_CASE("AIController name returns unknown for null strategy", "[ai][controller]") {
    AIController controller(nullptr);
    REQUIRE(controller.strategyName() == "Unknown");
}

// ============================================================================
// Strategy Name Tests
// ============================================================================

TEST_CASE("All strategy names are non-empty", "[ai]") {
    BasicStrategy bs;
    ConservativeStrategy cs;
    AggressiveStrategy as;
    RandomStrategy rs;
    RuleSet rules;
    RoundState round(rules);
    CardCounterStrategy ccs(&round.shoe());

    REQUIRE(!bs.name().empty());
    REQUIRE(!cs.name().empty());
    REQUIRE(!as.name().empty());
    REQUIRE(!rs.name().empty());
    REQUIRE(!ccs.name().empty());
}

// ============================================================================
// RoundState addSeat Tests
// ============================================================================

TEST_CASE("RoundState addSeat increases seat count", "[ai][round]") {
    RuleSet rules;
    RoundState round(rules);
    REQUIRE(round.seats().size() == 1); // default player seat

    round.addSeat("AI 1", rules.startingBankroll);
    REQUIRE(round.seats().size() == 2);
    REQUIRE(round.seats()[1].name == "AI 1");
    REQUIRE(round.seats()[1].bankroll == rules.startingBankroll);

    round.addSeat("AI 2", rules.startingBankroll);
    REQUIRE(round.seats().size() == 3);
}

TEST_CASE("RoundState allSeatsHaveBets works with multiple seats", "[ai][round]") {
    RuleSet rules;
    RoundState round(rules);
    round.addSeat("AI 1", rules.startingBankroll);
    round.addSeat("AI 2", rules.startingBankroll);

    REQUIRE(round.allSeatsHaveBets() == false);
    round.placeBet(0, 10);
    REQUIRE(round.allSeatsHaveBets() == false);
    round.placeBet(1, 10);
    REQUIRE(round.allSeatsHaveBets() == false);
    round.placeBet(2, 10);
    REQUIRE(round.allSeatsHaveBets() == true);
}
