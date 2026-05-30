#include <catch2/catch_test_macros.hpp>
#include <blackjack/persistence.h>
#include <blackjack/card.h>
#include <blackjack/rules.h>
#include <filesystem>
#include <fstream>

using namespace blackjack;

static std::string getTestSaveDirectory() {
    const char* home = std::getenv("HOME");
    if (!home) home = ".";
    return std::string(home) + "/Library/Application Support/BlackjackGame_Test";
}

static void cleanupTestFiles() {
    try {
        std::string dir = getTestSaveDirectory();
        if (std::filesystem::exists(dir)) {
            std::filesystem::remove_all(dir);
        }
    } catch (...) {}
}

// ============================================================================
// JSON Serialization Round-Trip Tests
// ============================================================================

TEST_CASE("RuleSet JSON round-trip", "[persistence]") {
    RuleSet rules;
    rules.deckCount = 8;
    rules.dealerHitsSoft17 = true;
    rules.surrenderAllowed = false;
    rules.insuranceAllowed = false;
    rules.doubleAfterSplit = false;
    rules.maxSplitHands = 2;
    rules.blackjackPayoutNumerator = 6;
    rules.blackjackPayoutDenominator = 5;
    rules.minBet = 25;
    rules.maxBet = 1000;
    rules.startingBankroll = 500;
    rules.reshuffleThreshold = 0.5f;

    nlohmann::json j = rules;
    RuleSet restored = j.get<RuleSet>();

    REQUIRE(restored.deckCount == 8);
    REQUIRE(restored.dealerHitsSoft17 == true);
    REQUIRE(restored.surrenderAllowed == false);
    REQUIRE(restored.insuranceAllowed == false);
    REQUIRE(restored.doubleAfterSplit == false);
    REQUIRE(restored.maxSplitHands == 2);
    REQUIRE(restored.blackjackPayoutNumerator == 6);
    REQUIRE(restored.blackjackPayoutDenominator == 5);
    REQUIRE(restored.minBet == 25);
    REQUIRE(restored.maxBet == 1000);
    REQUIRE(restored.startingBankroll == 500);
    REQUIRE(restored.reshuffleThreshold == 0.5f);
}

TEST_CASE("PlayerProfile JSON round-trip", "[persistence]") {
    PlayerProfile profile;
    profile.name = "TestPlayer";
    profile.avatarId = 3;
    profile.preferredRules.deckCount = 4;
    profile.masterVolume = 80;
    profile.sfxVolume = 60;
    profile.ambientVolume = 40;
    profile.muted = true;

    nlohmann::json j = profile;
    PlayerProfile restored = j.get<PlayerProfile>();

    REQUIRE(restored.name == "TestPlayer");
    REQUIRE(restored.avatarId == 3);
    REQUIRE(restored.preferredRules.deckCount == 4);
    REQUIRE(restored.masterVolume == 80);
    REQUIRE(restored.sfxVolume == 60);
    REQUIRE(restored.ambientVolume == 40);
    REQUIRE(restored.muted == true);
}

TEST_CASE("PlayerStats JSON round-trip", "[persistence]") {
    PlayerStats stats;
    stats.gamesPlayed = 42;
    stats.gamesWon = 20;
    stats.gamesLost = 15;
    stats.gamesPushed = 7;
    stats.blackjacks = 3;
    stats.biggestWin = 500;
    stats.totalWagered = 10000;
    stats.totalWon = 9500;
    stats.currentStreak = 5;
    stats.bestWinStreak = 8;
    stats.handsPlayed = 120;
    stats.sessionsWithoutBust = 15;

    nlohmann::json j = stats;
    PlayerStats restored = j.get<PlayerStats>();

    REQUIRE(restored.gamesPlayed == 42);
    REQUIRE(restored.gamesWon == 20);
    REQUIRE(restored.gamesLost == 15);
    REQUIRE(restored.gamesPushed == 7);
    REQUIRE(restored.blackjacks == 3);
    REQUIRE(restored.biggestWin == 500);
    REQUIRE(restored.totalWagered == 10000);
    REQUIRE(restored.totalWon == 9500);
    REQUIRE(restored.currentStreak == 5);
    REQUIRE(restored.bestWinStreak == 8);
    REQUIRE(restored.handsPlayed == 120);
    REQUIRE(restored.sessionsWithoutBust == 15);
}

TEST_CASE("Achievement JSON round-trip", "[persistence]") {
    Achievement ach;
    ach.id = AchievementId::TenWinStreak;
    ach.name = "Unstoppable";
    ach.description = "Win 10 games in a row.";
    ach.unlocked = true;

    nlohmann::json j = ach;
    Achievement restored = j.get<Achievement>();

    REQUIRE(restored.id == AchievementId::TenWinStreak);
    REQUIRE(restored.name == "Unstoppable");
    REQUIRE(restored.description == "Win 10 games in a row.");
    REQUIRE(restored.unlocked == true);
}

TEST_CASE("SaveState JSON round-trip", "[persistence]") {
    SaveState state;
    state.profileName = "Alice";
    state.timestamp = 1234567890;
    state.version = 1;
    state.currentBankroll = 850;
    state.rules.deckCount = 4;
    state.phase = RoundPhase::PlayerTurns;
    state.currentSeatIndex = 0;
    state.currentHandIndex = 0;
    state.shoeCards = {"AH", "KS", "2D", "QC"};
    state.shoeRunningCount = 5;
    state.dealerUpCards = {"10H"};
    state.dealerHoleCard = "5S";
    state.dealerHoleVisible = false;

    SaveState::SavedHand hand1;
    hand1.cards = {"8D", "9C"};
    hand1.mainBet = 100;
    hand1.insuranceBet = 0;
    hand1.doubled = false;
    hand1.surrendered = false;
    hand1.isSplit = false;
    hand1.finished = false;
    hand1.outcome = "Pending";
    state.playerHands.push_back(hand1);

    state.stats.gamesPlayed = 10;
    state.achievements.push_back({AchievementId::FirstBlackjack, "First Blackjack", "Get your first natural blackjack.", true});

    nlohmann::json j = state.toJson();
    SaveState restored = SaveState::fromJson(j);

    REQUIRE(restored.profileName == "Alice");
    REQUIRE(restored.timestamp == 1234567890);
    REQUIRE(restored.version == 1);
    REQUIRE(restored.currentBankroll == 850);
    REQUIRE(restored.rules.deckCount == 4);
    REQUIRE(restored.phase == RoundPhase::PlayerTurns);
    REQUIRE(restored.currentSeatIndex == 0);
    REQUIRE(restored.currentHandIndex == 0);
    REQUIRE(restored.shoeCards.size() == 4);
    REQUIRE(restored.shoeCards[0] == "AH");
    REQUIRE(restored.shoeRunningCount == 5);
    REQUIRE(restored.dealerUpCards.size() == 1);
    REQUIRE(restored.dealerHoleCard == "5S");
    REQUIRE(restored.dealerHoleVisible == false);
    REQUIRE(restored.playerHands.size() == 1);
    REQUIRE(restored.playerHands[0].cards.size() == 2);
    REQUIRE(restored.playerHands[0].mainBet == 100);
    REQUIRE(restored.playerHands[0].outcome == "Pending");
    REQUIRE(restored.stats.gamesPlayed == 10);
    REQUIRE(restored.achievements.size() == 1);
    REQUIRE(restored.achievements[0].id == AchievementId::FirstBlackjack);
    REQUIRE(restored.achievements[0].unlocked == true);
}

// ============================================================================
// PersistenceManager Tests
// ============================================================================

TEST_CASE("Create and load profile", "[persistence]") {
    cleanupTestFiles();

    PersistenceManager pm;
    pm.setSaveDirectoryOverride(getTestSaveDirectory());
    REQUIRE(pm.createProfile("TestUser") == true);
    REQUIRE(pm.profileExists("TestUser") == true);

    PlayerProfile profile = pm.loadProfile("TestUser");
    REQUIRE(profile.name == "TestUser");
    REQUIRE(profile.avatarId == 0);

    // Modify and save
    profile.avatarId = 7;
    profile.masterVolume = 50;
    REQUIRE(pm.saveProfile(profile) == true);

    PlayerProfile loaded = pm.loadProfile("TestUser");
    REQUIRE(loaded.avatarId == 7);
    REQUIRE(loaded.masterVolume == 50);

    cleanupTestFiles();
}

TEST_CASE("List profiles", "[persistence]") {
    cleanupTestFiles();

    PersistenceManager pm;
    pm.setSaveDirectoryOverride(getTestSaveDirectory());
    pm.createProfile("Alice");
    pm.createProfile("Bob");
    pm.createProfile("Charlie");

    auto profiles = pm.listProfiles();
    REQUIRE(profiles.size() == 3);

    cleanupTestFiles();
}

TEST_CASE("Delete profile", "[persistence]") {
    cleanupTestFiles();

    PersistenceManager pm;
    pm.setSaveDirectoryOverride(getTestSaveDirectory());
    pm.createProfile("DeleteMe");
    REQUIRE(pm.profileExists("DeleteMe") == true);

    REQUIRE(pm.deleteProfile("DeleteMe") == true);
    REQUIRE(pm.profileExists("DeleteMe") == false);

    cleanupTestFiles();
}

TEST_CASE("Duplicate profile creation fails", "[persistence]") {
    cleanupTestFiles();

    PersistenceManager pm;
    pm.setSaveDirectoryOverride(getTestSaveDirectory());
    REQUIRE(pm.createProfile("Duplicate") == true);
    REQUIRE(pm.createProfile("Duplicate") == false);

    cleanupTestFiles();
}

TEST_CASE("Invalid profile name rejected", "[persistence]") {
    cleanupTestFiles();

    PersistenceManager pm;
    pm.setSaveDirectoryOverride(getTestSaveDirectory());
    REQUIRE(pm.createProfile("") == false);
    REQUIRE(pm.createProfile(std::string(17, 'a')) == false);

    cleanupTestFiles();
}

TEST_CASE("Save and load game state", "[persistence]") {
    cleanupTestFiles();

    PersistenceManager pm;
    pm.setSaveDirectoryOverride(getTestSaveDirectory());
    pm.createProfile("SaveTest");

    SaveState state;
    state.profileName = "SaveTest";
    state.timestamp = 1234567890;
    state.currentBankroll = 1200;
    state.rules.startingBankroll = 1200;
    state.phase = RoundPhase::DealerTurn;
    state.shoeCards = {"AH", "KS", "2D"};
    state.shoeRunningCount = 3;
    state.dealerUpCards = {"10H"};
    state.dealerHoleCard = "6S";
    state.dealerHoleVisible = true;

    SaveState::SavedHand hand;
    hand.cards = {"JD", "QC"};
    hand.mainBet = 100;
    hand.outcome = "Win";
    state.playerHands.push_back(hand);

    state.stats.gamesPlayed = 5;
    state.stats.gamesWon = 3;

    REQUIRE(pm.saveGame("SaveTest", state) == true);
    REQUIRE(pm.hasSave("SaveTest") == true);

    SaveState loaded = pm.loadGame("SaveTest");
    REQUIRE(loaded.profileName == "SaveTest");
    REQUIRE(loaded.currentBankroll == 1200);
    REQUIRE(loaded.phase == RoundPhase::DealerTurn);
    REQUIRE(loaded.shoeCards.size() == 3);
    REQUIRE(loaded.dealerHoleVisible == true);
    REQUIRE(loaded.playerHands.size() == 1);
    REQUIRE(loaded.playerHands[0].outcome == "Win");
    REQUIRE(loaded.stats.gamesPlayed == 5);
    REQUIRE(loaded.stats.gamesWon == 3);

    cleanupTestFiles();
}

TEST_CASE("Delete save", "[persistence]") {
    cleanupTestFiles();

    PersistenceManager pm;
    pm.setSaveDirectoryOverride(getTestSaveDirectory());
    pm.createProfile("DelSave");

    SaveState state;
    state.profileName = "DelSave";
    pm.saveGame("DelSave", state);
    REQUIRE(pm.hasSave("DelSave") == true);

    REQUIRE(pm.deleteSave("DelSave") == true);
    REQUIRE(pm.hasSave("DelSave") == false);

    cleanupTestFiles();
}

TEST_CASE("Load non-existent save returns empty state", "[persistence]") {
    cleanupTestFiles();

    PersistenceManager pm;
    pm.setSaveDirectoryOverride(getTestSaveDirectory());
    SaveState state = pm.loadGame("NonExistent");
    REQUIRE(state.profileName == "NonExistent");
    REQUIRE(state.version == 1);
    REQUIRE(state.currentBankroll == 0);
    REQUIRE(state.playerHands.empty() == true);

    cleanupTestFiles();
}

// ============================================================================
// GameSession Tests
// ============================================================================

TEST_CASE("GameSession newGame initializes correctly", "[persistence]") {
    cleanupTestFiles();

    GameSession session;
    session.setSaveDirectoryOverride(getTestSaveDirectory());
    RuleSet rules;
    rules.startingBankroll = 2000;
    session.newGame("NewPlayer", rules);

    REQUIRE(session.round().seats()[0].bankroll == 2000);
    REQUIRE(session.stats().gamesPlayed == 0);
    REQUIRE(session.achievements().size() == 6);

    cleanupTestFiles();
}

TEST_CASE("GameSession save and restore round state", "[persistence]") {
    cleanupTestFiles();

    GameSession session;
    session.setSaveDirectoryOverride(getTestSaveDirectory());
    RuleSet rules;
    rules.startingBankroll = 1000;
    session.newGame("RestoreTest", rules);

    // Set up some round state
    session.round().startRound();
    session.round().placeBet(0, 100);
    session.round().seats()[0].hands[0].hand.addCard(Card(Suit::Hearts, Rank::Ten));
    session.round().seats()[0].hands[0].hand.addCard(Card(Suit::Spades, Rank::Eight));
    session.round().dealer().hand.addCard(Card(Suit::Clubs, Rank::Nine));
    session.round().dealer().holeCard = Card(Suit::Diamonds, Rank::Seven);
    session.round().phase() = RoundPhase::PlayerTurns;
    session.round().currentSeatIndex() = 0;
    session.round().currentHandIndex() = 0;
    session.round().shoe().setCards({Card(Suit::Hearts, Rank::Ace), Card(Suit::Spades, Rank::King)});
    session.round().shoe().setRunningCount(2);

    REQUIRE(session.saveGame() == true);

    GameSession loaded;
    loaded.setSaveDirectoryOverride(getTestSaveDirectory());
    REQUIRE(loaded.loadGame("RestoreTest") == true);

    REQUIRE(loaded.round().seats()[0].bankroll == 900);
    REQUIRE(loaded.round().seats()[0].hands[0].hand.cardCount() == 2);
    REQUIRE(loaded.round().dealer().hand.cardCount() == 1);
    REQUIRE(loaded.round().dealer().holeCard == Card(Suit::Diamonds, Rank::Seven));
    REQUIRE(loaded.round().phase() == RoundPhase::PlayerTurns);
    REQUIRE(loaded.round().currentSeatIndex() == 0);
    REQUIRE(loaded.round().currentHandIndex() == 0);
    REQUIRE(loaded.round().shoe().remaining() == 2);
    REQUIRE(loaded.round().shoe().getRunningCount() == 2);

    cleanupTestFiles();
}

TEST_CASE("GameSession recordHandResult updates stats", "[persistence]") {
    cleanupTestFiles();

    GameSession session;
    session.setSaveDirectoryOverride(getTestSaveDirectory());
    RuleSet rules;
    rules.startingBankroll = 1000;
    session.newGame("StatsTest", rules);
    session.round().startRound();
    session.round().placeBet(0, 100);

    session.recordHandResult(HandOutcome::Win, 100, 200);
    REQUIRE(session.stats().gamesWon == 1);
    REQUIRE(session.stats().handsPlayed == 1);
    REQUIRE(session.stats().currentStreak == 1);
    REQUIRE(session.stats().totalWagered == 100);
    REQUIRE(session.stats().totalWon == 100);
    REQUIRE(session.stats().biggestWin == 100);

    session.recordHandResult(HandOutcome::Win, 100, 250);
    REQUIRE(session.stats().gamesWon == 2);
    REQUIRE(session.stats().currentStreak == 2);
    REQUIRE(session.stats().bestWinStreak == 2);
    REQUIRE(session.stats().totalWon == 250);
    REQUIRE(session.stats().biggestWin == 150);

    session.recordHandResult(HandOutcome::Lose, 100, 0);
    REQUIRE(session.stats().gamesLost == 1);
    REQUIRE(session.stats().currentStreak == -1);

    session.recordHandResult(HandOutcome::Push, 100, 100);
    REQUIRE(session.stats().gamesPushed == 1);
    REQUIRE(session.stats().currentStreak == 0);

    // After push (streak=0), bust should set streak to -1, not -2
    session.recordHandResult(HandOutcome::Bust, 100, 0);
    REQUIRE(session.stats().gamesLost == 2);
    REQUIRE(session.stats().currentStreak == -1);
    REQUIRE(session.stats().sessionsWithoutBust == 0);

    // totalWon should be net profit: 100 + 150 - 100 + 0 - 100 = 50
    REQUIRE(session.stats().totalWon == 50);

    cleanupTestFiles();
}

TEST_CASE("GameSession checkAchievements", "[persistence]") {
    cleanupTestFiles();

    GameSession session;
    session.setSaveDirectoryOverride(getTestSaveDirectory());
    RuleSet rules;
    rules.startingBankroll = 1000;
    session.newGame("AchTest", rules);
    session.round().startRound();
    session.round().placeBet(0, 100);

    // First blackjack — recordHandResult for stats, recordBlackjack for session count
    session.recordHandResult(HandOutcome::Blackjack, 100, 250);
    session.recordBlackjack();
    session.checkAchievements();
    REQUIRE(session.achievements()[0].unlocked == true); // FirstBlackjack

    // Bankroll achievement
    session.round().seats()[0].bankroll = 1000;
    session.checkAchievements();
    REQUIRE(session.achievements()[1].unlocked == true); // ThousandBankroll

    // 5 blackjacks in session (4 more session-only records)
    for (int i = 0; i < 4; ++i) session.recordBlackjack();
    session.checkAchievements();
    REQUIRE(session.achievements()[2].unlocked == true); // FiveBlackjacksSession

    // Hundred hands
    for (int i = 0; i < 100; ++i) {
        session.recordHandResult(HandOutcome::Win, 10, 20);
    }
    session.checkAchievements();
    REQUIRE(session.achievements()[4].unlocked == true); // HundredHands

    cleanupTestFiles();
}

TEST_CASE("GameSession load non-existent save returns false", "[persistence]") {
    cleanupTestFiles();

    GameSession session;
    session.setSaveDirectoryOverride(getTestSaveDirectory());
    REQUIRE(session.loadGame("NoSuchSave") == false);

    cleanupTestFiles();
}

TEST_CASE("Multiple profiles do not interfere", "[persistence]") {
    cleanupTestFiles();

    PersistenceManager pm;
    pm.setSaveDirectoryOverride(getTestSaveDirectory());
    pm.createProfile("PlayerA");
    pm.createProfile("PlayerB");

    PlayerProfile profileA = pm.loadProfile("PlayerA");
    profileA.avatarId = 1;
    pm.saveProfile(profileA);

    PlayerProfile profileB = pm.loadProfile("PlayerB");
    profileB.avatarId = 2;
    pm.saveProfile(profileB);

    PlayerProfile loadedA = pm.loadProfile("PlayerA");
    PlayerProfile loadedB = pm.loadProfile("PlayerB");

    REQUIRE(loadedA.avatarId == 1);
    REQUIRE(loadedB.avatarId == 2);

    // Saves should also be independent
    SaveState stateA;
    stateA.profileName = "PlayerA";
    stateA.currentBankroll = 500;
    pm.saveGame("PlayerA", stateA);

    SaveState stateB;
    stateB.profileName = "PlayerB";
    stateB.currentBankroll = 1500;
    pm.saveGame("PlayerB", stateB);

    SaveState loadedSaveA = pm.loadGame("PlayerA");
    SaveState loadedSaveB = pm.loadGame("PlayerB");

    REQUIRE(loadedSaveA.currentBankroll == 500);
    REQUIRE(loadedSaveB.currentBankroll == 1500);

    cleanupTestFiles();
}
